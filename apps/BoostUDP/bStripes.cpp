/*------------------------------------------------------------------
 * bStripes.cpp
 *
 * Boost based packet handler to subdivide flows into Stripes and 
 * apply SMPTE FEC to packets
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <vector>
#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <map>
#include <filesystem>
#include <cstdlib>
#include <utility> // for std::move


#include <boost/asio.hpp>
#include <boost/process/v1.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

#include "logger.h"
#include "bStriperConfig.h"
#include "bStripes.h"
#include "threadManager.h"
#include "watchdog.h"


using namespace boost::interprocess;
namespace bp = boost::process;

using namespace my_logger;

 // 1. Define types using the shared memory allocator hierarchy
typedef boost::interprocess::managed_shared_memory::segment_manager     SegmentManager;
typedef allocator<uint8_t, SegmentManager>                              ByteAllocator;
typedef allocator<uint8_t, SegmentManager>                              ShmemByteAllocator;
typedef allocator<void, SegmentManager>                                 VoidAllocator; // placeholder

// Allocator for StripeDequeMessage (defined below) will be declared after message is known.
// We'll define the message's internal vector allocator alias below once message is declared.

struct StripeShmDequeMessage; // forward declare so we can declare the deque allocator type
typedef boost::interprocess::allocator<int, SegmentManager>                   StripeSharedDataAllocator;
typedef boost::interprocess::allocator<StripeShmDequeMessage, SegmentManager>    ShStripeMsgAllocator;
typedef boost::interprocess::deque<StripeShmDequeMessage, ShStripeMsgAllocator>  StripeShmDeque;
typedef boost::interprocess::vector<uint8_t, allocator<uint8_t, SegmentManager>> ShmVector;

struct ShmPacketHeader {
    PacketHeaderType htype;
    ShmVector header;

    // Allocator-aware constructors
    explicit ShmPacketHeader(const allocator<uint8_t, SegmentManager>& alloc) 
        : htype(PacketHeaderType::NOTSET), header(alloc)
    { }

    ShmPacketHeader(PacketHeaderType htype_, const allocator<uint8_t, SegmentManager>& alloc)
        : htype(htype_), header(alloc) {
    }    
};

struct StripeShmDequeMessage {
    DeqMsgType msg_type;
    int msg_length;

    // Use interprocess vector for payload so it can live inside shared memory
    ShmVector data;

    typedef boost::interprocess::vector<ShmPacketHeader, allocator<ShmPacketHeader, SegmentManager>> ShmHeaderVector;
    ShmHeaderVector headers;


    // Allocator-aware constructors
    explicit StripeShmDequeMessage(const allocator<uint8_t, SegmentManager>& alloc)
        : msg_type(DeqMsgType::NOTSET), msg_length(0), data(alloc), headers(alloc) {
    }

    StripeShmDequeMessage(DeqMsgType t, int len, const allocator<uint8_t, SegmentManager>& alloc)
        : msg_type(t), msg_length(len), data(alloc), headers(alloc) {
    }
};
    
static std::ostream& operator<<(std::ostream& os, const StripeShmDequeMessage& msg) {
    os << "StripeDequeMessage{msg_type="
        << std::string(magic_enum::enum_name(msg.msg_type))
        << ", msg_length=" << msg.msg_length;
    if (msg.msg_type == DeqMsgType::PACKET) {
        os << ", headers=[";
        for (auto it = msg.headers.begin(); it != msg.headers.end(); ++it) {
            os << std::string(magic_enum::enum_name(it->htype));
            if (std::next(it) != msg.headers.end()) os << ", ";
        }
        os << "]";
    }
    os << ", data=";
    int msize = msg.data.size();
    if (msg.data.size() != static_cast<size_t>(msg.msg_length)) {
        os << "(Warning: msg_length does not match data size!"
            << " size=" << msg.data.size()
            << " length=" << msg.msg_length
            << ") ";
    }
    if (msg.data.size() != 0) {
        auto count = 10;
        switch (msg.msg_type) {
        case DeqMsgType::MESSAGE:
            os << " [";
            for (auto it = msg.data.begin(); it != msg.data.end(); ++it) {
                os << static_cast<char>(*it);
            }
            os << "]";
            break;
        case DeqMsgType::PACKET:
            os << " Packet: [";
            for (auto it = msg.data.begin(); it != msg.data.end() && count; ++it, --count) {
                os << std::to_string(static_cast<int>(*it));
                if (std::next(it) != msg.data.end() || count != 1) os << ", ";
            }
            os << "]";

            break;
        case DeqMsgType::EXIT_PROCESS:
            break;
        }
    }
    else {
        os << "[]";
    }
    os << "}";
    return os;
}

// Data stucture to share data between Main process 
// (StripesMangager) and indidividual stripe processes 
// (StripeProcessManager) 
struct StripeSharedData {
    StripeShmDeque stripe_deque;
    boost::interprocess::interprocess_condition cond_nonempty;
    boost::interprocess::interprocess_mutex mutex;

    StripeSharedData(StripeSharedDataAllocator alloc_inst) : stripe_deque(alloc_inst) {}

    // Non-copyable
    StripeSharedData(const StripeSharedData&) = delete;
    StripeSharedData& operator=(const StripeSharedData&) = delete;

    // Movable
    StripeSharedData(StripeSharedData&& other) = delete;
    StripeSharedData& operator=(StripeSharedData&& other) = delete;

    ~StripeSharedData() {
        // Destructor logic if needed
    }
};

// Class to manage an individual stripe process
class StripeProcessManager {
public:
    std::string shm_name;
    StriperModeE mode;
    size_t shm_size;
    std::unique_ptr<boost::interprocess::managed_shared_memory> segment;
    //StripeShmDeque* stripe_deque;
    //boost::interprocess::interprocess_condition cond_nonempty;
    //boost::interprocess::interprocess_mutex mutex;
    bp::v1::child stripe_process;
    StripeSharedData* shared_data;

    StripeProcessManager(const char* shm_name_, size_t shm_size_, StriperModeE mode_)
        : shm_name(shm_name_)
        , shm_size(shm_size_)
        , mode(mode_)
    {
        std::string SharedDataStr = "SharedData" + std::string(magic_enum::enum_name(mode));        
        shared_memory_object::remove(shm_name.c_str());
		LOG(LoggerVerbosity::INFO, "Creating shared memory segment: " + shm_name + " of size: " + std::to_string(shm_size) + " SharedDataStr=" + SharedDataStr);
        segment = std::make_unique<boost::interprocess::managed_shared_memory>(
            boost::interprocess::create_only, shm_name.c_str(), shm_size);

        shared_data = segment->construct<StripeSharedData>(SharedDataStr.c_str())(segment->get_segment_manager());
        SendMessage("Hello");
    }

    // Non-copyable
    StripeProcessManager(const StripeProcessManager&) = delete;
    StripeProcessManager& operator=(const StripeProcessManager&) = delete;

    // Movable
    StripeProcessManager(StripeProcessManager&& other) noexcept
        : shm_name(std::move(other.shm_name))
        , shm_size(other.shm_size)
        , segment(std::move(other.segment))
        //, stripe_deque(other.stripe_deque)
        , stripe_process(std::move(other.stripe_process))
        , shared_data(other.shared_data)
    {
        other.shared_data = nullptr;
        other.shm_size = 0;
    }

    StripeProcessManager& operator=(StripeProcessManager&& other) noexcept {
        if (this != &other) {
            shm_name = std::move(other.shm_name);
            shm_size = other.shm_size;
            segment = std::move(other.segment);
            //stripe_deque = other.stripe_deque;
            stripe_process = std::move(other.stripe_process);
            shared_data = other.shared_data;

            //other.stripe_deque = nullptr;
            other.shared_data = nullptr;
            other.shm_size = 0;
        }
        return *this;
    }

    void startStripeProcess(const std::string& process_invocation_str) {
        stripe_process = bp::v1::child(process_invocation_str);
    }

    void SendMessage(std::string message) {
        // Create a message using the shared-memory byte allocator
        allocator<uint8_t, SegmentManager> byteAlloc(segment->get_segment_manager());
        StripeShmDequeMessage msg(byteAlloc);
        std::string send_msg = "(" + shm_name + "): " + message;
        msg.msg_type = DeqMsgType::MESSAGE;
        msg.msg_length = send_msg.size();
        //msg.data.emplace_back(message.begin(), message.end());
        for (uint8_t i = 0; i < msg.msg_length; ++i) {
            msg.data.push_back(send_msg[i]);
        }
        shared_data->mutex.lock();
        shared_data->stripe_deque.push_back(msg);
        shared_data->mutex.unlock();
        shared_data->cond_nonempty.notify_one();
    }


    void SendPacket(PacketHeaders& headers_, const std::vector<uint8_t>& data_, std::size_t length_) {
        allocator<uint8_t, SegmentManager> byteAlloc(segment->get_segment_manager());
        StripeShmDequeMessage msg(byteAlloc);
        msg.msg_type = DeqMsgType::PACKET;

        for (const auto& hdr : headers_.headers) {
            ShmPacketHeader shmHdr(byteAlloc);
            shmHdr.htype = hdr->header_type;
            auto hdrv = hdr->serialize();
            for (auto& b : hdrv) {
                shmHdr.header.emplace_back(b);
            }
            msg.headers.emplace_back(shmHdr);
        }

        msg.msg_length = length_;
        //msg.data = std::move(data_.begin(), data_.begin() + length_);
        msg.data.assign(std::make_move_iterator(data_.begin()), std::make_move_iterator(data_.begin() + length_));
        std::stringstream ss;
        ss << msg;
        LOG(LoggerVerbosity::INFO, "Created SHM Packet:" + ss.str());

        // Place Packet on Shared Deque
        shared_data->mutex.lock();
        shared_data->stripe_deque.push_back(msg);
        shared_data->mutex.unlock();
        shared_data->cond_nonempty.notify_one();
    }
    void ReceivePacket(UdpStriperPortE port_mode, PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length) {
		LOG(LoggerVerbosity::INFO, "SPM:ReceivePacket called for shm_name=" + shm_name
            + " port_mode=" + std::string(magic_enum::enum_name(port_mode)) 
            + " length=" + std::to_string(length));
    }

    void SendExit() {
        // Create a message using the shared-memory byte allocator
        allocator<uint8_t, SegmentManager> byteAlloc(segment->get_segment_manager());
        StripeShmDequeMessage msg(byteAlloc);
        msg.msg_type = DeqMsgType::EXIT_PROCESS;
        msg.msg_length = 0;
        shared_data->mutex.lock();
        shared_data->stripe_deque.push_back(msg);
        shared_data->mutex.unlock();
        shared_data->cond_nonempty.notify_one();
    }

    ~StripeProcessManager() {
        if (stripe_process.running()) {
            stripe_process.terminate();
            stripe_process.wait();
        }
        if (shared_data) {
            //segment->destroy<StripeShmDeque>("SharedData");
        }
        shared_memory_object::remove(shm_name.c_str());
    }
};

class StripesManagerImpl {
private:
    std::vector<StripeProcessManager> stripe_processors;
    std::vector<StripeProcessManager>::iterator stripe_it;
    uint32_t current_stripe = 0;
    AllStriperConfig* striperConfig = nullptr;
    StriperModeE mode;
    std::string process_path;
    std::string process_args;

public:
    StripesManagerImpl(AllStriperConfig* striper_config_) 
        : striperConfig(striper_config_) 
        , stripe_it(stripe_processors.begin())
    {}

    int InitializeInternal(StriperModeE mode_, std::string path_, std::string args_) {
        mode = mode_;
        process_path = path_;
        process_args = args_;
		std::string mode_str = std::string(magic_enum::enum_name(mode));
        //std::string mode_str = (mode == StriperModeE::RECEIVER) ? "RECEIVER" : "TRANSMITTER";

        if (mode == StriperModeE::RECEIVER) {
            process_args += " --rx_striper";
        }
        else if (mode == StriperModeE::TRANSMITTER) {
            process_args += " --tx_striper";
        }

        for (int sid = 0; sid < striperConfig->schedulerCfg.MaxNumberStripes; ++sid) {
            std::string process_name = "Stripe-" + std::to_string(sid) +"-"+mode_str;
            std::string stripe_args = process_args + " --stripe_process " + process_name + " --logfile " + process_name + ".log";

            LOG(LoggerVerbosity::INFO, "Starting Stripe Process: " + process_name + " at path: " + process_path + " with args: " + process_args);

            try {
                StripeProcessManager spm(process_name.c_str(), 65536, mode); // 64KB shared memory for each stripe
                spm.startStripeProcess(process_path + stripe_args);
                stripe_processors.emplace_back(std::move(spm));
            }
            catch (const std::exception& e) {
                LOG(LoggerVerbosity::CRITICAL, "Failed to start Stripe Process: " + process_name + ". Error: " + e.what());
                return 1;
            }

        }
        LOG(LoggerVerbosity::INFO, "All child processes spawned and running in parallel...");
        return 0;
    }

    void SendMessage(std::string msg, int stripe_num=-1) {
        if (stripe_num == -1) {
            for (auto& sp : stripe_processors) {
                sp.SendMessage(msg);
            }
        } else {
            if (stripe_num >= stripe_processors.size()) {
                LOG(LoggerVerbosity::ERR, "Trying to send to invalid stripe, num=" + std::to_string(stripe_num));
                return;
            }
            stripe_processors[stripe_num].SendMessage(msg);
        }
    }

    void SendPacket(PacketHeaders& pkt_, const std::vector<uint8_t>& data_, std::size_t length_) {
        // Select Stripe to send packet too

        if (stripe_processors.size() == 0) {
            LOG(LoggerVerbosity::ERR, "No stripe processors instantiated!!!");
            return;
        }
        if (current_stripe >= stripe_processors.size()) current_stripe = 0;
        stripe_processors[current_stripe].SendPacket(pkt_, data_, length_);
        current_stripe++;
    }

    void ReceivePacket(UdpStriperPortE port_mode, PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length) {
        // For now, just receive from the first stripe processor
        if (stripe_processors.size() == 0) {
            LOG(LoggerVerbosity::ERR, "No stripe processors instantiated!!!");
            return;
        }
		// Outer UDP header is 2nd header, stripe # contained in UDP header, so we can use that to determine which stripe to receive from
        if (headers.headers.size() < 2) {
            LOG(LoggerVerbosity::ERR, "Packet headers do not contain enough headers to determine stripe number.");
            return;
		}
        std::shared_ptr<PacketHeaderUDP> udpHdr = std::dynamic_pointer_cast<PacketHeaderUDP>(headers.headers[1]);
		if (!udpHdr) {
            LOG(LoggerVerbosity::ERR, "Second header is not a UDP header, cannot determine stripe number.");
            return;
        }
        int stripe_num = (udpHdr->srcPort - striperConfig->stripeCfg.StartUdpSrcPortNumber) >> 3;
        if (stripe_num < 0 || stripe_num >= stripe_processors.size()) {
            LOG(LoggerVerbosity::ERR, "Invalid stripe number derived from UDP header: stripe_num=" 
                + std::to_string(stripe_num)
				+ " srcPort=" + std::to_string(udpHdr->srcPort)
            );
            return;
		}
        // Check Dest port
        uint16_t pm = udpHdr->dstPort - striperConfig->stripeCfg.StartUdpDstPortNumber;
        if (pm == 0 && port_mode != UdpStriperPortE::STRIPER_PORT_DATA) {
            LOG(LoggerVerbosity::ERR, "Mistmatch between Destination Port and port_mode: Destination Port=" + std::to_string(udpHdr->dstPort)
                + " pm=" + std::to_string(pm)
                + " port_mode=" + std::string(magic_enum::enum_name(port_mode))
                );
            return;
        } else if (pm == 2 && port_mode != UdpStriperPortE::STRIPER_PORT_FEC_COL) {
            LOG(LoggerVerbosity::ERR, "Mistmatch between Destination Port and port_mode: Destination Port=" + std::to_string(udpHdr->dstPort)
                + " pm=" + std::to_string(pm)
                + " port_mode=" + std::string(magic_enum::enum_name(port_mode))
            );
            return;
        } else if (pm == 4 && port_mode != UdpStriperPortE::STRIPER_PORT_FEC_ROW) {
            LOG(LoggerVerbosity::ERR, "Mistmatch between Destination Port and port_mode: Destination Port=" + std::to_string(udpHdr->dstPort)
                + " pm=" + std::to_string(pm)
                + " port_mode=" + std::string(magic_enum::enum_name(port_mode))
            );
            return;
        } else if (pm != 0 && pm != 2 && pm != 4) {
            LOG(LoggerVerbosity::ERR, "Invalid PM: Destination Port=" + std::to_string(udpHdr->dstPort)
                + " pm=" + std::to_string(pm)
                + " port_mode=" + std::string(magic_enum::enum_name(port_mode))
            );
            return;
        }
        stripe_processors[stripe_num].ReceivePacket(port_mode, headers, data, length);
    }

    void SendExit(int stripe_num=-1) {
        if (stripe_num == -1) {
            for (auto& sp : stripe_processors) {
                sp.SendExit();
            }
        }
        else {
            if (stripe_num >= stripe_processors.size()) {
                LOG(LoggerVerbosity::ERR, "Trying to send to invalid stripe, num=" + std::to_string(stripe_num));
                return;
            }
            stripe_processors[stripe_num].SendExit();
        }
    }
    
    void WaitForCompleteInternal() {

        // 2. Wait for each process to finish to avoid zombie processes
        for (auto& sp : stripe_processors) {
            sp.stripe_process.wait();
            LOG(my_logger::LoggerVerbosity::CRITICAL, "Child process with PID " + std::to_string(sp.stripe_process.id()) + " exited with code: "
                + std::to_string(sp.stripe_process.exit_code()));
        }
    }
};

// Public StripesManager class methods
StripesManager::StripesManager(AllStriperConfig* striper_config_) : impl(std::make_unique< StripesManagerImpl>(striper_config_)) {}
StripesManager::~StripesManager() = default;
StripesManager::StripesManager(StripesManager&&) noexcept = default;
StripesManager& StripesManager::operator=(StripesManager&&) noexcept = default;

int StripesManager::Initialize(StriperModeE mode_, std::string path_, std::string args_) {
    return impl->InitializeInternal(mode_, path_, args_);
}

void StripesManager::SendMessage(std::string msg, int stripe_num) { // stripe_num == -1 means all stripes
    return impl->SendMessage(msg, stripe_num);
}

void StripesManager::SendPacket(PacketHeaders& pkt, const std::vector<uint8_t>& data, std::size_t length) {
    return impl->SendPacket(pkt, data, length);
}
void StripesManager::ReceivePacket(UdpStriperPortE port_mode, PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length) {
    return impl->ReceivePacket(port_mode, headers, data, length);
}

void StripesManager::SendExit(int stripe_num) { // stripe_num == -1 means all stripes
    return impl->SendExit(stripe_num);
}

void StripesManager::WaitForComplete() {
    return impl->WaitForCompleteInternal();
}

// Class Stripe Process Methods
StripeProcess::StripeProcess(std::string name_, uint16_t stripe_num_, StriperModeE mode_, AllStriperConfig* striper_config_)
    : name(name_) 
    , stripe_num(stripe_num_)
    , mode(mode_)
    , striperConfig(striper_config_)
    , segment(boost::interprocess::managed_shared_memory(open_only, name.c_str()))
{
	SharedDataStr = "SharedData" + std::string(magic_enum::enum_name(mode));
    LOG(LoggerVerbosity::INFO, "This is a Stripe Processor: " + name + " SharedDataSegment=" + SharedDataStr);

    //segment = managed_shared_memory(open_only, name.c_str());


    ThreadManager& TM = ThreadManager::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();

    // Create FEC Engine
    fecEngine = std::make_unique<SMPTE_FEC_Engine>(name, stripe_num, mode, striperConfig);
    if (fecEngine == nullptr) {
        LOG(LoggerVerbosity::ERR, "FEC Engine not initialized for Stripe Process: " + name);
    }

    TM.StartThread("MonitorDequeue-"+name, [this, &TM, &watchdog]() {
        LOG(LoggerVerbosity::INFO, "Starting Stripe Process Dequeue Monitor Thread: name="+ this->name);
        boost::interprocess::managed_shared_memory local_segment(open_only, this->name.c_str());
        // Find the named deque instance
        std::pair<StripeSharedData*, managed_shared_memory::handle_t> res =
            local_segment.find<StripeSharedData>(this->SharedDataStr.c_str());
        if (res.first != nullptr) {
            StripeSharedData* my_data = res.first;

            // Read and pop elements from the deque
            LOG(LoggerVerbosity::INFO, "Found deque with " + std::to_string(my_data->stripe_deque.size()) + " elements.");
            bool exit_loop = false;
            while (!exit_loop && !TM.force_stop.load()) {
                bool msg_available;
                scoped_lock<interprocess_mutex> lock(my_data->mutex);
                my_data->cond_nonempty.wait(lock, [&]() {
                    return !my_data->stripe_deque.empty();
                    });

                // Deque Not empty
                while (!my_data->stripe_deque.empty()) {
                    StripeShmDequeMessage msg = my_data->stripe_deque.front();
                    switch (msg.msg_type) {
                    case DeqMsgType::MESSAGE:
                        break;
                    case DeqMsgType::PACKET:
                    {
                        PacketHeaders pkt_headers;
                        for (const auto& shmHdr : msg.headers) {
                            std::shared_ptr<PacketHeaderBase> hdr = nullptr;
                            switch (shmHdr.htype) {
                            case PacketHeaderType::IPv4:
                                hdr = std::make_shared<PacketHeaderIPv4>();
                                break;
                            case PacketHeaderType::RTP:
                                hdr = std::make_shared<PacketHeaderRTP>();
                                break;
                            case PacketHeaderType::SMPTE:
                                hdr = std::make_shared<PacketHeaderSMPTE>();
                                break;
                            case PacketHeaderType::UDP:
                                hdr = std::make_shared<PacketHeaderUDP>();
                                break;
                            default:
                                LOG(LoggerVerbosity::ERR, "Unknown header type in shared memory: " + std::string(magic_enum::enum_name(shmHdr.htype)));
                            }
                            if (hdr == nullptr) {
                                LOG(LoggerVerbosity::ERR, "Failed to create header object for type: " + std::string(magic_enum::enum_name(shmHdr.htype)));
                                break;
                            }
                            std::vector<uint8_t> header_vec(shmHdr.header.begin(), shmHdr.header.end());
                            auto deserialized_hdr = hdr->deserialize(header_vec);
                            pkt_headers.AddHeader(std::move(deserialized_hdr), -1);
                        }

                        std::vector<uint8_t> vec_data(msg.data.begin(), msg.data.end());
                        if (vec_data.size() != static_cast<size_t>(msg.msg_length)) {
                            LOG(LoggerVerbosity::ERR, "Data size does not match msg_length in shared memory: "
                                + std::to_string(vec_data.size()) + " vs " + std::to_string(msg.msg_length));
                        }
                        this->ReceivedPacket(pkt_headers, vec_data, msg.msg_length);
                    }
                        break;
                    case DeqMsgType::EXIT_PROCESS:
                        exit_loop = true;
                        TM.StopAllThreads();
                        LOG(LoggerVerbosity::CRITICAL, "[" + name + "]: Received Exit command");
                        break;
                    }
                    std::stringstream ss;
                    ss << msg;
                    LOG(LoggerVerbosity::INFO, "Popped: " + ss.str());
                    my_data->stripe_deque.pop_front();
                    watchdog.CheckIn();
                }
            }
        }
        else {
            LOG(LoggerVerbosity::ERR, "Deque object not found in shared memory for Stripe Process: " + name);
        }
		LOG(LoggerVerbosity::CRITICAL, "Exiting Stripe Process Dequeue Monitor Thread: name=" + this->name);
        });
}


void StripeProcess::ReceivedPacket(PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length) {
    if (fecEngine == nullptr) {
        LOG(LoggerVerbosity::ERR, "FEC Engine not initialized for Stripe Process: " + name);
        return;
	}
    if (mode == StriperModeE::TRANSMITTER) {
        // Perform SMPTE FEC on packet
        fecEngine->PerformFEC(headers, data, length);
    } else {

    }
}

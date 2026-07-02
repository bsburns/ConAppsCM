#pragma once
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
    size_t shm_size;
    std::unique_ptr<boost::interprocess::managed_shared_memory> segment;
    //StripeShmDeque* stripe_deque;
    //boost::interprocess::interprocess_condition cond_nonempty;
    //boost::interprocess::interprocess_mutex mutex;
    bp::v1::child stripe_process;
    StripeSharedData* shared_data;

    StripeProcessManager(const char* shm_name_, size_t shm_size_)
        : shm_name(shm_name_), shm_size(shm_size_)
    {
        shared_memory_object::remove(shm_name.c_str());
        segment = std::make_unique<boost::interprocess::managed_shared_memory>(
            boost::interprocess::create_only, shm_name.c_str(), shm_size);

        shared_data = segment->construct<StripeSharedData>("SharedData")(segment->get_segment_manager());
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

        if (mode == StriperModeE::RECEIVER) {
            process_args += " --rx_striper";
        }
        else if (mode == StriperModeE::TRANSMITTER) {
            process_args += " --tx_striper";
        }

        for (int sid = 0; sid < striperConfig->schedulerCfg.MaxNumberStripes; ++sid) {
            std::string process_name = "Stripe-" + std::to_string(sid);
            std::string stripe_args = process_args + " --stripe_process " + process_name + " --logfile " + process_name + ".log";

            LOG(LoggerVerbosity::INFO, "Starting Stripe Process: " + process_name + " at path: " + process_path + " with args: " + process_args);

            try {
                StripeProcessManager spm(process_name.c_str(), 65536); // 64KB shared memory for each stripe
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
    , fecEngine(name_, stripe_num_, mode_, striper_config_)
    , segment(boost::interprocess::managed_shared_memory(open_only, name.c_str()))
{
    LOG(LoggerVerbosity::INFO, "This is a Stripe Processor: " + name);

    //segment = managed_shared_memory(open_only, name.c_str());


    ThreadManager& TM = ThreadManager::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();

    if (mode == StriperModeE::TRANSMITTER) {
        // Open UDP Client for out-going FEC streams
    }

    TM.StartThread("MonitorDequeue-"+name, [this, &TM, &watchdog]() {
        boost::interprocess::managed_shared_memory local_segment(open_only, this->name.c_str());
        // Find the named deque instance
        std::pair<StripeSharedData*, managed_shared_memory::handle_t> res =
            local_segment.find<StripeSharedData>("SharedData");
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
        });
}


void StripeProcess::ReceivedPacket(PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length) {
    if (mode == StriperModeE::TRANSMITTER) {
        // Perform SMPTE FEC on packet
        fecEngine.PerformFEC(headers, data, length);
    } else {

    }
}

// FEC Engine Methods
SMPTE_FEC_Engine::SMPTE_FEC_Engine(std::string owning_stripe_name_, uint16_t stripe_num_, StriperModeE mode_, AllStriperConfig* striper_config_)
    : owning_stripe_name(owning_stripe_name_)
    , stripe_num(stripe_num_)
    , mode(mode_)
    , striperConfig(striper_config_)
	, StatsRxPackets(owning_stripe_name +":FEC_Engine_RxPkts", nullptr)
    , StatsTxPackets(owning_stripe_name + ":FEC_Engine_TxPkts", nullptr)
    , StatsFillPackets(owning_stripe_name + ":FEC_Engine_FillPkts", nullptr)
    , StatsFecColPackets(owning_stripe_name + ":FEC_Engine_FecColPkts", nullptr)
    , StatsFecRowPackets(owning_stripe_name + ":FEC_Engine_FecRowPkts", nullptr)
{
    ThreadManager& TM = ThreadManager::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();

	// Initialize configuration parameters for FEC based on striperConfig
    number_columns = striperConfig->stripeCfg.Parm_L_cols;
    number_rows = striperConfig->stripeCfg.Parm_D_rows;
    switch (striperConfig->stripeCfg.Mode) {
        case FEC_Mode::NONE:
            base_payload_type = RTP_PayloadTypeE::NO_FEC;
            break;
        case FEC_Mode::OptionA: // Column only FEC
            base_payload_type = RTP_PayloadTypeE::MODE_A_FEC;
			number_rows += 1; // Add one row for FEC
            break;
		case FEC_Mode::OptionB: // Row and Column FEC
            base_payload_type = RTP_PayloadTypeE::MODE_B_FEC;
			number_rows += 1; // Add one row for FEC
			number_columns += 1; // Add one column for FEC
            break;
        default:
            LOG(LoggerVerbosity::ERR, "Unknown FEC Mode in configuration");
            base_payload_type = RTP_PayloadTypeE::NOTSET;
            break;
	}
	block_size = number_rows * number_columns;
	dport_data = striperConfig->stripeCfg.StartUdpDstPortNumber;
	dport_col_fec = dport_data + 2;
	dport_row_fec = dport_data + 4;
	double target_packet_rate_period = 1.0 / striperConfig->stripeCfg.MinPacketRate_pps;

	current_column_fec_packets.resize(number_columns);

	// Open UDP Clients for out-going FEC streams
    // DPORT= N>5000(even), Col FEC=N+2 Row FEC=N+4
    boost::asio::io_context io_context;
    std::string ServerIP = "127.0.0.1";
    if (!striperConfig->stripeCfg.StripeReceiverIpAddress.empty()) ServerIP = striperConfig->stripeCfg.StripeReceiverIpAddress;
    
	std::string sport = std::to_string(stripe_num);
	std::string dport_data = std::to_string(striperConfig->stripeCfg.StartUdpDstPortNumber);
    std::string dport_ColFEC = std::to_string(striperConfig->stripeCfg.StartUdpDstPortNumber + 2);
    std::string dport_RowFEC = std::to_string(striperConfig->stripeCfg.StartUdpDstPortNumber + 4);
    udpClientData = std::make_shared< UdpClient>(io_context, ServerIP, sport, dport_data, UdpSendMode::PACKET);
    udpClientColFEC = std::make_shared< UdpClient>(io_context, ServerIP, sport, dport_ColFEC, UdpSendMode::PACKET);
    udpClientRowFEC = std::make_shared< UdpClient>(io_context, ServerIP, sport, dport_RowFEC, UdpSendMode::PACKET);
    if (udpClientData == nullptr) {
        LOG(LoggerVerbosity::ERR, "Could not create UDP client for stripe data: Stripe=" + owning_stripe_name
            + " ServerIP=" + ServerIP + " SrcPort=" + sport
            + " DstPort=" + dport_data
            );
        return;
    }
    if (udpClientColFEC == nullptr) {
        LOG(LoggerVerbosity::ERR, "Could not create UDP client for stripe column FEC: Stripe=" + owning_stripe_name
            + " ServerIP=" + ServerIP + " SrcPort=" + sport
            + " DstPort=" + dport_ColFEC
        );
        return;
    }
    if (udpClientRowFEC == nullptr) {
        LOG(LoggerVerbosity::ERR, "Could not create UDP client for stripe row FEC: Stripe=" + owning_stripe_name
            + " ServerIP=" + ServerIP + " SrcPort=" + sport
            + " DstPort=" + dport_RowFEC
        );
        return;
    }

    // Tell UDP Server we are in Packet Mode:
	/* Won't work until we implement a UDP Server that can receive this message and change mode
    
    udpClientData->SendMessage("PACKET_MODE:");
    udpClientColFEC->SendMessage("PACKET_MODE:");
    udpClientRowFEC->SendMessage("PACKET_MODE:");
    */

    TM.StartThread("IdleFill", [this, &watchdog, &TM, &target_packet_rate_period]() {
        std::chrono::duration<double> duration(target_packet_rate_period);
		std::vector<uint8_t> fill_packet(1, 0); // Fill packet of 1 byte
		PacketHeaders fill_headers;
        auto ipHdr = std::make_shared<PacketHeaderIPv4>();
        ipHdr->Version = 4;
        ipHdr->IHL = 5; // 5 * 4 = 20 bytes
        ipHdr->TOS = 0;
        ipHdr->totalLength = 0;
        ipHdr->TTL = 64;
        ipHdr->Protocol = static_cast<uint8_t>(PacketHeaderType::UDP); // UDP
        ipHdr->srcIP = 0;
        ipHdr->dstIP = 0; // Destination IP can be set to 0 for now, or you can set it to a specific value if needed
        fill_headers.AddHeader(ipHdr);

        while (!TM.force_stop.load()) {
            // Idle loop to fill FEC block 
            if (this->idle.load() && this->block_started.load()) {
                LOG(LoggerVerbosity::DEBUG, "IdleFill: FEC block started but idle, injecting fill packets to maintain rate");
                // Inject fill packets here
				this->PerformFEC(fill_headers, fill_packet, fill_packet.size(), true);
			}
			this->idle.store(true);

            std::this_thread::sleep_for(duration);
        }
        });
}

int SMPTE_FEC_Engine::PerformFEC(PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length, bool fill)
{
    // Implement SMPTE FEC algorithm here
	block_started.store(true);
	idle.store(false);
	bool send_fec_column_fec = false;
	bool send_fec_row_fec = false;

    StatsRxPackets.addValue(length);

    auto hdr = headers.headers.back();
	auto ip_hdr = std::dynamic_pointer_cast<PacketHeaderIPv4>(hdr);
    if (!ip_hdr) {
        LOG(LoggerVerbosity::ERR, "First header is not IPv4, cannot perform FEC: type="
            + std::string(magic_enum::enum_name(hdr->header_type)));
        return -1;
	}

    // Create RTP Header
	std::shared_ptr<PacketHeaderRTP> rtp_hdr = std::make_shared<PacketHeaderRTP>();
    rtp_hdr->payload_type = static_cast<uint8_t>(base_payload_type);
    rtp_hdr->sequence_number = this->sequence_number.fetch_add(1, std::memory_order_relaxed); // atomic increment
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    rtp_hdr->timestamp = millis.count(); // Set appropriate timestamp
    rtp_hdr->sync_src = ip_hdr->srcIP; // Set appropriate sync source
	headers.AddHeader(rtp_hdr, 0); // Add RTP header at the beginning
    LOG(LoggerVerbosity::INFO, "Performing FEC on packet with length: " + std::to_string(length)
    + " RTP_SEQ=" + std::to_string(rtp_hdr->sequence_number)
    + " RxPktStats:" + StatsRxPackets.ToString());

    switch (base_payload_type) {
    case RTP_PayloadTypeE::NO_FEC:
        break;
    case RTP_PayloadTypeE::MODE_A_FEC: // Column FEC
    {
        auto cell_index = rtp_hdr->sequence_number % block_size;
        auto row = cell_index / number_columns;
        auto column = cell_index % number_columns;
        if (row == number_rows - 2 && column == number_columns - 1) {
            LOG(LoggerVerbosity::INFO, "MODE_A: reached FEC ROW: row="
                + std::to_string(row)
                + " col=" + std::to_string(column));
            send_fec_column_fec = true;
            block_started.store(false);
        }
        if (cell_index == 0) {
            LOG(LoggerVerbosity::INFO, "Starting new FEC block: seq=" + std::to_string(rtp_hdr->sequence_number)
                + " row=" + std::to_string(row) + " column=" + std::to_string(column));
            for (auto& fec_pkt : current_column_fec_packets) {
                fec_pkt.Clear();
            }
        }
        current_column_fec_packets[column].AddPacket(rtp_hdr, data, length);
    }
        break;
    case RTP_PayloadTypeE::MODE_B_FEC: // Row and Column FEC
        break;
    default:
        LOG(LoggerVerbosity::ERR, "Base Payload type not valid!! type="
            + std::string(magic_enum::enum_name(base_payload_type)));
        return 1;
    }

	// Send packet to UDP Port
// Stopped here
	// If we have reached the end of a FEC block, send the FEC packets
    if (send_fec_column_fec) {
        LOG(LoggerVerbosity::INFO, "Sending Column FEC packets for block ending at seq="
            + std::to_string(rtp_hdr->sequence_number));
        for (size_t col = 0; col < current_column_fec_packets.size(); ++col) {
            auto& fec_pkt = current_column_fec_packets[col];
            if (fec_pkt.payload.size() > 0) {
                // Send fec_pkt to UDP Port for column FEC
                StatsFecColPackets.addValue(fec_pkt.payload.size());
                LOG(LoggerVerbosity::INFO, "Sent Column FEC packet for column " + std::to_string(col)
                    + " size=" + std::to_string(fec_pkt.payload.size())
                    + " FecColStats:" + StatsFecColPackets.ToString());
            }
        }
	}
	return 0; // Return 0 for success
}
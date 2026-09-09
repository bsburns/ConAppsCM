/*------------------------------------------------------------------
 * SXTP_TX_main.cpp
 *
 * Space X Thruput Test Transmitter main program
 * 
 * Sends UDP stream to SpaceX Thruput Test Receiver
 *
 * August 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */#include "logger.h"

#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <thread>
#include <boost/asio.hpp>

#include "SXTP_common.h"
#include "commandLineParser.h"
#include "watchdog.h"
#include "threadManager.h"
#include "cli/CLI.h"
#include "PacketHeader/PacketHeader.h"
#include "statistics.h"


using boost::asio::ip::udp;
using namespace my_logger;
namespace fs = std::filesystem;

std::string OutDir = "";

std::string show_version(bool use_cout = false) {
    std::stringstream ss;
    ss << "\n\tSaturnX Space Vehicle emulation Version: " << VERSION << "." << GIT_HASH << std::endl;
    ss << "\tBuild Date: " << __DATE__ << std::endl;
    ss << "\tBuild Time: " << __TIME__ << std::endl;
#ifdef _WIN32
    ss << "\tMSVC Version: " << _MSC_FULL_VER << std::endl;
#else
    ss << "\tCompiler: " << __VERSION__ << std::endl;
#endif
    ss << "\tBuild Machine: " << BUILD_MACHINE << std::endl;

    if (use_cout) {
        std::cout << ss.str();
    }
    return ss.str();
}

using namespace my_logger;
namespace fs = std::filesystem;

static const std::size_t CHUNK_SIZE = 1024; // safe UDP payload


class FileTransferHeader {
public:
    uint16_t chunkNumber;
    uint16_t chunkSize;
    FileTransferHeader() : chunkNumber(0), chunkSize(0) {}
    int GetHeaderSizeBytes() const {
        return 4; // 2 bytes for chunkNumber and 2 bytes for chunkSize
    }
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> data(4); // 4 bytes for the header
        data[0] = chunkNumber >> 8;
        data[1] = chunkNumber & 0xFF;
        data[2] = chunkSize >> 8;
        data[3] = chunkSize & 0xFF;
        return data;
    }
    static FileTransferHeader deserialize(const std::vector<uint8_t>& data) {
        if (data.size() < 4) {
            throw std::invalid_argument("Data too short for FileTransferHeader");
        }
        FileTransferHeader header;
        header.chunkNumber = (data[0] << 8) | data[1];
        header.chunkSize = (data[2] << 8) | data[3];
        return header;
    }
};


class TestUdpClient {
private:
    std::string sport;
    std::string dport;
    UdpSendMode sendMode;
    std::string serverIP;
    udp::socket socket;
    udp::resolver resolver;
    udp::endpoint receiver_endpoint;

public:
    TestUdpClient(boost::asio::io_context& io_context, std::string serverp_ip_, std::string sport_, std::string dport_, UdpSendMode mode_)
        : resolver(io_context)
        , receiver_endpoint(*resolver.resolve(udp::v4(), serverIP, dport).begin())
        , socket(io_context, udp::v4())
        , serverIP(serverp_ip_)
        , sport(sport_)
        , dport(dport_)
        , sendMode(mode_)
    {
        //boost::asio::io_context io_context;
        // 2. Resolve the remote hostname or IP address and port
        LOG(LoggerVerbosity::INFO, "Connecting to " + serverIP + ":" + sport + "::" + dport);

        // 3. Open the UDP socket
        int sourcePort;
        try {
            sourcePort = std::stoi(sport);
        }
        catch (std::exception& e) {
            std::cerr << "\nException: UdpClient: convert source Port: " << e.what() << std::endl;
            return;
        }

        try {
            //udp::endpoint local_ep(udp::v4(), sourcePort);
            if (sourcePort == 0) {
                LOG(LoggerVerbosity::INFO, "UDPC: Binding to any available port (sourcePort=0)");
			}
            socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), sourcePort));
            LOG(LoggerVerbosity::INFO, "UDPC: Binded to source port=" + std::to_string(sourcePort));
        }
        catch (std::exception& e) {
            std::cerr << "\nException: UdpClient: bind local endpoint: " << e.what() << std::endl;
            return;
        }

        try
        {
            receiver_endpoint = *resolver.resolve(udp::v4(), serverIP, dport).begin();
            //socket.open(udp::v4());
        }
        catch (const std::exception& e)
        {
            std::cerr << "\nException: UdpClient: Open socket with endpoint: " << e.what() << std::endl;
            return;
        }
        
        if (sendMode == UdpSendMode::MESSAGE) {
            LOG(LoggerVerbosity::INFO, "UDP Client is in MESSAGE mode.");
			SendMessage("Hello from UDP Client!");
        } else if (sendMode == UdpSendMode::PACKET) {
            LOG(LoggerVerbosity::INFO, "UDP Client is in PACKET mode.");
			StartPacketMode();
        } else if (sendMode == UdpSendMode::TEST_DATA_MODE) {
            LOG(LoggerVerbosity::INFO, "UDP Client is in TEST_DATA_MODE mode.");
            StartTestDataMode();
        } else {
			LOG(LoggerVerbosity::ERR, "UDP Client has an unknown send mode.");
        }
    }

    int SendFile(std::string filename) {
        if (!fs::exists(filename)) {
            LOG(LoggerVerbosity::ERR, "Send File does not exist!! file=" + filename);
            return 1;
        }

        LOG(LoggerVerbosity::INFO, "Sending file: " + filename);
        std::string message = "FILE_MODE: " + filename;
        socket.send_to(boost::asio::buffer(message), receiver_endpoint);

        FileTransferHeader fh;
        auto FTHS = fh.GetHeaderSizeBytes();

        std::vector<char> buffer(CHUNK_SIZE + FTHS);
        size_t total_bytes_sent = 0;

        // Open file in binary mode
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            LOG(LoggerVerbosity::ERR, "Cannot open file " + filename);
            return 2;
        }
        while (file) {
            file.read(buffer.data() + FTHS, CHUNK_SIZE - FTHS);
            std::streamsize bytes_read = file.gcount();
            if (bytes_read > 0) {
                fh.chunkSize = static_cast<uint16_t>(bytes_read);
                std::vector<uint8_t> headerData = fh.serialize();
                std::copy(headerData.begin(), headerData.end(), buffer.begin());
                size_t bytes_sent = socket.send_to(boost::asio::buffer(buffer.data(), bytes_read + FTHS), receiver_endpoint);
                total_bytes_sent += bytes_sent;
                LOG(LoggerVerbosity::INFO, "Send file: bytes_sent=" + std::to_string(bytes_sent) +
                    " total_bytes_sent=" + std::to_string(total_bytes_sent) +
                    " bytes_read=" + std::to_string(bytes_read));
                fh.chunkNumber++;
            }
        }
        // Send an empty packet to indicate EOF
        fh.chunkSize = 0;
        socket.send_to(boost::asio::buffer(fh.serialize(), FTHS), receiver_endpoint);

        LOG(LoggerVerbosity::INFO, "File sent successfully. Total bytes: " +
            std::to_string(total_bytes_sent));
        return 0;
    }

    int StartTestDataMode() {
        std::string message = "TEST_DATA_MODE:";
        if (socket.is_open()) {
            LOG(LoggerVerbosity::CRITICAL, "Starting Mode: " + message);
        }
        else {
            LOG(LoggerVerbosity::ERR, "Socket is not open. Cannot start Packet Mode.");
            return 1;
        }
        socket.send_to(boost::asio::buffer(message), receiver_endpoint);

        // Prepare a buffer to receive the reply back
        std::array<char, 1024> recv_buf;
        udp::endpoint sender_endpoint;

        // This blocks until data arrives
        size_t len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);

        std::cout << "\nReceived reply: ";
        std::cout.write(recv_buf.data(), len);
        std::cout << std::endl;
        return 0;
    }

    int SendTestData(PacketHeaderStripeTest& header, std::vector<uint8_t>& data, std::size_t length) {
        LOG(LoggerVerbosity::DEBUG, "UDPC: SendPacket: port=" + sport + "::" + dport + " : "
            + header.to_string()
            + " length=" + std::to_string(length)
        );

        if (!socket.is_open()) {
            LOG(LoggerVerbosity::ERR, "UDPC: port=" + sport + "::" + dport + " : Socket is not open. Cannot send packet.");
            return -1;
        }
        try {
            std::copy_n(header.serialize().begin(), header.Size(), data.begin());
            size_t bytes_sent = socket.send_to(boost::asio::buffer(data.data(), length), receiver_endpoint);
            LOG(LoggerVerbosity::DEBUG, "UDPC: port=" + sport + "::" + dport + " : Sent packet: bytes_sent=" + std::to_string(bytes_sent)
                + " total_length=" + std::to_string(length)
                //+ " headers=" + headers.ToString()
            );
        }
        catch (std::exception& e) {
            LOG(LoggerVerbosity::ERR, "Exception in SendPacket: " + std::string(e.what()));
            return -100;
        }

        return 0;
    }


    int StartPacketMode() {
        std::string message = "PACKET_MODE:";
        if (socket.is_open()) {
            LOG(LoggerVerbosity::INFO, "Starting Packet Mode: " + message);
        }
        else {
            LOG(LoggerVerbosity::ERR, "Socket is not open. Cannot start Packet Mode.");
            return 1;
        }
        socket.send_to(boost::asio::buffer(message), receiver_endpoint);
        return 0;
    }

    int SendPacket(PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length) {

        LOG(LoggerVerbosity::INFO, "UDPC: SendPacket: port=" + sport + "::" + dport + " : "
            + headers.ToString()
            + " length=" + std::to_string(length)
        );
        auto rc = headers.MakePacket(data, length);
        if (rc != 0) {
            LOG(LoggerVerbosity::ERR, "UDPC: port=" + sport + "::" + dport + " : Failed to make packet from headers.rc = " + std::to_string(rc));
            return rc;
        }

        if (!socket.is_open()) {
            LOG(LoggerVerbosity::ERR, "UDPC: port=" + sport + "::" + dport + " : Socket is not open. Cannot send packet.");
            return -1;
        }
        try {
            size_t bytes_sent = socket.send_to(boost::asio::buffer(data.data(), length), receiver_endpoint);
            LOG(LoggerVerbosity::INFO, "UDPC: port=" + sport + "::" + dport + " : Sent packet: bytes_sent=" + std::to_string(bytes_sent)
                + " total_length=" + std::to_string(length)
                //+ " headers=" + headers.ToString()
            );
        }
        catch (std::exception& e) {
            LOG(LoggerVerbosity::ERR, "Exception in SendPacket: " + std::string(e.what()));
            return -100;
        }

        return 0;
    }

    void SendMessage(std::string msg) {
        socket.send_to(boost::asio::buffer(msg), receiver_endpoint);
        std::cout << "\nSent message: " << msg;

        // Prepare a buffer to receive the reply back
        std::array<char, 1024> recv_buf;
        udp::endpoint sender_endpoint;

        // This blocks until data arrives
        size_t len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);

        std::cout << "\nReceived reply: ";
        std::cout.write(recv_buf.data(), len);
        std::cout << std::endl;
    }
};

class PacketGenerator {
public:
    uint64_t NumPackets = 1;
	double RunTime = 0; // seconds, 0 means run until NumPackets sent
    double PacketRate = 1.0; // packets per second
    uint32_t PacketSize = 1024; // bytes
	uint64_t SkipInterval = 0; // Skip every N packets
	uint32_t SkipCount = 0; // Number of packets to skip after SkipInterval

    // Statistics
    StatisticsRTM<uint64_t> StatsTxPackets;
    StatisticsBasic<double> StatsSleepTime;
    StatisticsBasic<double> StatsBurstTime;
    StatisticsBasic<double> StatsPktTime;

	// Working variables
	std::vector<uint8_t> pkt_buffer;
	PacketHeaderStripeTest pkt_header;

    PacketGenerator() 
		: StatsTxPackets("TxPackets", nullptr, 1.0, false)
		, StatsSleepTime("SleepTime", nullptr)
		, StatsBurstTime("BurstTime", nullptr)
		, StatsPktTime("PktTime", nullptr)
    {}

    void Start(TestUdpClient& client) {
        // Implementation for starting the packet generator with the given UDP client
        uint64_t sequence = 0;
		uint8_t pattern = 0xA5;
        ThreadManager& TM = ThreadManager::GetInstance();
        Watchdog& watchdog = Watchdog::GetInstance();

        ConfigurePktBfr(pattern);

		//pkt_buffer[48] = 0xAA; // Just for testing, set a specific byte in the packet buffer
        pkt_header.length = PacketSize - pkt_header.Size();
        pkt_header.command = (uint8_t) PacketHeaderStripeTest::Command::START;

        if (RunTime > 0) {
            if (NumPackets > 0) {
                LOG(LoggerVerbosity::WARNING, "PacketGenerator: Both RunTime and NumPackets are set. RunTime will take precedence.");
			}
            NumPackets = (uint64_t)(RunTime * PacketRate);
		}
        double packet_burst_interval = 0;
		double target_burst_interval = 0.1;
        uint32_t burst_size = 500;
        double pkt_tx_time = 1.0 / PacketRate; // seconds
        if (PacketRate > 0) {
			burst_size = std::max<uint32_t>(1, (uint32_t)(PacketRate * target_burst_interval));
			packet_burst_interval = pkt_tx_time * burst_size; // seconds for burst
            packet_burst_interval *= 0.95; // Fast Start
            //packet_burst_interval *= 1.7; // magic factor to make rate work
        }
        double skip_interval = pkt_tx_time * SkipCount;

        std::cout << "\nStarting Packet Generator: " << ConfigString() 
			<< " | Burst Interval: " << packet_burst_interval << " seconds"
            << " | Burst Size: " << burst_size << " packets"
			<< " | Run Time: " << RunTime << " seconds"
            << std::endl;

        std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point last_output_time = start_time;
        std::chrono::steady_clock::time_point last_burst_time = start_time;
        while (!TM.force_stop
            && (NumPackets == 0 || StatsTxPackets.count() < NumPackets))
        {
            std::chrono::steady_clock::time_point start_loop_time = std::chrono::steady_clock::now();
            if (SkipInterval != 0 && SkipCount != 0) {
                auto skip_condition = sequence % SkipInterval;
                if (skip_condition == 0 && sequence != 0) {
                    sequence += SkipCount;
					NumPackets -= SkipCount; // Adjust total packets to send
                    LOG(LoggerVerbosity::DEBUG, "PacketGenerator: Skipping " + std::to_string(SkipCount) + " packets at sequence=" + std::to_string(sequence));
                    std::this_thread::sleep_for(std::chrono::duration<double>(skip_interval));
                }
            }
            pkt_header.sequence_num = sequence++;
            StatsTxPackets.addValue(PacketSize);

            std::chrono::steady_clock::time_point tx_time = std::chrono::steady_clock::now();
            std::chrono::system_clock::time_point sys_tx_time = std::chrono::system_clock::now();
            pkt_header.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(sys_tx_time.time_since_epoch()).count();
            pkt_header.command = (uint8_t)PacketHeaderStripeTest::Command::NOMINAL;
            client.SendTestData(pkt_header, pkt_buffer, PacketSize);

            auto curr_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed_output = curr_time - last_output_time;
            if (elapsed_output.count() >= 1.0) {
                std::cout << "\nPackets Sent: " << StatsTxPackets.count() << " / " << NumPackets
                    << " | Total Bytes: " << StatsTxPackets.sum()
                    << " | TXpps: " << to_engineering(StatsTxPackets.periodCountRate())
                    << " | TXbps: " << to_engineering(StatsTxPackets.periodUnitRate() * 8)
                    << std::flush;
                last_output_time = curr_time;

				// Adjust burst interval based on actual TX rate
                double actual_tx_rate = StatsTxPackets.periodCountRate();
                if (actual_tx_rate > 0) {
                    double rate_ratio = actual_tx_rate / PacketRate;
                    if (rate_ratio > 0) {
                        packet_burst_interval -= (1-rate_ratio)*packet_burst_interval/10;
						//std::cout << "\nratio=" << rate_ratio << " rate=" << actual_tx_rate << " burst_iv = " << packet_burst_interval << std::flush;
						//LOG(LoggerVerbosity::INFO, "PacketGenerator: Adjusted burst interval to " + std::to_string(packet_burst_interval) + " seconds based on actual TX rate of " + std::to_string(actual_tx_rate) + " pps  ratio=" + std::to_string(rate_ratio));
                    }
				}
            }

            std::chrono::steady_clock::time_point end_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> pkt_time = end_time - start_loop_time;
            StatsPktTime.addValue(pkt_time.count());

            if (packet_burst_interval != 0 && StatsTxPackets.count() % burst_size == 0) {
                watchdog.CheckIn();

                curr_time = std::chrono::steady_clock::now();
                std::chrono::duration<double> elapsed = curr_time - last_burst_time;
                auto sleep_duration = std::chrono::duration<double>(packet_burst_interval - elapsed.count());
                if (sleep_duration.count() < 0) {
                    sleep_duration = std::chrono::duration<double>(0);
				}
                StatsSleepTime.addValue(sleep_duration.count());
                StatsBurstTime.addValue(elapsed.count());
                std::this_thread::sleep_for(sleep_duration);
				last_burst_time = std::chrono::steady_clock::now();
            }
        }

		// Send final Packet with STOP command
        pkt_header.sequence_num = sequence++;
        StatsTxPackets.addValue(PacketSize);
        std::chrono::system_clock::time_point last_tx_time = std::chrono::system_clock::now();
        pkt_header.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(last_tx_time.time_since_epoch()).count();
        pkt_header.command = (uint8_t)PacketHeaderStripeTest::Command::STOP;
        client.SendTestData(pkt_header, pkt_buffer, PacketSize);

        std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_time;
		double overall_rate = StatsTxPackets.count() / elapsed.count();

        std::cout << "\n\nFINAL: Packets Sent: " << StatsTxPackets.count() << " / " << NumPackets
            << " | Total Bytes: " << StatsTxPackets.sum()
            << " | TXpps: " << to_engineering(StatsTxPackets.periodCountRate()) 
            << " | TXbps: " << to_engineering(StatsTxPackets.periodUnitRate()*8)
            << std::flush;

		std::cout << "\nSleep Time Stats: " << StatsSleepTime.ToString();
		std::cout << "\nBurst Time Stats: " << StatsBurstTime.ToString();
		std::cout << "\nPacket Time Stats: " << StatsPktTime.ToString();
        std::cout << "\nElapsed Time: " << elapsed.count() << " seconds  Overall Rate=" << to_engineering(overall_rate) << "\n";

        exit(0);
    }

    void ConfigurePktBfr(uint8_t pattern) {
        pkt_header.pattern = pattern;
        pkt_buffer.assign(PacketSize, pattern);
    }

    std::string ConfigString() {
        std::stringstream ss;
        ss << "PG_CFG = {"
           << " NumPackets=" << NumPackets
           << ", PacketRate=" << PacketRate
           << ", PacketSize=" << PacketSize
           << " }";
        return ss.str();
    }
};

int main(int argc, char* argv[]) {
    my_logger::LoggerVerbosity verbosity = my_logger::LoggerVerbosity::ERR;
    double WatchdogTimeout = 360;
    std::string LogFile;
    std::string ServerPort = "8080";
	std::string SourcePort = "0";
	std::string ServerIP = "127.0.0.1";
    PacketGenerator PG;

	LOG_INST.verbosity = verbosity;
    LOG_INST.SetTimeStamping(false);

    CommandLineParser CLP;
    CLP.AddCommand({
        CLP_Command("version", "Show version information", [](const std::string& argument) {
            std::cout << "\nVERSION INFO:";
            show_version(true);
            exit(0);
        }, "", typeid(void)),
        CLP_Command("verbosity, v", "Set logging verbosity level: " +  LOG_INST.GetLogLevelNames() , [&verbosity](const std::string& argument) {
            std::string uarg = argument;
            std::transform(uarg.begin(), uarg.end(), uarg.begin(), ::toupper);
            verbosity = magic_enum::enum_cast<LoggerVerbosity>(uarg).value_or(LoggerVerbosity::NOTSET);
            if (verbosity == LoggerVerbosity::NOTSET) {
                try {
                    int v_int = std::stoi(argument);
                    verbosity = static_cast<LoggerVerbosity>(v_int);
                }
                catch (const std::exception& e) {
                    std::cerr << "Invalid verbosity level: " << argument << ". Setting to NOTSET.\n";
                }
            }
			LOG_INST.verbosity = verbosity;
        }, "ERR", typeid(std::string)),
        CLP_Command("outdir, d", "Specifies output directory", [](const std::string& argument) {
            OutDir = argument;
        }, ".\\.output", typeid(std::string)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "SXTP_TX.log", typeid(std::string)),
        CLP_Command("timestamping, T", "Adds timestamps to log entries",
            [](const std::string& argument) {
            LOG_INST.SetTimeStamping(true);
        }, "", typeid(void)),
        CLP_Command("server_port, p", "Specifies UDP Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "12000", typeid(std::string)),
        CLP_Command("source_port, s", "Specifies UDP Port number of source", [&SourcePort](const std::string& argument) {
            SourcePort = argument;
		}, "0", typeid(std::string)),
        CLP_Command("server_ip, i", "Specifies UDP IP address of server", [&ServerIP](const std::string& argument) {
            ServerIP = argument;
		}, "127.0.0.1", typeid(std::string)),
        CLP_Command("num_packets, n", "Specifies the number of packets to send", [&PG](const std::string& argument) {
            std::stringstream ss(argument);
			double num_packets_double;
            if (!(ss >> num_packets_double && ss.eof())) {
                std::cerr << "Invalid number packets: " << argument << "\n";
                exit(10);
            }
			PG.NumPackets = static_cast<uint64_t>(num_packets_double);
        }, "0", typeid(uint64_t)),
        CLP_Command("pkt_rate, r", "Specifies the packets rate to send at", [&PG](const std::string& argument) {
            try {
                PG.PacketRate = std::stod(argument);
            }
            catch (const std::exception& e) {
                std::cerr << "Invalid packet Rate: " << argument << ". Setting to default 1.\n";
                PG.PacketRate = 1.0;
            }
        }, "1", typeid(double)),
        CLP_Command("run_time, x", "Specifies the time (seconds) to run for", [&PG](const std::string& argument) {
            try {
                PG.RunTime = std::stod(argument);
            }
            catch (const std::exception& e) {
                std::cerr << "Invalid run time: " << argument << ". Setting to default 0.\n";
                PG.RunTime = 0;
            }
        }, "0", typeid(double)),
        CLP_Command("pkt_size, t", "Specifies the packet size to send", [&PG](const std::string& argument) {
            std::stringstream ss(argument);
            if (!(ss >> PG.PacketSize && ss.eof())) {
                std::cerr << "Invalid packet size: " << argument << "\n";
                exit(10);
            }
            if (PG.PacketSize < 64 || PG.PacketSize > 1440) {
                std::cerr << "Packet size not valid. Must be 64-1440 bytes. Specified=" << PG.PacketSize << "\n";
                exit(11);
            }
        }, "64", typeid(uint32_t)),
        CLP_Command("skip_intv, a", "Specifies the skip interval of sequence numbers", [&PG](const std::string& argument) {
            std::stringstream ss(argument);
            if (!(ss >> PG.SkipInterval && ss.eof())) {
                std::cerr << "Invalid skip sequeunce interval: " << argument << "\n";
                exit(12);
            }
        }, "0", typeid(uint64_t)),
        CLP_Command("skip_cnt, b", "Specifies the number of seqs to skip in a skip interval", [&PG](const std::string& argument) {
            std::stringstream ss(argument);
            if (!(ss >> PG.SkipCount && ss.eof())) {
                std::cerr << "Invalid skip sequeunce interval: " << argument << "\n";
                exit(12);
            }
        }, "0", typeid(uint64_t)),
        CLP_Command("watchdog,w", "Watchdog timeout in seconds", [&WatchdogTimeout](const std::string& argument) {
            try {
                WatchdogTimeout = std::stod(argument);
            }
            catch (const std::exception& e) {
                std::cerr << "\nInvalid watchdog timeout value: " << argument << ". Setting to default 360 seconds.\n";
                WatchdogTimeout = 360;
            }
        }, "360", typeid(double)),
        });

    LOG(LoggerVerbosity::DEBUG, show_version());
    LOG(LoggerVerbosity::DEBUG, "*** Setting DEFAULT command line arguments...***");
    CLP.SetDefaultValues();
    LOG(LoggerVerbosity::DEBUG, "*** Parsing command line arguments...***");
    CLP.ProcessArguments(argc, argv);

	// Validate output directory exists
    fs::path dirPath = OutDir;
    if (!(fs::exists(dirPath) && fs::is_directory(dirPath))) {
        std::cout << "Output Directory does not exist: \"" << OutDir << "\"\n";
        std::cout << "Creating output directory\n";
        std::filesystem::create_directory(dirPath);
    }

    LOG_INST.SetLogFile(LogFile);
    LOG(LoggerVerbosity::DEBUG, "Starting log file: " + LogFile);
    auto vname = std::string(magic_enum::enum_name(LOG_INST.verbosity));
    if (vname.empty()) vname = std::to_string(LOG_INST.GetVerbosity());
    LOG(LoggerVerbosity::CRITICAL, "Current verbosity = " + vname);    
    

    // Start WATCHDOG thread to monitor and adjust FEC stripes
    ThreadManager& TM = ThreadManager::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();
    watchdog.SetTimeout(WatchdogTimeout);
    watchdog.SetOnTimeoutForceExit(false); // Force exit on watchdog timeout

    watchdog.SetOnTimeoutCallback([]() {
        // Perform any necessary cleanup here
        LOG(LoggerVerbosity::CRITICAL, "Watchdog timeout callback invoked. Performing cleanup before exit.");
        });

    TM.StartThread("WatchdogMonitor", Watchdog::monitor_thread);
    if (1) {
        std::string arg_string = "[";
        for (int i = 0; i < argc; ++i) {
            arg_string += argv[i];
            if ((i + 1) != argc) arg_string += " ";
        }
        arg_string += "]";
        LOG(LoggerVerbosity::CRITICAL, "Arguments = " + arg_string);
    }

    // Start CLI input thread
    auto& CMP = CliMenuProcessor::GetInstance(".SXTP_TX.command_history");
    CMP.SetPrompt("SXTP_TX> ");
    CMP.AddSubMenu(LOG_INST.cli_menu);
    CMP.AddSubMenu(Watchdog::GetInstance().cli_menu);
    TM.StartThread("CLIInput", CliMenuProcessor::GetUserInput_thread);

    try {
        boost::asio::io_context io_context;
		LOG(LoggerVerbosity::CRITICAL, "Starting UDP Client to Server: " + ServerIP + ":" + ServerPort + " from Source Port: " + SourcePort);
		TestUdpClient uclient(io_context, ServerIP, SourcePort, ServerPort, UdpSendMode::TEST_DATA_MODE);
        io_context.run();
        PG.Start(uclient);
    }
    catch (std::exception& e) {
        std::cerr << "Client error: " << e.what() << "\n";
    }


    // Wait for threads to join
    LOG(LoggerVerbosity::INFO, "Waiting threads to join...");
    watchdog.StopMonitoring(); 
    TM.WaitAllThreads(); // Wait for all threads to finish
    return 0;
}

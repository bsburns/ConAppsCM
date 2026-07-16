/*------------------------------------------------------------------
 * test-Server.cpp
 * 
 * Boost based UDP Server to receive test traffic streams
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "logger.h"
#include <iostream>
#include <string>
#include <array>
#include <boost/asio.hpp>
#include <random>
#include <limits>
#include <chrono>
#include <set>

#include <boost/asio.hpp>

#include "commandLineParser.h"
#include "watchdog.h"
#include "PacketHeader.h"
#include "statistics.h"
#include "bUDP.h"
#include "cli/CLI.h"

std::string show_version(bool use_cout = false) {
    std::stringstream ss;
    ss << "\n\tTest Server (boost) Version: " << VERSION << "." << GIT_HASH << std::endl;
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
using boost::asio::ip::udp;

class test_udp_connection {
public:
    std::string name;
    bool validPacketStream = false;
    uint16_t srcPort = 0;
    uint32_t srcIP = 0;
    std::map<uint16_t, std::vector<uint8_t>> file_chunks; // For storing file chunks if in SEND_FILE mode
    std::string file_name; // Store the file name being sent by the client
    std::chrono::system_clock::time_point connection_time; // Track when the connection was established
    uint32_t expected_sequence = 0;
    std::map<uint32_t, bool> rx_seq_nums; // key is sequence numbers

    // Statistics
    StatisticsRTM<uint64_t> StatsRxPackets;
    StatisticsRTM<uint64_t> StatsBadPackets;
    uint64_t StatOutOfSequence = 0;
    uint64_t StatBadPacketData = 0;
    uint64_t StatBadLength = 0;
    uint64_t StatDuplicateSeqNum = 0;

    test_udp_connection() {}
    test_udp_connection(std::string conn_name) 
        : name(conn_name)
        , StatsRxPackets(conn_name + ":RxPkts", nullptr)
        , StatsBadPackets(conn_name + ":BadPkts", nullptr)
    {}

     std::string BriefStats() const {
        std::string str = name;
        str += ": RXP=" + std::to_string(StatsRxPackets.count());
        str += ", RXB=" + std::to_string(StatsRxPackets.sum());
        str += ", RXBPS=" + std::to_string(StatsRxPackets.periodUnitRate()*8);
        str += ", BADP=" + std::to_string(StatsBadPackets.count());

        return str;
    }
};


class TestUdpServer {
public:
    const MenuItem cli_menu =
    {
    .name = "show",
    .description = "show server status commands",
    .subMenus = {
        {
            .name = "connections",
            .description = "Show open connections",
            .subMenus = {},
            .valType = MenuItemValueTypes::NONE,
            .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {
                std::cout << "\nConnections:";
                for (const auto& [key, conn] : KnownClientConnections) {
                    std::cout << "\n\t" << conn.BriefStats();
                }
            },
        },
    },

    .valType = MenuItemValueTypes::SUBMENU,
    .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {
        std::cout << "\nAll connections: ";
        for (const auto& [key, conn] : KnownClientConnections) {
            std::cout << "\n\t" << conn.name;
        }
        },
    };

    // Bind to the given port on all available network interfaces
    TestUdpServer(boost::asio::io_context& io_context, short port_)
        : socket_(io_context, udp::endpoint(udp::v4(), port_))
        , port(port_)
    {
        start_receive();
    }

    void StopServer() {
        boost::system::error_code ec;
        socket_.cancel(ec);
    }
private:
    short port;
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::vector<uint8_t> recv_buffer_{ std::vector<uint8_t>(4096, 0) };
    std::map<udp::endpoint, test_udp_connection> KnownClientConnections; // Map of client endpoints to their connection info

    void start_receive() {
        // Wait asynchronously for an incoming packet
        socket_.async_receive_from(
            boost::asio::buffer(recv_buffer_), remote_endpoint_,
            [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                if (ec == boost::asio::error::operation_aborted) {
                    // Graceful exit: operation was canceled, cleanup and return
                    return;
                }

                if (ec) {
                    // Handle other actual network errors
                    return;
                }

                if (bytes_transferred > 0) {
                    process_packet(bytes_transferred);
                }

                // Immediately resume listening for other clients
                start_receive();
            });
    }

    void decodeRemoteString(std::string remote_str, uint32_t& srcIP, uint16_t& srcPort) {
        auto pos = remote_str.find(":", 0);
        if (pos != std::string::npos) {
            srcIP = inet_addr(remote_str.substr(0, pos).c_str());
            try {
                unsigned long parsed = std::stoul(remote_str.substr(pos + 1));

                if (parsed > UINT16_MAX) {
                    throw std::out_of_range("Value exceeds 16-bit unsigned range");
                }

                srcPort = static_cast<uint16_t>(parsed);
            }
            catch (const std::invalid_argument& e) {
                std::cout << "Error: Not a valid number.\n";
                srcPort = 0xFFFF;
            }
            catch (const std::out_of_range& e) {
                std::cout << "Error: Out of range.\n";
                srcPort = 0xFFFE;
            }
        }
    }

    void process_packet(std::size_t length) {
        Watchdog& watchdog = Watchdog::GetInstance();
        watchdog.CheckIn(); // Reset watchdog timer on packet receipt
        std::ostringstream oss;
        oss << remote_endpoint_;
        std::string remote_str = oss.str();
        std::string remote_str_log = remote_str + ":" + std::to_string(port);
        std::string message = "";

        // Try to insert a new connection if not present
        auto it = KnownClientConnections.find(remote_endpoint_);
        if (it == KnownClientConnections.end()) {
            KnownClientConnections[remote_endpoint_] = test_udp_connection(remote_str_log);
            KnownClientConnections[remote_endpoint_].connection_time = std::chrono::system_clock::now();
            message = std::string(reinterpret_cast<const char*>(recv_buffer_.data()), length);
            LOG(LoggerVerbosity::INFO, "New Client Connected " + remote_str_log);
            uint16_t srcPort = 0;
            uint32_t srcIP = 0;
            decodeRemoteString(remote_str, srcIP, srcPort);
            KnownClientConnections[remote_endpoint_].srcIP = srcIP;
            KnownClientConnections[remote_endpoint_].srcPort = srcPort;

            auto result = message.compare(0, 12, "PACKET_MODE:");
            if (result == 0) {
                LOG(LoggerVerbosity::INFO, "New Client start with 'PACKET_MODE:'");
                KnownClientConnections[remote_endpoint_].validPacketStream = true;
            } else {
                LOG(LoggerVerbosity::ERR, "New Client did not start with 'PACKET_MODE:'");
            }
            LOG(LoggerVerbosity::INFO, remote_str_log);
            return;
        } else {
            // Known Connection

        }

        if (KnownClientConnections[remote_endpoint_].validPacketStream) {
            KnownClientConnections[remote_endpoint_].StatsRxPackets.addValue(length);

            // Extract Test Header
            auto testHdr = std::make_shared<PacketHeaderStripeTest>(recv_buffer_);
            length -= testHdr->Size();
            recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + testHdr->Size());

            LOG(LoggerVerbosity::INFO, remote_str_log
                + " - PM: Received Packet:"
                + " length=" + std::to_string(length)
                + " " + testHdr->to_string()
            );

            // Verify data
            bool packet_error = false;
            if (KnownClientConnections[remote_endpoint_].expected_sequence != testHdr->sequence_num) {
                KnownClientConnections[remote_endpoint_].StatOutOfSequence++;
            } else {
                KnownClientConnections[remote_endpoint_].expected_sequence++;
            }
            auto [it, insert_seq] = KnownClientConnections[remote_endpoint_].rx_seq_nums.emplace(testHdr->sequence_num, true);
            if (!insert_seq) {
                KnownClientConnections[remote_endpoint_].StatDuplicateSeqNum++;
            }
            if (length != testHdr->length) {
                LOG(LoggerVerbosity::ERR, "Data length and leng mismatch: "
                    "length=" + std::to_string(length)
                    + ", tstHdr.length=" + std::to_string(testHdr->length)
                );
                KnownClientConnections[remote_endpoint_].StatBadLength++;
                packet_error = true;
            }

            uint16_t err_count = 0;
            for (size_t i = 0; i < length; ++i) {
                if (recv_buffer_[i] != testHdr->pattern) {
                    err_count++;
                }
            }
            if (err_count) {
                LOG(LoggerVerbosity::ERR, "Data bytes do not match pattern: "
                    "length=" + std::to_string(length)
                    + ", pattern=" + std::to_string(testHdr->pattern)
                    + ", error_count=" + std::to_string(err_count)
                );
                KnownClientConnections[remote_endpoint_].StatBadPacketData++;
                packet_error = true;
            }
            if (packet_error) {
                KnownClientConnections[remote_endpoint_].StatsBadPackets.addValue(length);
            }
        } else {
            std::cout << "Unhandled Mode for endpoint " << remote_str_log << std::endl;
        }
    }
};


int main(int argc, char* argv[]) {
    LoggerVerbosity verbosity = LoggerVerbosity::ERR;
	double WatchdogTimeout = 360;
    std::string LogFile;
    std::string ServerPort = "8080";
    std::string SourcePort = "100";
    std::string ServerIP = "127.0.0.1";
    uint8_t data_pattern = 0xA5;
    uint32_t num_packets = 0;
    double bit_rate;
    uint16_t min_pkt_size = 64;
    uint16_t max_pkt_size = 1416;

    LOG_INST.SetTimeStamping(false);

    CommandLineParser CLP;
    CLP.AddCommand({
        CLP_Command("version", "Show version information", [](const std::string& argument) {
            show_version(true);
            exit(0);
        }, "", typeid(void)),
        CLP_Command("verbosity,v", "Set logging verbosity level: " + LOG_INST.GetLogLevelNames(), [&verbosity](const std::string& argument) {
            std::string uarg = argument;
            std::transform(uarg.begin(), uarg.end(), uarg.begin(), ::toupper);
            verbosity = magic_enum::enum_cast<LoggerVerbosity>(uarg).value_or(LoggerVerbosity::NOTSET);
            if (verbosity == LoggerVerbosity::NOTSET) {
                try {
                    int v_int = std::stoi(argument);
                    verbosity = static_cast<LoggerVerbosity>(v_int);
                }
                catch (const std::exception& e) {
                    std::cerr << "\nInvalid verbosity level: " << argument << ". Setting to NOTSET.\n";
                }
			}
            LOG_INST.verbosity = verbosity;
     
            std::cout << "\nSetting Logger Verbosity to " << std::string(magic_enum::enum_name(verbosity))
                << " (" << static_cast<int>(verbosity) << ")"
				<< " argument=" << argument
                << "\n";
        }, "INFO", typeid(std::string)),
        CLP_Command("timestamping, T", "Adds timestamps to log entries",
            [](const std::string& argument) {
            LOG_INST.SetTimeStamping(true);
        }, "", typeid(void)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "testS.log", typeid(std::string)),
        CLP_Command("server_port, p", "Specifies UDP Destination Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "7000", typeid(std::string)),
        CLP_Command("watchdog,w", "Watchdog timeout in seconds", [&WatchdogTimeout](const std::string& argument) {
            try {
                WatchdogTimeout = std::stod(argument);
            }
            catch (const std::exception& e) {
                std::cerr << "\nInvalid watchdog timeout value: " << argument << ". Setting to default 360 seconds.\n";
                WatchdogTimeout = 360;
                exit(10);
            }
        }, "360", typeid(double)),

        });

    LOG(LoggerVerbosity::DEBUG, show_version());
    LOG(LoggerVerbosity::DEBUG, "*** Setting DEFAULT command line arguments...***");
    CLP.SetDefaultValues();
    LOG(LoggerVerbosity::DEBUG, "*** Parsing command line arguments...***");
    CLP.ProcessArguments(argc, argv);

    LOG_INST.SetLogFile(LogFile);
    LOG(verbosity, "Starting log\n");

    // Start WATCHDOG thread to monitor and adjust FEC stripes
    ThreadManager& TM = ThreadManager::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();
    watchdog.SetTimeout(WatchdogTimeout);
    watchdog.SetOnTimeoutForceExit(false); // Force exit on watchdog timeout
    watchdog.SetOnTimeoutCallback([]() {
        // Perform any necessary cleanup here
        LOG(LoggerVerbosity::CRITICAL, "Watchdog timeout callback invoked. Performing cleanup before exit.");
        exit(100);
        });
    TM.StartThread("WatchdogMonitor", Watchdog::monitor_thread);

    // Start CLI input thread

    auto& CMP = CliMenuProcessor::GetInstance();
    CMP.SetPrompt("TestStripeSvr> ");
    CMP.AddSubMenu(LOG_INST.cli_menu);
    CMP.AddSubMenu(Watchdog::GetInstance().cli_menu);
    TM.StartThread("CLIInput", CliMenuProcessor::GetUserInput_thread);


    try {
        boost::asio::io_context io_context;
        short port = static_cast<short>(std::stoi(ServerPort));
        TestUdpServer server(io_context, port);
        CMP.AddSubMenu(server.cli_menu);

        std::cout << "\nUDP Server running on port " << ServerPort << "..." << std::endl;
        watchdog.SetOnTimeoutCallback([&server]() {
            LOG(LoggerVerbosity::CRITICAL, "Watchdog timeout callback invoked. Performing cleanup before exit.");
            // Perform any necessary cleanup here
            server.StopServer(); // Explicitly call destructor to clean up resources
            });
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }


    // Wait for threads to join
    LOG(LoggerVerbosity::INFO, "Waiting threads to join...");
    watchdog.StopMonitoring();
    TM.WaitAllThreads(); // Wait for all threads to finish

    return 0;
}

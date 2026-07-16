/*------------------------------------------------------------------
 * test-Client.cpp
 * 
 * Boost based UDP Client to generate test traffic streams
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

#include "commandLineParser.h"
#include "watchdog.h"
#include "PacketHeader.h"
#include "bUDP.h"
#include "bUDP-Client.h"
#include "statistics.h"

std::string show_version(bool use_cout = false) {
    std::stringstream ss;
    ss << "\n\tTest Client (boost) Version: " << VERSION << "." << GIT_HASH << std::endl;
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
        }, "testC.log", typeid(std::string)),
        CLP_Command("server_port, p", "Specifies UDP Destination Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "7000", typeid(std::string)),
        CLP_Command("source_port, a", "Specifies UDP Source Port number of this client", [&SourcePort](const std::string& argument) {
            SourcePort = argument;
        }, "100", typeid(std::string)),
        CLP_Command("server_ip, s", "Specifies IP address of server", [&ServerIP](const std::string& argument) {
            ServerIP = argument;
        }, "127.0.0.1", typeid(std::string)),
        CLP_Command("pattern, x", "Specifies data pattern to be used in message", [&data_pattern](const std::string& argument) {
            try {
                auto val = std::stoi(argument, nullptr, 0);
                if (val < 0 || val > std::numeric_limits<uint8_t>::max()) {
                    std::cerr << "\nInvalid data pattern: value is not a valid 8-bit value: must be between 0 and 225: pat=" << argument << "\n";
                    exit(10);
                }
                data_pattern = static_cast<uint8_t>(val);
            }
            catch (const std::exception& e) {
                std::cerr << "\nInvalid data pattern: " << argument << ". Must be 8-bit number.\n";
                exit(10);
            }
        }, "0x5A", typeid(uint8_t)),
        CLP_Command("num_pkts, n", "Number of packets to send (0 is infinite)", [&num_packets](const std::string& argument) {
            try {
                num_packets = std::stoi(argument);
                if (num_packets < 0) {
                    std::cerr << "\nInvalid number of packets cannot be negative: " << argument << ".\n";
                    exit(10);
                }
            }
            catch (const std::exception& e) {
                std::cerr << "\nInvalid number of packets: " << argument << ".\n";
                exit(10);
            }
        }, "10", typeid(uint32_t)),
        CLP_Command("bit_rate, b", "Bit Rate to send packets at in bps", [&bit_rate](const std::string& argument) {
            try {
                bit_rate = std::stod(argument);
            }
            catch (const std::exception& e) {
                std::cerr << "\nInvalid bit Rate: " << argument << ".\n";
                exit(10);
            }
            if (bit_rate < 0) {
                std::cerr << "\nInvalid bit Rate: Rate cannot be negative!! rate=" << bit_rate << ".\n";
                exit(10);
            }
        }, "10e3", typeid(double)),
        CLP_Command("min_pkt_size, c", "Minimum Packet size in bytes", [&min_pkt_size](const std::string& argument) {
            try {
                min_pkt_size = std::stoi(argument);
            }
            catch (const std::exception& e) {
                std::cerr << "\nInvalid Minimum packet size: " << argument << ".\n";
                exit(10);
            }
            if (min_pkt_size < 64 || min_pkt_size > 1360) {
                std::cerr << "\nMinimum packet size outside valid range (64-1360): " << min_pkt_size << ".\n";
                exit(1);
            }
        }, "64", typeid(uint32_t)),
        CLP_Command("max_pkt_size, d", "Maximum Packet size in bytes", [&max_pkt_size](const std::string& argument) {
            try {
                max_pkt_size = std::stoi(argument);
            }
            catch (const std::exception& e) {
                std::cerr << "\nInvalid Maximum packet size: " << argument << ".\n";
                exit(10);
            }
            if (max_pkt_size < 64 || max_pkt_size > 1360) {
                std::cerr << "\nMaximum packet size outside valid range (64-1360): " << max_pkt_size << ".\n";
                exit(2);
            }
        }, "1360", typeid(uint32_t)),
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


    boost::asio::io_context io_context;
    UdpClient uclient(io_context, ServerIP, SourcePort, ServerPort, UdpSendMode::PACKET);
    uclient.StartPacketMode();

    if (min_pkt_size > max_pkt_size) {
        std::cerr << "Fatal Error: configured packet sizes are incorrect: min(" << min_pkt_size << ") is greater than max(" << max_pkt_size << ")!!";
        exit(100);
    }
    std::ostringstream pat;
    pat << "0x" << std::hex << std::uppercase << (int) data_pattern;
    LOG(LoggerVerbosity::INFO, "Starting Packet generator with these parameters:"
        " bit_rate=" + std::to_string(bit_rate)
        + ", num_packets=" + std::to_string(num_packets)
        + ", pattern=" + pat.str()
        + ", min=" + std::to_string(min_pkt_size)
        + ", max=" + std::to_string(max_pkt_size)
    );

    // Setup for main loop
    // Create a random device and generator

    std::random_device rd;  // Non-deterministic seed
    std::mt19937 gen(rd()); // Mersenne Twister engine


    // Uniform distribution in the given range
    std::uniform_int_distribution<int> pkt_dist(min_pkt_size, max_pkt_size);

    // Main Spin Loop
    StatisticsRTM<uint64_t> StatsTxPackets("TX", nullptr);

    uint32_t sequence = 0;
    while ((num_packets == 0 || sequence < num_packets) && !TM.force_stop.load()) {
        uint16_t length = pkt_dist(gen);
        auto testPktHdr = std::make_shared<PacketHeaderStripeTest>();
        testPktHdr->sequence_num = sequence++;
        testPktHdr->length = length;
        testPktHdr->pattern = data_pattern;
        PacketHeaders headers;

        auto time_to_tx = length * 8 / bit_rate;
        LOG(LoggerVerbosity::INFO, "Sending: " + testPktHdr->to_string() + " tx_time=" + std::to_string(time_to_tx));

        headers.AddHeader(testPktHdr);
        std::vector<uint8_t> data(length + testPktHdr->Size(), data_pattern);
        uclient.SendPacket(headers, data, length);
        StatsTxPackets.addValue(length);

        // Sleep for time to tx packet
        auto duration = std::chrono::duration<double>(time_to_tx);
        std::this_thread::sleep_for(duration);
        watchdog.CheckIn();
    }

    std::cout << "\n\nCompleted sending ALL packets: last_seq=" << std::to_string(sequence) << "\n";
    std::cout << "\n" << StatsTxPackets.ToString();

    // Wait for threads to join
    LOG(LoggerVerbosity::INFO, "Waiting threads to join...");
    watchdog.StopMonitoring();
    TM.WaitAllThreads(); // Wait for all threads to finish

    return 0;
}

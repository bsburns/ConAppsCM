/*------------------------------------------------------------------
 * SXTP_RX_main.cpp
 *
 * Space X Thruput Test Receiver main program
 * 
 * Receives UDP streams from SpaceX Thruput Test Transmitter
 *
 * August 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include "logger.h"

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <thread>
#include <boost/asio.hpp>

#include "SXTP_common.h"
#include "commandLineParser.h"
#include "watchdog.h"
#include "threadManager.h"
#include "cli/CLI.h"
#include "statistics.h"
#include "PacketHeader.h"


std::string OutDir = "";

std::string show_version(bool use_cout = false) {
    std::stringstream ss;
    ss << "\n\tSaturnX Laser Comm Terminal emulation Version: " << VERSION << "." << GIT_HASH << std::endl;
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

using boost::asio::ip::udp;
using namespace my_logger;
namespace fs = std::filesystem;


struct CheckerConfiguration {
    uint32_t ReorderWindow = 500;
    bool no_sequence_num_check = false;
    bool no_payload_check = false;
	bool save_missing_sequence_numbers = false;
	bool make_output_filename_unique = true;
};


class MissingSequenceTrackerEntry {
public:
    uint64_t sequence_number;
    std::chrono::steady_clock::time_point first_missing_time;
    std::chrono::steady_clock::time_point last_missing_time;
    uint32_t missing_count = 0;
    MissingSequenceTrackerEntry(uint64_t seq_num, std::chrono::steady_clock::time_point tp)
        : sequence_number(seq_num)
        , first_missing_time(tp)
        , last_missing_time(std::chrono::steady_clock::now())
        , missing_count(1) {
    }
    int update_missing(uint64_t seq_num, std::chrono::steady_clock::time_point tp) {
        if (seq_num != sequence_number+missing_count) {
            return 1;
		}
        last_missing_time = tp;
        ++missing_count;
		return 0;
    }
};

class MissingSequenceTracker {
public:
    std::string streamName;
    CheckerConfiguration* chkrCfg = nullptr;
    uint64_t expected_sequence = 0;
    std::list<MissingSequenceTrackerEntry> missing_entries;
    std::map<uint64_t, std::chrono::steady_clock::time_point> missing_rx_seq_nums; // key is sequence numbers
	std::chrono::steady_clock::time_point last_packet_rx_time = std::chrono::steady_clock::now();
    std::optional<std::ofstream> outputFile;

	// Statistics
    StatisticsBasic<uint64_t> StatsConsecutiveCount;
    StatisticsBasic<uint64_t> StatsConsecutiveTime;
    uint64_t StatOutOfSequence = 0;
    uint64_t StatDuplicateSeqNum = 0;
    uint64_t ReorderedSequenceCount = 0;
    uint64_t OutsideReorderWindowCount = 0;

    // Default constructor
    MissingSequenceTracker() {}

    MissingSequenceTracker(std::string name, CheckerConfiguration* checkerConfig_) 
		: streamName(name)
        , chkrCfg(checkerConfig_) {}

    void check_sequence(uint64_t seq_num, std::chrono::steady_clock::time_point tp) {
        if (missing_rx_seq_nums.size() > 0) {
            // Check if this sequence number is in the missing list
            auto snum = missing_rx_seq_nums.begin()->first;
            if (snum < seq_num - chkrCfg->ReorderWindow) {
                // Missing Sequence number is outside of reorde window
                auto rx_time = missing_rx_seq_nums.begin()->second;
                update_missing(snum, rx_time);
                missing_rx_seq_nums.erase(missing_rx_seq_nums.begin());
            }
        }
        if (expected_sequence != seq_num) {
            StatOutOfSequence++;

            auto it = missing_rx_seq_nums.find(seq_num);
            if (it != missing_rx_seq_nums.end()) {
                // Sequence # is already in missing list so rx out of oreder
                LOG(LoggerVerbosity::ERR, "[Seq=" + std::to_string(seq_num)
                    + "]: Out of sequence: expected=" + std::to_string(expected_sequence)
                    + ", received=" + std::to_string(seq_num));
                missing_rx_seq_nums.erase(seq_num);
                ReorderedSequenceCount++;
            }
            else {
                // We skipped past expected, so add to missing list and set expected
                auto last_rx_time = last_packet_rx_time;
                for (uint64_t seq = expected_sequence; seq < seq_num; ++seq) {
                    missing_rx_seq_nums[seq] = last_packet_rx_time;
                }
                std::prev(missing_rx_seq_nums.end())->second = tp; // set last in sequence to current time
                expected_sequence = seq_num + 1;
            }
        }
        else {
            expected_sequence++;
        }
        last_packet_rx_time = tp;
	}

    void closeOutputFile() {
        if (outputFile && outputFile->is_open()) {
			outputFile->flush();
            outputFile->close();
            outputFile.reset();
        }
	}

    void update_missing(uint64_t seq_num, std::chrono::steady_clock::time_point tp) {
        if (missing_entries.empty()) {
            missing_entries.emplace_back(seq_num, tp);
        } else {
            auto& last_entry = missing_entries.back();
            if (last_entry.update_missing(seq_num, tp) != 0) {
				// Not a continuation of the last missing sequence, so create a new entry 
                missing_entries.emplace_back(seq_num, tp);

				StatsConsecutiveCount.addValue(last_entry.missing_count);
                std::chrono::duration<double> missing_duration = last_entry.last_missing_time - last_entry.first_missing_time;
				StatsConsecutiveTime.addValue(missing_duration.count());
                if (chkrCfg->save_missing_sequence_numbers) { // save completed entry to file
                    if (!outputFile) {
                        fs::path outPath = OutDir;
                        if (chkrCfg->make_output_filename_unique) {
                            // Convert to local time or keep as UTC using current_zone()
                            auto const now = std::chrono::system_clock::now();
                            std::time_t time_now = std::chrono::system_clock::to_time_t(now);

                            // Convert to local time structure safely
                            std::tm local_tm = *std::localtime(&time_now);

                            // Stream format into a string
                            std::stringstream ss;
                            ss << std::put_time(&local_tm, "%Y%m%d_%H%M%S");
                            //auto const local_time = std::chrono::current_zone()->to_local(now);
                            //std::string timestamp = std::format("%Y%m%d_%H%M%S", local_time);

                            std::string fn = streamName;
                            std::replace(fn.begin(), fn.end(), '.', '_'); // Replace periods with underscores    
                            std::replace(fn.begin(), fn.end(), ':', '_'); // Replace colons with underscores    

                            fn = ss.str() + "-STRM" + fn;
                            fn += "_MissingSeq.csv";
                            outPath /= fn;
                        } else {
                            outPath /= "MissingSeq.csv";
						}
						LOG(LoggerVerbosity::CRITICAL, "Saving missing sequence numbers to file: " + outPath.string());
                        //outputFile.emplace(".output\\test.csv", std::ios::out | std::ios::trunc);
                        outputFile.emplace(outPath.string(), std::ios::out | std::ios::trunc);
                        *outputFile << "StartSeq, Count, Elapsed_Time_ns\n";
                    }
                    auto missing_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(last_entry.last_missing_time - last_entry.first_missing_time);
                    *outputFile << last_entry.sequence_number << ", " << last_entry.missing_count << ", " << missing_duration.count() << "\n";
                    outputFile->flush();
                }
            }
        }
	}

    std::string BriefStats() const {
        std::stringstream ss;
        uint64_t StatOutOfSequence = 0;
        uint64_t StatDuplicateSeqNum = 0;
        uint64_t ReorderedSequenceCount = 0;
        uint64_t OutsideReorderWindowCount = 0;

        ss << "SeqStats: {";
        ss << " OOS=" << StatOutOfSequence;
        ss << " | REORDERED=" << ReorderedSequenceCount;
		ss << " | OUTSIDE_REORDER_WINDOW=" << OutsideReorderWindowCount;
		ss << " | DUPLICATE_SEQ=" << StatDuplicateSeqNum;
        ss << " | MissingSequenceConsecutive: Groups=" << missing_entries.size();
        if (missing_entries.size() > 0) {
            ss << " | MinGrp=" << StatsConsecutiveCount.min();
            ss << " | AvgGrp=" << StatsConsecutiveCount.mean();
            ss << " | MaxGrp=" << StatsConsecutiveCount.max();
            ss << " | MinElapseTime=" << to_engineering(StatsConsecutiveTime.min());
            ss << " | AvgElapseTime=" << to_engineering(StatsConsecutiveTime.mean());
            ss << " | MaxElapseTime=" << to_engineering(StatsConsecutiveTime.max());
        }
        ss << " | InWindowMissingSeqNum=" << missing_rx_seq_nums.size()
            << " (" << missing_rx_seq_nums.size() * 100.0 / expected_sequence << "%)";
        ss << "}";
        return ss.str();
    }
};


class test_udp_connection {
public:
    std::string name;
    CheckerConfiguration* chkrCfg;
    bool validPacketStream = false;
    uint16_t srcPort = 0;
    uint32_t srcIP = 0;
    std::map<uint16_t, std::vector<uint8_t>> file_chunks; // For storing file chunks if in SEND_FILE mode
    std::string file_name; // Store the file name being sent by the client
    std::chrono::system_clock::time_point connection_time; // Track when the connection was established
    std::chrono::system_clock::time_point last_output_time = std::chrono::system_clock::now(); // Track when the connection was established
    std::chrono::steady_clock::time_point last_packet_rx_time = std::chrono::steady_clock::now();
	MissingSequenceTracker missing_sequence_tracker; // Track missing sequence numbers

    // Statistics
    StatisticsRTM<uint64_t> StatsRxPackets;
    StatisticsRTM<uint64_t> StatsBadPackets;
    StatisticsBasic<double> StatsLatency;
    uint64_t StatBadPacketData = 0;
    uint64_t StatBadLength = 0;

    test_udp_connection() {}
    test_udp_connection(std::string conn_name_, CheckerConfiguration* checkerConfig_)
        : name(conn_name_)
        , chkrCfg(checkerConfig_)
		, missing_sequence_tracker(conn_name_, checkerConfig_)
        , StatsRxPackets(conn_name_ + ":RxPkts", nullptr)
        , StatsBadPackets(conn_name_ + ":BadPkts", nullptr)
		, StatsLatency(conn_name_ + ":Latency", nullptr)
    {
		last_output_time = std::chrono::system_clock::now();
    }

    int process_packet(const std::vector<uint8_t>& receive_buffer_, std::size_t length,
        std::chrono::system_clock::time_point now, std::chrono::steady_clock::time_point nows) 
    {
        int rc = 0;
        if (validPacketStream) {
            StatsRxPackets.addValue(length);

            // Extract Test Header
            auto testHdr = std::make_shared<PacketHeaderStripeTest>(receive_buffer_);
            length -= testHdr->Size();

            LOG(LoggerVerbosity::INFO, name
                + " - PM: Received Packet:"
                + " length=" + std::to_string(length)
                + " " + testHdr->to_string()
            );

            auto duration = std::chrono::microseconds(testHdr->timestamp);
            std::chrono::time_point<std::chrono::system_clock> timestamp(duration);
            std::chrono::duration<double> latency = now - timestamp;
            StatsLatency.addValue(latency.count());

            // Verify data
            bool packet_error = false;
            if (!chkrCfg->no_sequence_num_check) {
				missing_sequence_tracker.check_sequence(testHdr->sequence_num, nows);
			} // end sequence check

            if (length != testHdr->length) {
                LOG(LoggerVerbosity::ERR, "[Seq=" + std::to_string(testHdr->sequence_num)
                    + "]: Data length and leng mismatch: "
                    "length=" + std::to_string(length)
                    + ", tstHdr.length=" + std::to_string(testHdr->length)
                    + "  Stats=" + BriefStats()
                );
                StatBadLength++;
                packet_error = true;
            }

            if (!chkrCfg->no_payload_check) {
                uint16_t err_count = 0;
                for (size_t i = testHdr->Size(); i < testHdr->length + testHdr->Size(); ++i) {
                    if (receive_buffer_[i] != testHdr->pattern) {
                        err_count++;
                    }
                }
                if (err_count) {
                    LOG(LoggerVerbosity::ERR, "Data bytes do not match pattern: "
                        "length=" + std::to_string(length)
                        + ", pattern=" + std::to_string(testHdr->pattern)
                        + ", error_count=" + std::to_string(err_count)
                    );
                    StatBadPacketData++;
                    packet_error = true;
                }
            }
            if (packet_error) {
                StatsBadPackets.addValue(length);
            }

            auto curr_time = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_output = curr_time - last_output_time;

            if (elapsed_output.count() > 1.0) {
                std::cout << "\nPeriodic: "
                    << " SEQ=" << testHdr->sequence_num << " "
                    << BriefStats();
                last_output_time = now;
            }
            auto cmd = testHdr->command;
            if (cmd == (uint8_t)PacketHeaderStripeTest::Command::STOP) {
                std::cout << "\n\nReceived STOP command from client: " << name 
                    << "\nFINAL STATS:\n" << BriefStats()
                    << "\n\nLatency: min=" << to_engineering(StatsLatency.min())
                    << " mean=" << to_engineering(StatsLatency.mean())
                    << " max=" << to_engineering(StatsLatency.max())
                    << std::endl;
				rc = 1; // Indicate to the server that this connection should be closed
				missing_sequence_tracker.closeOutputFile();
            }
            last_packet_rx_time = nows;
        } else {
            std::cout << "Unhandled Mode for endpoint " << name << std::endl;
        }
        return rc;
    }

    std::string BriefStats() const {
        std::stringstream ss;
        ss << name;
        ss << ": RXP=" << StatsRxPackets.count();
        ss << " | RXpps=" << to_engineering(StatsRxPackets.periodCountRate());
        ss << " | RXB=" << StatsRxPackets.sum();
        ss << " | RXbps=" << to_engineering(StatsRxPackets.periodUnitRate() * 8);
        ss << " | BADP=" << StatsBadPackets.count();
		ss << " | LATus=" << to_engineering(StatsLatency.mean());
		ss << " | " << missing_sequence_tracker.BriefStats();

        return ss.str();
    }
};


class TestUdpServer {
public:
    CheckerConfiguration* chkrCfg;
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
    TestUdpServer(boost::asio::io_context& io_context, short port_, CheckerConfiguration* checkerCfg_)
        : socket_(io_context, udp::endpoint(udp::v4(), port_))
        , port(port_)
		, chkrCfg(checkerCfg_)
    {
        // Increase OS receive buffer to 16 Megabytes (Default is often 256KB or less)
        boost::asio::socket_base::receive_buffer_size option(128 * 1024 * 1024);
        socket_.set_option(option);
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
    std::vector<uint8_t> recv_buffer_{ std::vector<uint8_t>(4*1024, 0) };
    std::map<udp::endpoint, test_udp_connection> KnownClientConnections; // Map of client endpoints to their connection info
    StatisticsBasic<double> StatsProcessingTime;

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
        int rc = 0;
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::chrono::steady_clock::time_point nows = std::chrono::steady_clock::now();
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
            KnownClientConnections[remote_endpoint_] = test_udp_connection(remote_str_log, chkrCfg);
            KnownClientConnections[remote_endpoint_].connection_time = std::chrono::system_clock::now();
            message = std::string(reinterpret_cast<const char*>(recv_buffer_.data()), length);
            LOG(LoggerVerbosity::CRITICAL, "New Client Connected: " + remote_str_log);
            uint16_t srcPort = 0;
            uint32_t srcIP = 0;
            decodeRemoteString(remote_str, srcIP, srcPort);
            KnownClientConnections[remote_endpoint_].srcIP = srcIP;
            KnownClientConnections[remote_endpoint_].srcPort = srcPort;

            if (message.compare(0, 12, "PACKET_MODE:") == 0) {
                LOG(LoggerVerbosity::INFO, "New Client start with 'PACKET_MODE:'");
                KnownClientConnections[remote_endpoint_].validPacketStream = true;
            }
            else if (message.compare(0, 10, "SEND_FILE:") == 0) {
                LOG(LoggerVerbosity::INFO, "New Client start with 'SEND_FILE:'");
                KnownClientConnections[remote_endpoint_].validPacketStream = false;
                KnownClientConnections[remote_endpoint_].file_name = message.substr(10);
            } else if (message.compare(0, 15, "TEST_DATA_MODE:") == 0) {
                LOG(LoggerVerbosity::CRITICAL, "New Client start with 'TEST_DATA_MODE:'");
                KnownClientConnections[remote_endpoint_].validPacketStream = true;
				SendMessage(remote_endpoint_, "TEST_DATA_MODE_ACK:");
            } else {
                LOG(LoggerVerbosity::ERR, "New Client did not start with known mode: msg="+message);
            }
            LOG(LoggerVerbosity::INFO, remote_str_log);
            return;
        } else {
            // Known Connection

        }

        rc = KnownClientConnections[remote_endpoint_].process_packet(recv_buffer_, length, now, nows);

        std::chrono::steady_clock::time_point endt = std::chrono::steady_clock::now();
        StatsProcessingTime.addValue(std::chrono::duration<double>(endt - nows).count());    
        if (rc == 1) {
            // Client sent STOP command, so remove from known connections
            std::cout << "\n\nPacket Processing Time: min=" << to_engineering(StatsProcessingTime.min())
                << " mean=" << to_engineering(StatsProcessingTime.mean())
                << " max=" << to_engineering(StatsProcessingTime.max())
				<< std::endl;
		}
    }
    
    void SendMessage(udp::endpoint remote_endpoint, std::string text) {
        boost::system::error_code ec;
        // asynchronous send back to the sender stored in remote_endpoint_
        socket_.send_to(boost::asio::buffer(text), remote_endpoint_);
    }
};


int main(int argc, char* argv[]) {
    CheckerConfiguration CheckerCfg;
    my_logger::LoggerVerbosity verbosity = my_logger::LoggerVerbosity::ERR;
    double WatchdogTimeout = 360;
    std::string LogFile;
    std::string ServerPort = "8080";

	LOG_INST.verbosity = verbosity;
    LOG_INST.SetTimeStamping(false);

    CommandLineParser CLP;
    CLP.AddCommand({
        CLP_Command("version", "Show version information", [](const std::string& argument) {
            std::cout << "\nVERSION INFO:";
            show_version(true);
            exit(0);
        }, "", typeid(void)),
        CLP_Command("verbosity,v", "Set logging verbosity level: " +  LOG_INST.GetLogLevelNames() , [&verbosity](const std::string& argument) {
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
        }, "SXTP_RX.log", typeid(std::string)),
        CLP_Command("timestamping, T", "Adds timestamps to log entries",
            [](const std::string& argument) {
            LOG_INST.SetTimeStamping(true);
        }, "", typeid(void)),
        CLP_Command("server_port, p", "Specifies UDP Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "12000", typeid(std::string)),
        CLP_Command("reorder_window_size, n", "Specifies the number of packets in reorder window", [&CheckerCfg](const std::string& argument) {
            std::stringstream ss(argument);
            double rws_double;
            if (!(ss >> rws_double && ss.eof())) {
                std::cerr << "Invalid number packets: " << argument << "\n";
                exit(10);
            }
            CheckerCfg.ReorderWindow = static_cast<uint32_t>(rws_double);
        }, "500", typeid(uint32_t)),
        CLP_Command("no_sequence_check, a", "Disable sequence number checks", [&CheckerCfg](const std::string& argument) {
            CheckerCfg.no_sequence_num_check = true;
        }, "", typeid(void)),
        CLP_Command("no_payload_check, b", "Disable payload checks", [&CheckerCfg](const std::string& argument) {
            CheckerCfg.no_payload_check = true;
        }, "", typeid(void)),
        CLP_Command("save_missing_sequence_numbers, c", "Save missing sequence numbers to a file", [&CheckerCfg](const std::string& argument) {
            CheckerCfg.save_missing_sequence_numbers = true;
        }, "", typeid(void)),
        CLP_Command("no_unique_output_filename, z", "Do not make Output filename unique", [&CheckerCfg](const std::string& argument) {
            CheckerCfg.make_output_filename_unique = false;
        }, "", typeid(void)),
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
    auto& CMP = CliMenuProcessor::GetInstance(".SXTP_RX.command_history");
    CMP.SetPrompt("SXTP_RX> ");
    CMP.AddSubMenu(LOG_INST.cli_menu);
    CMP.AddSubMenu(Watchdog::GetInstance().cli_menu);
    TM.StartThread("CLIInput", CliMenuProcessor::GetUserInput_thread);

    // Create LCT engine
    //auto engine = LCT_Engine();
    //engine.Start();
    try {
        boost::asio::io_context io_context;
		short port = static_cast<short>(std::stoi(ServerPort));
        TestUdpServer server(io_context, port, &CheckerCfg);
        std::cout << "\n\nUDP server running on port "<< port<<"...\n";
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Server error: " << e.what() << "\n";
    }

    // Wait for threads to join
    LOG(LoggerVerbosity::INFO, "Waiting threads to join...");
    watchdog.StopMonitoring(); 
    TM.WaitAllThreads(); // Wait for all threads to finish
    return 0;
}

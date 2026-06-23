#include "logger.h"

#include <iostream>
#include <string>
#include <set>
#include <memory>
#include <boost/asio.hpp>
//#include <boost/process/v2/process.hpp>
#include <boost/process/v1.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <map>
#include <filesystem>
#include <cstdlib>
#include "bUDP.h"
#include "bStriperConfig.h"
#include "commandLineParser.h"

using boost::asio::ip::udp;
using namespace boost::interprocess;
namespace bp = boost::process;


std::string OutDir = "";
bool StriperEnable = false;
AllStriperConfig StriperConfig; // Global configuration for the striper

void show_version() {
    std::cout << "UDP Server (boost) Version: " << VERSION << "." << GIT_HASH << std::endl;
    std::cout << "Build Date: " << __DATE__ << std::endl;
    std::cout << "Build Time: " << __TIME__ << std::endl;
#ifdef _WIN32
    std::cout << "MSVC Version: " << _MSC_FULL_VER << std::endl;
#else
    std::cout << "Compiler: " << __VERSION__ << std::endl;
#endif
    std::cout << "Build Machine: " << BUILD_MACHINE << std::endl;
}

using namespace my_logger;
namespace fs = std::filesystem;


class udp_connection {
public:
	UdpSendMode mode = UdpSendMode::NOTSET;
	std::map<uint16_t, std::vector<uint8_t>> file_chunks; // For storing file chunks if in SEND_FILE mode
	std::string file_name; // Store the file name being sent by the client
	std::chrono::system_clock::time_point connection_time; // Track when the connection was established

    udp_connection() {}
};

class UdpServer {
public:
    // Bind to the given port on all available network interfaces
    UdpServer(boost::asio::io_context& io_context, short port)
        : socket_(io_context, udp::endpoint(udp::v4(), port)) {
        start_receive();
    }

private:
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 4096> recv_buffer_;
    std::set<udp::endpoint> known_clients_; // Active client registry
	std::map<udp::endpoint, udp_connection> KnownClientConnections; // Map of client endpoints to their connection info

    void start_receive() {
        // Wait asynchronously for an incoming packet
        socket_.async_receive_from(
            boost::asio::buffer(recv_buffer_), remote_endpoint_,
            [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                if (!ec && bytes_transferred > 0) {
                    process_packet(bytes_transferred);
                }
                // Immediately resume listening for other clients
                start_receive();
            });
    }

    void process_packet(std::size_t length) {
        std::string message(recv_buffer_.data(), length);
        std::ostringstream oss;
        oss << remote_endpoint_;
		std::string remote_str = oss.str();

        // Try to insert a new connection if not present
        auto [it, inserted] = KnownClientConnections.emplace(remote_endpoint_, udp_connection{});
        if (inserted) {
            LOG(LoggerVerbosity::INFO, "New Client Connected " + remote_str);
            auto result = message.compare(0, 10, "FILE_MODE:");
			KnownClientConnections[remote_endpoint_].connection_time = std::chrono::system_clock::now();
            if (result == 0) {
				KnownClientConnections[remote_endpoint_].mode = UdpSendMode::SEND_FILE;
				KnownClientConnections[remote_endpoint_].file_chunks.clear();
				fs::path filePath(message.substr(10)); // Extract file name after "FILE_MODE:"
				
                KnownClientConnections[remote_endpoint_].file_name = filePath.filename().string(); 
				LOG(LoggerVerbosity::INFO, remote_str + ": file_name=" + KnownClientConnections[remote_endpoint_].file_name);
				return; // Don't process the file mode message further
            } else {
				KnownClientConnections[remote_endpoint_].mode = UdpSendMode::MESSAGE;
			}
            LOG(LoggerVerbosity::INFO, remote_str + ": mode = " +
                std::string(magic_enum::enum_name(KnownClientConnections[remote_endpoint_].mode)));
        }

        if (KnownClientConnections[remote_endpoint_].mode == UdpSendMode::MESSAGE) {
            LOG(LoggerVerbosity::INFO, remote_str +
                ":MM: Received Message: " + message);

            // Echo the packet back asynchronously to the sender
            auto response_msg = std::make_shared<std::string>("Echo: " + message);
            socket_.async_send_to(
                boost::asio::buffer(*response_msg), remote_endpoint_,
                [response_msg](boost::system::error_code /*ec*/, std::size_t /*bytes*/) {
                    // Shared pointer capture keeps data alive until transmission completes
                });
		}
        else if (KnownClientConnections[remote_endpoint_].mode == UdpSendMode::SEND_FILE) {
			// FILE MODE: Expecting file chunks in the format "CHUNK:<chunk_number>:<data>"
            std::vector<uint8_t> data(recv_buffer_.begin(), recv_buffer_.begin() + length);
            FileTransferHeader fh = FileTransferHeader::deserialize(data);
            LOG(LoggerVerbosity::INFO, remote_str +
                ":FM: Received " + std::to_string(length) + " bytes" +
				" chunkNumber=" + std::to_string(fh.chunkNumber) +
				" chunkSize=" + std::to_string(fh.chunkSize) 
            );
            if (fh.chunkSize > 0) {
                auto [cit, inserted] = 
                    KnownClientConnections[remote_endpoint_].file_chunks.emplace(
                        fh.chunkNumber,
                        std::vector<uint8_t>(data.begin() + fh.GetHeaderSizeBytes(), data.end())
                    );
            }
            else {
                // End of file
                LOG(LoggerVerbosity::INFO, remote_str +
                    ":FM: Received EOF");

				// Write the received file chunks to disk

                std::string output_file = OutDir + "/" + KnownClientConnections[remote_endpoint_].file_name;
                std::ofstream ofs(output_file, std::ios::binary);
                if (!ofs) {
                    LOG(LoggerVerbosity::CRITICAL, remote_str +
                        ":FM: Error opening output file: " + output_file);
                    return;
                }
                for (const auto& [chunkNum, chunkData] : KnownClientConnections[remote_endpoint_].file_chunks) {
                    ofs.write(reinterpret_cast<const char*>(chunkData.data()), chunkData.size());
                }
                ofs.close();
                LOG(LoggerVerbosity::INFO, remote_str +
					":FM: File saved successfully to " + output_file);
            }
        }
        else {
            std::cout << "Unhandled Mode: " 
                << std::string(magic_enum::enum_name(KnownClientConnections[remote_endpoint_].mode))
                << " for endpoint " << remote_endpoint_ << std::endl;
        }            
    }
};

enum class DeqMsgType : int {
    NOTSET = 0,
    MESSAGE = 1,
    PACKET = 2,
    EXIT_PROCESS = 3
};

struct StripeDequeMessage {
	DeqMsgType msg_type;
	int msg_length;
	std::vector<uint8_t> data; // Example payload, can be extended as needed
};

std::ostream& operator<<(std::ostream& os, const StripeDequeMessage& msg) {
    os << "StripeDequeMessage{msg_type=" 
       << static_cast<int>(msg.msg_type)
       << ", msg_length=" << msg.msg_length
       << ", data=";
    if (msg.data.size() != msg.msg_length) {
        os << "(Warning: msg_length does not match data size!) ";
	}
    if (!msg.data.empty()) {
        os << "[";
  //      for (const auto& byte : msg.data) {
  //          os << std::to_string(static_cast<int>(byte)) << ", ";
		//}
        //for (size_t i = 0; i < msg.data.size(); ++i) {
        //    os << static_cast<int>(msg.data[i]);
        //    if (i < msg.data.size() - 1) os << ", ";
        //}
        os << "]";
    } else {
        os << "[]";
	}
    return os;
}

// 1. Define types using the shared memory allocator hierarchy
typedef managed_shared_memory::segment_manager                          SegmentManager;
typedef allocator<StripeDequeMessage, SegmentManager>                   ShmemAllocator;
typedef boost::interprocess::deque<StripeDequeMessage, ShmemAllocator>  StripeShmDeque;

class StripeProcessor {
public:
    std::string shm_name;
    size_t shm_size;
    std::unique_ptr<boost::interprocess::managed_shared_memory> segment;
    StripeShmDeque* stripe_deque;
    bp::v1::child stripe_process;

    StripeProcessor(const char* shm_name_, size_t shm_size_)
      : shm_name(shm_name_), shm_size(shm_size_)
    {
        shared_memory_object::remove(shm_name.c_str());
        segment = std::make_unique<boost::interprocess::managed_shared_memory>(
                      boost::interprocess::create_only, shm_name.c_str(), shm_size);
        ShmemAllocator alloc_inst(segment->get_segment_manager());
        stripe_deque = segment->construct<StripeShmDeque>("SharedDeque")(alloc_inst);

        for (uint8_t i = 0; i < 5; ++i) {
            stripe_deque->push_back(StripeDequeMessage{ 
                DeqMsgType::MESSAGE, 
                1, 
                {static_cast<uint8_t>(i * 10)} });
        }

    }
    void startStripeProcess(const std::string& process_invocation_str) {
        stripe_process = bp::v1::child(process_invocation_str);
	}
};

int main(int argc, char* argv[]) {
    LoggerVerbosity verbosity = LoggerVerbosity::CRITICAL;
    double WatchdogTimeout = 360;
    std::string LogFile;
	std::string StripeConfigFile;
    std::string ServerPort = "8080";
    std::string StripeProcessName = "";
    std::vector<bp::v1::child> stripe_processes;
    std::vector<StripeProcessor> stripe_processors;
    
	LOG_INST.verbosity = verbosity;
    std::cout << "Program: " << argv[0] << std::endl;

    CommandLineParser CLP;
    CLP.AddCommand({
        CLP_Command("version", "Show version information", [](const std::string& argument) {
            show_version();
            exit(0);
        }, "", typeid(void)),
        CLP_Command("verbosity,v", "Set logging verbosity level", [&verbosity](const std::string& argument) {
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
            std::cout << "\nSetting Logger Verbosity to " << std::string(magic_enum::enum_name(verbosity))
                << " (" << static_cast<int>(verbosity) << ")"
                << " argumnet=" << argument
                << "\n";
			LOG_INST.verbosity = verbosity;
        }, "CRITICAL", typeid(std::string)),
        CLP_Command("outdir, d", "Specifies output directory", [](const std::string& argument) {
            OutDir = argument;
        }, "c:\\local\\output", typeid(std::string)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "bUDPS.log", typeid(std::string)),
        CLP_Command("server_port, p", "Specifies UDP Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "8080", typeid(std::string)),
        CLP_Command("stripe_process, x", "Stripe Process name", [&StripeProcessName](const std::string& argument) {
            StripeProcessName = argument;
        }, "", typeid(std::string)),
        CLP_Command("striper_config, s", "Specifies Striper Configuration JSON file", [&StripeConfigFile](const std::string& argument) {
            try {
                std::ifstream ifs(argument);
                if (!ifs.is_open()) {
                    std::cerr << "\nCannot open Striper config file: " << argument << "\n";
                    fs::path cwd = fs::current_path();
                    std::cout << "Current working directory: " << cwd << '\n';
                    exit(410);
                }
                // Parse JSON directly into AllStriperConfig struct
                json j;
                ifs >> j;
                StriperConfig = j.get<AllStriperConfig>();
                LOG(LoggerVerbosity::DEBUG, "Striper Configuration:" + j.dump(4));
				//std::cout << "\nStriper Configuration: " << j.dump(4) << "\n";
                std::cout << "\nStriper Config loaded successfully from " << argument << "\n";
				StriperEnable = true;
				StripeConfigFile = argument;
            }
            catch (const std::exception& e) {
                std::cerr << "\nError loading striper config: " << e.what() << "\n";
                exit(411);
            }

        }, "", typeid(std::string)),
        CLP_Command("create_striper_cfg, c", "Create Striper Configuration JSON file <config file name>", [](const std::string& argument) {
            if (argument.empty()) {
                std::cerr << "\nNo config file name provided for creation.\n";
                return;
			}
            try {
                std::ofstream ofs(argument);
                if (!ofs) {
                    std::cerr << "\nCannot open config file for writing: " << argument << "\n";
                    exit(400);
                }
                json j = StriperConfig;
                ofs << j.dump(4); // Pretty print with 4 spaces indent
            }
            catch (const std::exception& e) {
                std::cerr << "\nError saving config: " << e.what() << "\n";
                exit(401);
            }

			std::cout << "\nStriper Configuration JSON file created: " << argument << "\n";
			exit(0);
        }, "", typeid(std::string)),
        });

    show_version();
    LOG(LoggerVerbosity::DEBUG, "*** Setting DEFAULT command line arguments...***");
    CLP.SetDefaultValues();
    LOG(LoggerVerbosity::DEBUG, "\*** Parsing command line arguments...***");
    CLP.ProcessArguments(argc, argv);

	// Validate output directory exists
    fs::path dirPath = OutDir;
    if (!(fs::exists(dirPath) && fs::is_directory(dirPath))) {
        std::cout << "Output Directory does not exist: \"" << OutDir << "\"\n";
        std::cout << "Exiting Program due to non-existence of output directory\n";
        exit(300);
    }

    LOG_INST.SetLogFile(LogFile);
    LOG(LoggerVerbosity::DEBUG, "Starting log file: " + LogFile);
    LOG(LoggerVerbosity::CRITICAL, "Current verbosity = " + std::string(magic_enum::enum_name(LOG_INST.GetVerbosity())));
    if (StriperEnable) {
        LOG(LoggerVerbosity::INFO, "Striper Configuration Enabled");
        json j = StriperConfig;
        LOG(LoggerVerbosity::INFO, "Striper Configuration:" + j.dump(4));
    } else {
        LOG(LoggerVerbosity::DEBUG, "Striper Configuration Disabled");
	}

    if (!StripeProcessName.empty()) {
		// This is a Stripe Processor, not the main UDP server
        LOG(LoggerVerbosity::INFO, "This is a Stripe Processor: " + StripeProcessName);
        managed_shared_memory segment(open_only, StripeProcessName.c_str());
        // Find the named deque instance
        std::pair<StripeShmDeque*, managed_shared_memory::handle_t> res =
            segment.find<StripeShmDeque>("SharedDeque");
        if (res.first != nullptr) {
            StripeShmDeque* my_deque = res.first;

            // Read and pop elements from the deque
            std::cout << "\n["<< StripeProcessName <<"] Found deque with " << my_deque->size() << " elements:\n";
            while (!my_deque->empty()) {
                std::cout << "[" << StripeProcessName << "] Popped: " << my_deque->front() << "\n";
                my_deque->pop_front();
            }
        }
        else {
            std::cerr << "[" << StripeProcessName << "] Error: Deque object not found!\n";
        }
		exit(1);
	}
	else { // Main UDP Server
        if (StriperEnable) {
			// Striping is enabled so create the Stripe Processor processes
            //boost::asio::io_context ctx;
            //bp::process_stdio bstdio;
            //bstdio.out = stdout;
            //bstdio.err = stderr;

            

            for (int sid = 0; sid < StriperConfig.schedulerCfg.MaxNumberStripes; ++sid) {
                std::string process_name = "Stripe-" + std::to_string(sid);
                std::string process_path = argv[0];
                std::string process_args = " -x " + process_name + " --striper_config " + StripeConfigFile + " -l " + process_name + ".log";
                LOG(LoggerVerbosity::INFO, "Starting Stripe Process: " + process_name + " at path: " + process_path + " with args: " + process_args);
                
                try {
					StripeProcessor sp(process_name.c_str(), 65536); // 64KB shared memory for each stripe
                    sp.startStripeProcess(process_path + process_args);
                    stripe_processors.emplace_back(std::move(sp));
                }
                catch (const std::exception& e) {
                    LOG(LoggerVerbosity::CRITICAL, "Failed to start Stripe Process: " + process_name + ". Error: " + e.what());
                    exit(500);
                }

            }
            LOG(LoggerVerbosity::INFO, "All child processes spawned and running in parallel...");

            // 2. Wait for each process to finish to avoid zombie processes
            for (auto& sp : stripe_processors) {
                sp.stripe_process.wait();
                LOG(my_logger::LoggerVerbosity::CRITICAL, "Child process with PID " + std::to_string(sp.stripe_process.id()) + " exited with code: "
                    + std::to_string(sp.stripe_process.exit_code()));
            }

                //try {
                //    //stripe_processes.emplace_back(bp::process(ctx, process_path, process_args, bstdio));
                //} catch (const std::exception& e) {
                //    LOG(LoggerVerbosity::CRITICAL, "Failed to start Stripe Process: " + process_name + ". Error: " + e.what());
                //    exit(500);
                //}
			//}
		}
        try {
            boost::asio::io_context io_context;
            short port = static_cast<short>(std::stoi(ServerPort));
            UdpServer server(io_context, port);
        
            std::cout << "\nUDP Server running on port " << ServerPort << "..." << std::endl;
            io_context.run();
        } catch (std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
    }
    return 0;
}

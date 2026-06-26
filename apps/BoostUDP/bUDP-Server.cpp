#include "logger.h"

#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <boost/asio.hpp>
#include <boost/process/v1.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/deque.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>

#include "bUDP.h"
#include "bStriperConfig.h"
#include "bStripes.h"
#include "commandLineParser.h"
#include "watchdog.h"
#include "threadManager.h"



using namespace boost::interprocess;
namespace bp = boost::process;


std::string OutDir = "";
bool StriperEnable = false;
AllStriperConfig StriperConfig; // Global configuration for the striper
StriperModeE StriperMode = StriperModeE::NOTSET;

std::string show_version(bool use_cout = false) {
    std::stringstream ss;
    ss << "\n\tUDP Server (boost) Version: " << VERSION << "." << GIT_HASH << std::endl;
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




// 1. Define types using the shared memory allocator hierarchy
typedef boost::interprocess::managed_shared_memory::segment_manager     SegmentManager;
typedef allocator<uint8_t, SegmentManager>                              ByteAllocator;
typedef allocator<uint8_t, SegmentManager>                              ShmemByteAllocator;
typedef allocator<void, SegmentManager>                                 VoidAllocator; // placeholder
// Allocator for StripeDequeMessage (defined below) will be declared after message is known.
// We'll define the message's internal vector allocator alias below once message is declared.

/*
struct StripeDequeMessage; // forward declare so we can declare the deque allocator type
typedef boost::interprocess::allocator<int, SegmentManager>                   StripeSharedDataAllocator;
typedef boost::interprocess::allocator<StripeDequeMessage, SegmentManager>    ShStripeMsgAllocator;
typedef boost::interprocess::deque<StripeDequeMessage, ShStripeMsgAllocator>  StripeShmDeque;
*/
// Now define the message using an interprocess vector for the payload
/*
enum class DeqMsgType : int {
    NOTSET = 0,
    MESSAGE = 1,
    PACKET = 2,
    EXIT_PROCESS = 3
};
*/

/*
struct StripeDequeMessage {
    DeqMsgType msg_type;
    int msg_length;

    // Use interprocess vector for payload so it can live inside shared memory
    typedef boost::interprocess::vector<uint8_t, allocator<uint8_t, SegmentManager>> ShmVector;
    ShmVector data;

    // Allocator-aware constructors
    explicit StripeDequeMessage(const allocator<uint8_t, SegmentManager>& alloc)
        : msg_type(DeqMsgType::NOTSET), msg_length(0), data(alloc) {
    }

    StripeDequeMessage(DeqMsgType t, int len, const allocator<uint8_t, SegmentManager>& alloc)
        : msg_type(t), msg_length(len), data(alloc) {
    }
};


static std::ostream& operator<<(std::ostream& os, const StripeDequeMessage& msg) {
    os << "StripeDequeMessage{msg_type="
        << static_cast<int>(msg.msg_type)
        << ", msg_length=" << msg.msg_length
        << ", data=";
    int msize = msg.data.size();
    if (msg.data.size() != static_cast<size_t>(msg.msg_length)) {
        os << "(Warning: msg_length does not match data size!) ";
    } 
    if (msg.data.size() != 0) {
        os << "[";
        for (auto it = msg.data.begin(); it != msg.data.end(); ++it) {
            os << std::to_string(static_cast<int>(*it));
            if (std::next(it) != msg.data.end()) os << ", ";
        }
        os << "]";
    }
    else {
        os << "[]";
    }
    os << "}";
    return os;
}
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
        //ShStripeMsgAllocator alloc_inst(segment->get_segment_manager());
        //stripe_deque = segment->construct<StripeShmDeque>("SharedDeque")(alloc_inst);

        // Create a message using the shared-memory byte allocator
        allocator<uint8_t, SegmentManager> byteAlloc(segment->get_segment_manager());
        StripeDequeMessageNew msg(byteAlloc);
        msg.msg_type = DeqMsgType::MESSAGE;
        msg.msg_length = 5;
        for (uint8_t i = 0; i < msg.msg_length; ++i) {
            msg.data.push_back(i*10);
        }
        shared_data->stripe_deque.push_back(msg);
        shared_data->cond_nonempty.notify_one();
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
*/


int main(int argc, char* argv[]) {
    my_logger::LoggerVerbosity verbosity = my_logger::LoggerVerbosity::ERR;
    double WatchdogTimeout = 360;
    std::string LogFile;
	std::string StripeConfigFile;
    std::string ServerPort = "8080";
    std::string StripeProcessName = "";

	LOG_INST.verbosity = verbosity;
    //std::cout << "Program: " << argv[0] << std::endl;

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
        }, "c:\\local\\output", typeid(std::string)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "bUDPS.log", typeid(std::string)),
        CLP_Command("server_port, p", "Specifies UDP Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "8080", typeid(std::string)),
        CLP_Command("stripe_process, x", "Stripe Process name", [&StripeProcessName](const std::string& argument) {
            StripeProcessName = argument;
            LOG_INST.SetPreamble(argument);
        }, "", typeid(std::string)),
        CLP_Command("rx_striper, r", "Striper Mode is Receiver (receives FEC streams)", 
            [](const std::string& argument) {
            StriperMode = StriperModeE::RECEIVER;
        }, "", typeid(void)),
        CLP_Command("tx_striper, t", "Striper Mode is transmitter (creates FEC streams)",
            [](const std::string& argument) {
            StriperMode = StriperModeE::TRANSMITTER;
        }, "", typeid(void)),
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
        CLP_Command("watchdog,w", "Watchdog timeout in seconds", [&WatchdogTimeout](const std::string& argument) {
            try {
                WatchdogTimeout = std::stod(argument);
            }
            catch (const std::exception& e) {
                std::cerr << "Invalid watchdog timeout value: " << argument << ". Setting to default 360 seconds.\n";
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
        std::cout << "Exiting Program due to non-existence of output directory\n";
        exit(300);
    }

    LOG_INST.SetLogFile(LogFile);
    LOG(LoggerVerbosity::DEBUG, "Starting log file: " + LogFile);
    auto vname = std::string(magic_enum::enum_name(LOG_INST.GetVerbosity()));
    if (vname.empty()) vname = std::to_string((int) LOG_INST.GetVerbosity());
    LOG(LoggerVerbosity::CRITICAL, "Current verbosity = " + vname);    
    
    if (StriperEnable) {
        LOG(LoggerVerbosity::INFO, "Striper Configuration Enabled");
        json j = StriperConfig;
        LOG(LoggerVerbosity::INFO, "Striper Configuration:" + j.dump(4));
    } else {
        LOG(LoggerVerbosity::DEBUG, "Striper Configuration Disabled");
	}

    // Start WATCHDOG thread to monitor and adjust FEC stripes
    ThreadManager& TM = ThreadManager::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();
    // watchdog.SetTimeout(vm["watchdog"].as<double>();
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

    LOG(LoggerVerbosity::CRITICAL, "Striper Mode: " + std::string(magic_enum::enum_name(StriperMode)));
    if (!StripeProcessName.empty()) {
		// This is a Stripe Processor, not the main UDP server

        Debugger debug;
        debug.Launch();

        auto sp = StripeProcess(StripeProcessName);
		watchdog.StopMonitoring();
    }
    else {
        // Main UDP Server
        if (StriperEnable) {
            // Striping is enabled so create the Stripe Processor processes
            std::string process_args = " --striper_config " + StripeConfigFile;
            process_args += " --verbosity " + std::string(magic_enum::enum_name(verbosity));
            process_args += " --watchdog " + std::to_string(WatchdogTimeout);

            StripesManager stripesMgr(&StriperConfig);
            stripesMgr.Initialize(StriperMode, argv[0], process_args);
            stripesMgr.WaitForComplete();
        }

        if (TM.force_stop) {
            // Timeout must have occured durring launching of stripe processes
            LOG(LoggerVerbosity::CRITICAL, "Exiting progam as FORCE_STOP was issued");
        }
        else {

            try {
                boost::asio::io_context io_context;
                short port = static_cast<short>(std::stoi(ServerPort));
                UdpServer server(io_context, port, OutDir);

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
        }
    }

    // Wait for threads to join
    LOG(LoggerVerbosity::INFO, "Waiting threads to join...");
    TM.WaitAllThreads(); // Wait for all threads to finish
    return 0;
}

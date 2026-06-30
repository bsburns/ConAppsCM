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


int main(int argc, char* argv[]) {
    my_logger::LoggerVerbosity verbosity = my_logger::LoggerVerbosity::ERR;
    double WatchdogTimeout = 360;
    std::string LogFile;
	std::string StripeConfigFile;
    std::string ServerPort = "8080";
    std::string StripeProcessName = "";
    uint32_t WaitDebugAttachIterations = 0;
    StripeProcess* StripeProc = nullptr;

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
                std::cerr << "\nInvalid watchdog timeout value: " << argument << ". Setting to default 360 seconds.\n";
                WatchdogTimeout = 360;
            }
        }, "360", typeid(double)),
        CLP_Command("wait_attach_debugger, y", "Wait loop to attach debugger = #interations",
            [&WaitDebugAttachIterations](const std::string& argument) {
                try {
                    WaitDebugAttachIterations = std::stoul(argument);
                }
                catch (const std::exception& e) {
                    std::cerr << "\nInvalid watchdog timeout value: " << argument << ". Setting to default 360 seconds.\n";
                    WaitDebugAttachIterations = 0;
                }

            }, "0", typeid(uint32_t)),

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
    auto vname = std::string(magic_enum::enum_name(LOG_INST.verbosity));
    if (vname.empty()) vname = std::to_string(LOG_INST.GetVerbosity());
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
    std::shared_ptr<StripesManager> stripesMgr = std::make_shared<StripesManager>(&StriperConfig);
    if (!StripeProcessName.empty()) {
		// This is a Stripe Processor, not the main UDP server

        Debugger debug;
        debug.Launch(WaitDebugAttachIterations);

        StripeProc = new StripeProcess(StripeProcessName, StriperMode, &StriperConfig);
    }
    else {
        // Main UDP Server
        if (StriperEnable) {
            // Striping is enabled so create the Stripe Processor processes
            std::string process_args = " --striper_config " + StripeConfigFile;
            //process_args += " --verbosity " + std::string(magic_enum::enum_name(verbosity));
            process_args += " --verbosity " + std::to_string(LOG_INST.GetVerbosity());
            process_args += " --watchdog " + std::to_string(WatchdogTimeout);
            process_args += " -y " + std::to_string(WaitDebugAttachIterations);
            process_args += " --outdir " + OutDir;
            process_args += StriperMode == StriperModeE::TRANSMITTER ? " --tx_striper" : " --rx_striper";

            //std::string process_args2;
            //for (int i = 1; i < argc; i++) {
            //    process_args2 += std::string(argv[i]) + " ";
            //}
            
            stripesMgr->Initialize(StriperMode, argv[0], process_args);
            stripesMgr->SendMessage("Next message");
        }

        if (TM.force_stop) {
            // Timeout must have occured durring launching of stripe processes
            LOG(LoggerVerbosity::CRITICAL, "Exiting progam as FORCE_STOP was issued");
        } else {

            try {
                boost::asio::io_context io_context;
                short port = static_cast<short>(std::stoi(ServerPort));
                UdpServer server(io_context, port, OutDir, stripesMgr, StriperMode);

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
    stripesMgr->SendExit();
    stripesMgr->WaitForComplete();
    watchdog.StopMonitoring(); 
    TM.WaitAllThreads(); // Wait for all threads to finish
    return 0;
}

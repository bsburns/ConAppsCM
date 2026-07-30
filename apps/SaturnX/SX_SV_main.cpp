/*------------------------------------------------------------------
 * SX_SV_main.cpp
 *
 * Saturn X Space Vehicle emulation
 * 
 * Sends/Receives command/status to SaturnX Device
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */#include "logger.h"

#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <thread>
#include <boost/asio.hpp>

#include "SX_common.h"
#include "commandLineParser.h"
#include "watchdog.h"
#include "threadManager.h"
#include "cli/CLI.h"



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

int main(int argc, char* argv[]) {
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
        }, "c:\\local\\output", typeid(std::string)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "SX_SV.log", typeid(std::string)),
        CLP_Command("timestamping, T", "Adds timestamps to log entries",
            [](const std::string& argument) {
            LOG_INST.SetTimeStamping(true);
        }, "", typeid(void)),
        CLP_Command("server_port, p", "Specifies UDP Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "8080", typeid(std::string)),
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
        std::cout << "Exiting Program due to non-existence of output directory\n";
        exit(300);
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
    auto& CMP = CliMenuProcessor::GetInstance();
    CMP.SetPrompt("SX SV> ");
    CMP.AddSubMenu(LOG_INST.cli_menu);
    CMP.AddSubMenu(Watchdog::GetInstance().cli_menu);
    TM.StartThread("CLIInput", CliMenuProcessor::GetUserInput_thread);


    // Wait for threads to join
    LOG(LoggerVerbosity::INFO, "Waiting threads to join...");
    watchdog.StopMonitoring(); 
    TM.WaitAllThreads(); // Wait for all threads to finish
    return 0;
}

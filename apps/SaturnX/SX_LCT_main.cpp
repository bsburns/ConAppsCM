/*------------------------------------------------------------------
 * SX_LCT_main.cpp
 *
 * Saturn X Laser Comm Terminal emulation
 * 
 * Sends/Receives command/status to SaturnX Device
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include "logger.h"

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
#include "statistics.h"
#include "MulticastUdpServer.h"


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

class LCT_Engine {
private:
    MulticastUdpServer *CmdMsgServer = nullptr;

    // Stats
    uint64_t CmdMsgIncorrectLength = 0;
    uint64_t CmdMsgIncorrectBufferSize = 0;
    uint64_t CmdMessageBadMagic = 0;
    uint64_t CmdMessageBadCfgCrc = 0;
    uint64_t CmdMessageBadZero = 0;

public:
    MenuItem cli_menu =
    {
    .name = "lct_Engine",
    .description = "LCT commands",
    .subMenus = {
    },

    .valType = MenuItemValueTypes::SUBMENU,
    .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->cli_menu.Help(); },
    };
    MenuItem cmd_msg_menu = {
    .name = "cmdMsg",
    .description = "Command Messages",
    .subMenus = {
    },

    .valType = MenuItemValueTypes::SUBMENU,
    .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->cli_menu.Help(); },
    };


    LCT_Engine() {
        auto& CMP = CliMenuProcessor::GetInstance();
        cli_menu.AddSubMenu(cmd_msg_menu);
        CMP.AddSubMenu(cli_menu);
    }

    void Start() {
        ThreadManager& TM = ThreadManager::GetInstance();

        cli_menu.AddSubMenu(cmd_msg_menu);

        TM.StartThread("CmdMsgServer", [this, &TM]() {
            auto& CMP = CliMenuProcessor::GetInstance();
            Watchdog& watchdog = Watchdog::GetInstance();

            try {
                boost::asio::io_context io_context;
                short port = static_cast<short>(std::stoi(SX_DEVICE_CMD_PORT));
                CmdMsgServer = new MulticastUdpServer(
                    io_context, 
                    "0.0.0.0", 
                    SX_DEVICE_CMD_ADDR, 
                    port,
                    [this](std::vector<uint8_t>& buf, std::size_t len) { this->ProcessCommandMessage(buf, len); }
                );

                CMP.DynamicAddSubMenu(std::vector<std::string>{"lct_Engine", "cmdMsg"}, CmdMsgServer->cli_menu);

                std::cout << "\nCmd Message MX UDP Server running on "<< SX_DEVICE_CMD_ADDR<<":" << port << "..." << std::endl;
                watchdog.SetOnTimeoutCallback([this]() {
                    LOG(LoggerVerbosity::CRITICAL, "Watchdog timeout callback invoked. Performing cleanup before exit.");
                    // Perform any necessary cleanup here
                    CmdMsgServer->StopServer(); // Explicitly call destructor to clean up resources
                    });
                io_context.run();
            }
            catch (std::exception& e) {
                std::cerr << "Exception: " << e.what() << std::endl;
            }

        });
    }

    void ProcessCommandMessage(std::vector<uint8_t>& recv_buffer_, std::size_t length) {
        auto cmdMsg = PacketCommandMessage();
        if (length != cmdMsg.Size()) {
            CmdMsgIncorrectLength++;
            LOG(LoggerVerbosity::INFO, "Received Command Message of incorrect length=" + std::to_string(length));
            return;
        }
        auto rc = cmdMsg.deserialize(recv_buffer_);
        switch (rc) {
        case SX_ReturnCodes::NOMINAL:
            break;
        case SX_ReturnCodes::INCORRECT_PKT_SIZE:
            CmdMsgIncorrectBufferSize++;
            LOG(LoggerVerbosity::INFO, "Received Command Message of incorrect Buffer Size");
            break;
        case SX_ReturnCodes::BAD_MAGIC:
            CmdMessageBadMagic++;
            LOG(LoggerVerbosity::INFO, "Received Command Message with bad magic field");
            break;
        case SX_ReturnCodes::BAD_CFG_CRC:
            CmdMessageBadCfgCrc++;
            LOG(LoggerVerbosity::INFO, "Received Command Message with bad CONFIG CRC field");
            break;
        case SX_ReturnCodes::BAD_ZERO_FIELD:
            CmdMessageBadZero++;
            LOG(LoggerVerbosity::INFO, "Received Command Message with bad ZERO field");
            break;
        default:
            break;
        }
    }
};


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
        }, "SX_LCT.log", typeid(std::string)),
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
    CMP.SetPrompt("SX LCT> ");
    CMP.AddSubMenu(LOG_INST.cli_menu);
    CMP.AddSubMenu(Watchdog::GetInstance().cli_menu);
    TM.StartThread("CLIInput", CliMenuProcessor::GetUserInput_thread);

    // Create LCT engine
    auto engine = LCT_Engine();
    engine.Start();

    // Wait for threads to join
    LOG(LoggerVerbosity::INFO, "Waiting threads to join...");
    watchdog.StopMonitoring(); 
    TM.WaitAllThreads(); // Wait for all threads to finish
    return 0;
}

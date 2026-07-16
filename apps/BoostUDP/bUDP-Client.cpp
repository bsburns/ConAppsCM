/*------------------------------------------------------------------
 * bUDP-Client.cpp
 * 
 * Boost based UDP Client application to test the bUDP-Server
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "logger.h"
#include <iostream>
#include <string>
#include <array>
#include <boost/asio.hpp>


#include "commandLineParser.h"
#include "watchdog.h"
#include "PacketHeader.h"
#include "bUDP.h"
#include "bUDP-Client.h"


std::string show_version(bool use_cout = false) {
    std::stringstream ss;
    ss << "\n\tUDP Client (boost) Version: " << VERSION << "." << GIT_HASH << std::endl;
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
    std::string SendFile;
    std::string ServerPort = "8080";
    std::string SourcePort = "100";
    std::string ServerIP = "127.0.0.1";
	bool interactiveMode = false;
	UdpSendMode sendMode = UdpSendMode::MESSAGE;

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
                    std::cerr << "Invalid verbosity level: " << argument << ". Setting to NOTSET.\n";
                }
			}
            LOG_INST.verbosity = verbosity;
     
            std::cout << "\nSetting Logger Verbosity to " << std::string(magic_enum::enum_name(verbosity))
                << " (" << static_cast<int>(verbosity) << ")"
				<< " argument=" << argument
                << "\n";
        }, "INFO", typeid(std::string)),
        CLP_Command("sendfile, f", "Specifies file name to send", [&SendFile, &sendMode](const std::string& argument) {
            SendFile = argument;
            sendMode = UdpSendMode::SEND_FILE;
        }, "", typeid(std::string)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "bUDPC.log", typeid(std::string)),
        CLP_Command("server_port, p", "Specifies UDP Destination Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "8080", typeid(std::string)),
        CLP_Command("source_port, a", "Specifies UDP Source Port number of this client", [&SourcePort](const std::string& argument) {
            SourcePort = argument;
        }, "100", typeid(std::string)),
        CLP_Command("interactive, i", "Interactively queries users for messages to be sent from client", [&interactiveMode](const std::string& argument) {
            interactiveMode = true;
        }, "", typeid(void)),
        CLP_Command("server_ip,s", "Specifies IP address of server", [&ServerIP](const std::string& argument) {
            ServerIP = argument;
        }, "127.0.0.1", typeid(std::string)),
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
    UdpClient uclient(io_context, ServerIP, SourcePort, ServerPort, sendMode);
    if (sendMode == UdpSendMode::SEND_FILE) {
		uclient.SendFile(SendFile);
    }
    else {
		uclient.SendMessage("Hello from Synchronous bUDP Client!");

        if (interactiveMode) {
            std::string input;
            ThreadManager& TM = ThreadManager::GetInstance();

            while (true && !TM.force_stop.load()) {
                std::cout << "Enter message to send (or 'exit' to quit): ";
                std::getline(std::cin, input);
                if (input == "exit") {
                    break;
                }
				uclient.SendMessage(input);
				watchdog.CheckIn(); // Reset watchdog timer after each message sent
            }
        }
        else {
            LOG(LoggerVerbosity::INFO, "Not in interactive mode, exiting after one message.");
        }
    }


    // Wait for threads to join
    LOG(LoggerVerbosity::INFO, "Waiting threads to join...");
    watchdog.StopMonitoring();
    TM.WaitAllThreads(); // Wait for all threads to finish

    return 0;
}

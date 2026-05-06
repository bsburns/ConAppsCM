/*------------------------------------------------------------------
 * ConsoleSimulator.cpp
 * 
 * Console app to provide infrasture to perform simulations
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <iostream>
#include <thread>
#include <algorithm>
#include <cctype>
// #include <boost/program_options.hpp>


#include "Simulator/RootSim.h"
#include "cli/CLI.h"
#include "logger.h"
#include "watchdog.h"
#include "commandLineParser.h"
#include "threadManager.h"


// namespace po = boost::program_options;

std::string TestName;

int main(int argc, char* argv[])
{
	ThreadManager& TM = ThreadManager::GetInstance();
    LoggerVerbosity verbosity = LoggerVerbosity::CRITICAL;
    TestName = "default";
	double WatchdogTimeout = 360;
    std::string LogFile;

    CommandLineParser CLP;
    CLP.AddCommand({
        CLP_Command("version", "Show version information", [](const std::string& argument) {
            std::cout << "Console Simulator Version: 1.0.0.2\n";
        }),
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
        }, "CRITICAL", typeid(std::string)),
        CLP_Command("testname, t", "Specifies test name", [](const std::string& argument) {
            TestName = argument;
        }, "default", typeid(std::string)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "default.log", typeid(std::string)),
        CLP_Command("watchdog,w", "Watchdog timeout in seconds", [&WatchdogTimeout](const std::string& argument) {
            try {
                WatchdogTimeout = std::stod(argument);
                std::cout << "\nSetting Watchdog Timeout to " << WatchdogTimeout << " seconds\n";
            }
            catch (const std::exception& e) {
                std::cerr << "Invalid watchdog timeout value: " << argument << ". Setting to default 360 seconds.\n";
                WatchdogTimeout = 360;
			}
        }, "360", typeid(double)),
        });
	std::cout << "\n** Setting DEFAULT command line arguments...**\n";
    CLP.SetDefaultValues();
    std::cout << "\n** Parsing command line arguments...**\n";
    CLP.ProcessArguments(argc, argv);

    LOG_INST.SetLogFile(LogFile);
    LOG(verbosity, "Starting log");

	// Start CLI input thread
	auto& CMP = CliMenuProcessor::GetInstance();
	CMP.SetPrompt("ConsoleSim> ");
    CMP.AddSubMenu(LOG_INST.cli_menu);
    CMP.AddSubMenu(SimulationManager::GetInstance().GetCliMenu());
    CMP.AddSubMenu(Watchdog::GetInstance().cli_menu);

    //std::thread cli_thread(CliMenuProcessor::GetUserInput_thread);
	TM.StartThread("CLIInput", CliMenuProcessor::GetUserInput_thread);

	// Start WATCHDOG thread to monitor and adjust FEC stripes
    Watchdog& watchdog = Watchdog::GetInstance();
    // watchdog.SetTimeout(vm["watchdog"].as<double>());
    watchdog.SetTimeout(WatchdogTimeout);
	TM.StartThread("WatchdogMonitor", Watchdog::monitor_thread);
    //std::thread watchdog_thread(Watchdog::monitor_thread);
    

    // Wait for threads to join
    std::cout << "\nWaiting threads to join...\n";
	TM.WaitAllThreads(); // Wait for all threads to finish
    //watchdog_thread.join();
    //std::cout << "WATCHDOG THREAD COMPLETED...wait for cli thread\n";
    //cli_thread.join(); // Clean up thread resources
    //std::cout << "CLI THREAD COMPLETED\n";

    std::cout << "\nExiting main thread...\n";
}

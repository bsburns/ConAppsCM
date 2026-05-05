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


// namespace po = boost::program_options;

std::string TestName;

int main(int argc, char* argv[])
{
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
	std::cout << "\nSetting DEFAULT command line arguments...\n";
    CLP.SetDefaultValues();
    std::cout << "\nParsing command line arguments...\n";
    CLP.ProcessArguments(argc, argv);
    // po::options_description generic("Generic options");
    // generic.add_options()
    //     ("version", "print version string")
    //     ("verbosity,v", po::value<std::string>(), "Logging verbsity level")
    //     ("watchdog", po::value<double>()->default_value(360), "Watchdog timeout (seconds)")
    //     ("logfile,l", po::value<std::string>(), "Specifies Log file name")
    //     ("testname,t", po::value<std::string>(), "Specifies test name")
    //     ("config", po::value<std::string>(), "Path to config file")
    //     ("help,h", "produce help message")
    //     ;

    // po::options_description cmdline_options;
    // cmdline_options.add(generic);

    // po::variables_map vm;
    // po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
    // po::notify(vm);




    // // If config file specified, read it
    // if (vm.count("config")) {
    //     std::ifstream ifs(vm["config"].as<std::string>(), std::ifstream::in);
    //     if (!ifs) {
    //         std::cerr << "Cannot open config file: " << vm["config"].as<std::string>() << "\n";
    //         return 1;
    //     }

    //     // Boost.Program_options ignores section headers like [database]
    //     // You can preprocess the file to remove them if needed
    //     po::store(po::parse_config_file(ifs, cmdline_options), vm);
    //     po::notify(vm);
    // }


    LOG_INST.SetLogFile(LogFile);
    LOG(verbosity, "Starting log");

	// Start CLI input thread
	auto& CMP = CliMenuProcessor::GetInstance();
	CMP.SetPrompt("ConsoleSim> ");
    CMP.AddSubMenu(LOG_INST.cli_menu);
    CMP.AddSubMenu(SimulationManager::GetInstance().GetCliMenu());
    CMP.AddSubMenu(Watchdog::GetInstance().cli_menu);

    std::thread cli_thread(CliMenuProcessor::GetUserInput_thread);

	// Start WATCHDOG thread to monitor and adjust FEC stripes
    Watchdog& watchdog = Watchdog::GetInstance();
    // watchdog.SetTimeout(vm["watchdog"].as<double>());
    watchdog.SetTimeout(WatchdogTimeout);
    std::thread watchdog_thread(Watchdog::monitor_thread);
    

    // Wait for threads to join
    std::cout << "\nWaiting threads to join...\n";
    watchdog_thread.join();
    std::cout << "WATCHDOG THREAD COMPLETED...wait for cli thread\n";
    cli_thread.join(); // Clean up thread resources
    std::cout << "CLI THREAD COMPLETED\n";

    std::cout << "\nExiting main thread...\n";
}

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
// #include <boost/program_options.hpp>


#include "Simulator/RootSim.h"
#include "cli/CLI.h"
#include "utilities/logger.h"
#include "utilities/watchdog.h"


// namespace po = boost::program_options;

std::string TestName;

int main(int argc, char* argv[])
{
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


    // if (vm.count("help")) {
    //   std::cout << cmdline_options << "\n";
    //   return 1;
    // }

    // if (vm.count("version")) {
    //   std::cout << "Console Simulator Version 1.0.0.2\n";
    //   return 1;
	// }

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


    TestName = "default";
    std::string LogFile = "default.log";
    // if (vm.count("logfile") == 0 && vm.count("testname") == 0) {
    //     std::cout << "Neither testname or logfile parameters were specified, so setting log filename to " << LogFile << "\n";
    // }
    // else if (vm.count("testname")) {
    //     TestName = vm["testname"].as<std::string>();
    //     std::cout << "Test Name = " << TestName << "\n";
    //     LogFile = TestName + ".log";
    // }
    // if (vm.count("logfile")) {
    //     LogFile = vm["logfile"].as<std::string>();
    //     std::cout << "Setting log filename to " << LogFile << "\n";
    // }
    LOG_INST.SetLogFile(LogFile);
    LOG(LoggerVerbosity::CRITICAL, "Starting log");
    // if (vm.count("verbosity")) {
    //   std::cout << "\nSetting Verbosity level to "
    //             << vm["verbosity"].as<std::string>() << ".\n";
	//   LOG_INST.SetVerbosityStr(vm["verbosity"].as<std::string>());
	// }

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
    watchdog.SetTimeout(5);
    std::thread watchdog_thread(Watchdog::monitor_thread);
    

    // Wait for threads to join
    std::cout << "\nWaiting threads to join...\n";
    watchdog_thread.join();
    std::cout << "WATCHDOG THREAD COMPLETED...wait for cli thread\n";
    cli_thread.join(); // Clean up thread resources
    std::cout << "CLI THREAD COMPLETED\n";

    std::cout << "\nExiting main thread...\n";
}

#pragma once
/*------------------------------------------------------------------
 * Command Line Parser Header File
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <string>
#include <vector>
#include <functional>
#include <typeinfo>  // For typeid
#include <iostream> // For std::cout
#include <iomanip> // For std::setw and std::setfill

#include "utility.h"

class CLP_Command {

public:
	std::string name;
	std::string description;
	std::function<void(const std::string& argument) > executeCommand;
	std::string defaultValue;
	const std::type_info* valueType = &typeid(void);
	bool hasValue = false;
	//std::string value;
	std::string ddashName;
	std::string sdashName;

	CLP_Command(std::string name, std::string description, std::function<void(const std::string& argument)> executeCommand, std::string defaultValue = "", const std::type_info& valueType = typeid(void))
		: name(name), description(description), executeCommand(executeCommand), defaultValue(defaultValue), valueType(&valueType) {
		hasValue = (&valueType != &typeid(void));
		if (name.find(",") != std::string::npos) {
			ddashName = "--" + util::trim(name.substr(0, name.find(",")));
			sdashName = "-" + util::trim(name.substr(name.find(",") + 1));
		}
		else {
			ddashName = "--" + name;
			sdashName = "";
		}
	}
	bool HasValue() const { return hasValue; }
	bool operator==(const std::string& cmdName) const {
		if (ddashName.find(cmdName) != std::string::npos) return true;
		if (!sdashName.empty() && cmdName == sdashName) return true;
		return false;
		//return name == cmdName || (name.find(",") != std::string::npos && (name.substr(0, name.find(",")) == cmdName || name.substr(name.find(",") + 1) == cmdName));
	}
};

class CommandLineParser {
private:
	std::vector<CLP_Command> commands;

public:
	CommandLineParser() {
		// Initialize with default commands
		commands.push_back(
			CLP_Command(
				"help, h",
				"Help",
				[this](const std::string& argument) {
					std::cout << "Application Help:\n";
					for (const auto& cmd : commands) {
						std::string command = cmd.ddashName;
						if (!cmd.sdashName.empty()) {
							command += " (" + cmd.sdashName + ")";
						}
						std::cout << "  " << std::left << std::setw(20)
							<< command << " : " << cmd.description << "\n";
					} // end foreach
					exit(0);
				} // end execute command
			) // end CLP_Command
		);
	}
	void ProcessArguments(int argc, char* argv[]) {
		// Parse command line arguments and populate member variables
		for (int i = 1; i < argc; ++i) {
			std::string arg = argv[i];
			for (const auto& cmd : commands) {
				if (arg == cmd) {
					bool hasValue = cmd.HasValue();
					std::string arg2 = "";
					if (hasValue) {
						i++;
						if (i < argc) {
							arg2 = argv[i];
						} else {
							std::cout << "Command: " << cmd.name << " Expected argument but none provided.\n";
							exit(200);
						}
					}
					std::cout << "\nExecuting command: " << cmd.ddashName
						<< " HasValue=" << hasValue << " Argument=" << arg2
						<< "\n";
					cmd.executeCommand(arg2);
				}
			}
			//if (arg == "--version") {
			//	versionFlag = true;
			//} else if (arg == "--help" || arg == "-h") {
			//	helpFlag = true;
			//} else if (arg == "--watchdog" && i + 1 < argc) {
			//	watchdogTimeout = std::stod(argv[++i]);
			//} else if ((arg == "--logfile" || arg == "-l") && i + 1 < argc) {
			//	logFileName = argv[++i];
			//} else if ((arg == "--testname" || arg == "-t") && i + 1 < argc) {
			//	testName = argv[++i];
			//} else if (arg == "--config" && i + 1 < argc) {
			//	configFilePath = argv[++i];
			//}
		}
	}

	void SetDefaultValues() {
		for (const auto& cmd : commands) {
			if (cmd.HasValue() && !cmd.defaultValue.empty()) {
				cmd.executeCommand(cmd.defaultValue);
			}
		}
	}
	
	void AddCommand(CLP_Command cmd) {
		commands.push_back(cmd);
	}
	void AddCommand(std::vector<CLP_Command> cmdV) {
		commands.insert(commands.end(), cmdV.begin(), cmdV.end());
	}
};
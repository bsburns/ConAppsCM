#pragma once
/*------------------------------------------------------------------
 * Command Line Parser Header File
 * 
 * Provides a simple command line parser that allows defining commands 
 * with associated descriptions, default values, and execution functions. 
 * It supports both long and short command formats (e.g., --help and -h) and can handle commands that require arguments.
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
		if (ddashName.rfind(cmdName, 0) == 0) return true;
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
			bool matched = false;
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
					std::cout << "\nExecuting command: " << arg << " matched_cmd=" << cmd.ddashName
						<< " HasValue=" << hasValue << " Argument=" << arg2
						<< "\n";
					cmd.executeCommand(arg2);
					matched = true;
					break;
				}
			} // end for each command
			if (!matched) {
				std::cout << "\nUnknown command line argument: " << arg << "\n";
				exit(100);
			}
		} // end for each argument
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
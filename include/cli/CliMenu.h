#pragma once
/*------------------------------------------------------------------
 * CLI Menu Header File
 *
 * Controls accepting user input from the command line interface (CLI)
 * 
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <iostream>
#include <string>
#include <vector>
#include "magic_enum.hpp"
#include "utilities/utility.h"

#define INDENT_LVL_SPACES 4
#define MAX_COMMAND_HISTORY 50

extern void stooges();

enum class MenuItemValueTypes: int {
	NONE,
	SUBMENU,
	ANY, // Accept any argument as string and pass to executeCommand, which can then parse it as needed
	STRING,
	INT,
	UINT,
	INT64,
	UINT64,
	DOUBLE,
};

class MenuItem {
private:
	int processArguments(std::string command, std::vector<std::string>& arguments, bool first=true) {
		int rc = 101;
		bool do_help = false;
		if (arguments.size() > 1 && arguments[1] == "?") do_help = true;
		if (arguments.size() > 0) { // More arguments to process
			std::vector<std::string> popFirst(arguments.begin() +1, arguments.end());
			for (auto& sub : subMenus) {
				if (sub.name.starts_with(arguments[0])) {
					if (do_help) {
						sub.Help();
						return 0;
					} else if (sub.valType == MenuItemValueTypes::SUBMENU){
						rc = sub.processArguments(command, popFirst, false);
						if (rc == 0) return 0;
					} else if (!sub.subMenus.empty() && !popFirst.empty()) {
						rc = sub.processArguments(command, popFirst, false);
						if (rc == 0) return 0;
					} else if (sub.valType == MenuItemValueTypes::NONE) {
						sub.executeCommand(valType, "");
						return 0;
					} else {
						if (arguments.size() > 1) {
							sub.executeCommand(valType, arguments[1]);
							return 0;
						} else if (sub.valType == MenuItemValueTypes::ANY) {
							sub.executeCommand(valType, "");
							return 0;
						} else {
							std::cout << "\nInvalid command input: cmd=" << name 
								<< " was expecting arguments of type " 
								<< std::string(magic_enum::enum_name(valType))
								<< ", but none were supplied.\n";
							return -2;
						}
					}
				}
			}
			return rc;
		} else { // No more arguments to process
			executeCommand(valType, "");
			return 0;
		}
		return -20;
	}
public:
	std::string name;
	std::string description;
	std::vector<MenuItem> subMenus;
	MenuItemValueTypes valType = MenuItemValueTypes::NONE;
	std::function<void(MenuItemValueTypes vt, const std::string& argument)> executeCommand;

	//MenuItemNew() {}

	void AddSubMenu(const MenuItem& new_submenu) {
		subMenus.push_back(new_submenu);
	}

	void ProcessCommand(const std::string& command) {
		auto args = util::splitBySpace(command);

		auto rc = processArguments(command, args);
		if (rc != 0) {
			stooges();
			std::cout << "\nError: Unable to process command: "<< command;
		}
	}

	void Help(bool traverse) { 
		Help("", "", traverse);
	}

	void Help(std::string indentStr = "", const std::string &preamble = "", bool traverse = false) const {
		static const std::string addIndent(INDENT_LVL_SPACES,' ');
		if (indentStr.empty()) {
			std::cout << "\nHELP";
			std::cout << "\n"
                  << indentStr << preamble << "-" << name << " : " << description;
		}

        indentStr += addIndent;
		std::string subIndentStr = indentStr + addIndent;
        if (!subMenus.empty()) {
            //std::cout <<"\n" << indentStr << "Sub-Menus:";
			std::cout << " : " << description;
			if (valType != MenuItemValueTypes::SUBMENU) {
				std::cout << "\n" << indentStr << preamble;
				if (valType != MenuItemValueTypes::NONE)
					std::cout << " [" << std::string(magic_enum::enum_name(valType)) << "]";
				std::cout << " : " << description;
			} 

			for (auto& sub : subMenus ) {
				std::cout << "\n" << indentStr << preamble << " " << sub.name;
				if (!sub.subMenus.empty()) {
					if (traverse) sub.Help(subIndentStr, preamble + sub.name, traverse);
				} else {
					if (sub.valType != MenuItemValueTypes::NONE) 
						std::cout << " [" << std::string(magic_enum::enum_name(sub.valType)) << "]";
						
					std::cout << " : " << sub.description;
				}
			}
        }

		std::cout << "\n";
	}
};


#pragma once
/*------------------------------------------------------------------
 * CLI Header File
 *
 * Defines function for accepting user input from the command line interface (CLI) and executing commands based on that input.
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <atomic>
#include <filesystem>

#include "utilities/logger.h"
#include "utilities/watchdog.h"

namespace fs = std::filesystem;

 // CliMenuProcessor class handles user input from the command line, maintains command history, and interacts with the MenuItem structure to execute commands based on user input.
 // Singleton class, only one instance should be created with reference to the root menu and prompt string.
class CliMenuProcessor {
private:
	CliMenuProcessor() {
		// Read previous command history from file
		//util::loadListFromFile(commandHistory, commandHistoryFile.string());
		itCmdHistory = commandHistory.begin();
	}

	std::string prompt;
	bool inEscSeq = false;
	std::list<std::string> commandHistory;
	std::list<std::string>::iterator itCmdHistory;
	std::string commandBuffer;
	fs::path commandHistoryFile = "";
	bool commandHistoryLoaded = false;
	int currentCursorPosition = 0;

	MenuItem rootMenu =
	{
	.name = "root",
	.description = "Root Menu",
	.subMenus = {
		{
			.name = "quit",
			.description = "Exit the program!!",
			.valType = MenuItemValueTypes::NONE,
			.executeCommand = [](MenuItemValueTypes vt, const std::string& argument) {
				ThreadManager::GetInstance().StopAllThreads();
				std::cout << "\n\nRx Quit Command: Quiting the Program!\n";
			},
		},
		{
			.name = "help",
			.description = "Show program help",
			.subMenus = {
				{
					.name = "all",
					.description = "Show all commands",
					.valType = MenuItemValueTypes::NONE,
					.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) { this->Help(true); },
				},
			},
			.valType = MenuItemValueTypes::NONE,
			.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {
				if (argument == "all") {
					this->Help(true);
				} else {
				    this->Help();
				}
			}
		},
		{
			.name = "history",
			.description = "Show command history",
			.subMenus = {
				{
				.name = "purge",
				.description = "Purge command history",
				.subMenus = {},
				.valType = MenuItemValueTypes::NONE,
				.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->commandHistory.clear(); },
				},
			},
			.valType = MenuItemValueTypes::ANY,
			.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument)
				{
					std::cout << "\nCommand History:";
					int i = 0;
					for (auto& cmd : commandHistory) {
						std::cout << "\n[" << i++ << "] : " << cmd;
					}
				},
		}
	},
	.valType = MenuItemValueTypes::SUBMENU,
	};


	void moveCursor(int delta);
	void processEscSeq(char ch);
	void updateCommandHistory(const std::string& command);

public:
	static CliMenuProcessor& GetInstance(std::string history_file = "") {
		static CliMenuProcessor instance; // Guaranteed to be destroyed and instantiated on first use.
	
		if (!instance.commandHistoryLoaded) {
			if (history_file.empty() && instance.commandHistoryFile.string().empty()) {
				history_file = ".default_command_history";
			}
			// Convert history_file to fs::path
			fs::path history_path(history_file);
			if (!history_path.has_parent_path() || history_path.parent_path().empty()) {
				// No path applied, save to current directory
				fs::path hpath = fs::current_path() / ".history";
				if (!std::filesystem::is_directory(hpath)) {
					std::filesystem::create_directory(hpath);
				}
				history_path = hpath / history_path;
			}
			if (instance.commandHistoryFile != history_path) {
				if (std::filesystem::exists(history_path)) {
					std::cout << "\nLoading command history from file: " << history_path << std::endl;
				}
				else {
					std::cout << "\nCommand history file does not exist: " << history_path << std::endl;
					std::cout << "Creating new command history file: " << history_path << std::endl;
					std::ofstream outfile(history_path);
					if (!outfile) {
						std::cerr << "\n\nError creating command history file: " << history_path << std::endl;
					}
					else {
						outfile << "# Command history file created on " << std::chrono::system_clock::now() << "\n";
						outfile.close();
					}
				}
				instance.commandHistoryFile = history_path;
			}

			// Load command history from the specified file
			util::loadListFromFile(instance.commandHistory, history_path.string());
			instance.itCmdHistory = instance.commandHistory.begin();
			instance.commandHistoryLoaded = true;
		}
		return instance;
	}

	void SetPrompt(const std::string& newPrompt) {
		prompt = newPrompt;
		std::cout << "\n" << prompt;
	}

	void AddSubMenu(const MenuItem& newMenu) {
		rootMenu.AddSubMenu(newMenu);
	}
	void DynamicAddSubMenu(std::vector<std::string> cmdHierarchy, const MenuItem& newMenu) {
		rootMenu.DynamicAddSubMenu(cmdHierarchy, newMenu);
	}

	static void GetUserInput_thread();
	void ProcessChar(unsigned char ch);
	void Help() { rootMenu.Help(); }
	void Help(bool traverse) { rootMenu.Help(traverse); }

	void SaveCommandHistory() {
		util::saveListToFile(commandHistory, ".command_history");
	}

	~CliMenuProcessor() {
		// Save command history to file
		util::saveListToFile(commandHistory, ".command_history");
	}

};

#pragma once
/*------------------------------------------------------------------
 * Logger Header File
 *
 * Class to perform logging
 * 
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <chrono>
#include <format>
#include <iostream>
#include <fstream>
#include <string_view>
#include <source_location>
#include <filesystem>
#include <mutex>
#include <ctime>
#include <iomanip>

#include "magic_enum.hpp"
#include "cli/CliMenu.h"

#define LOG_INST MyLogger::GetInstance()
#define LOG LOG_INST.Log

enum class LoggerVerbosity : int {
	NOTSET = 0,
	DEBUG = 10,
	INFO = 20,
	WARNING = 30,
	ERROR = 40,
	CRITICAL = 50
};

//constexpr std::string_view to_string(LoggerVerbosity v) { 
//	switch (v) {
//	case LoggerVerbosity::CRITICAL: return "CRITICAL";
//	case LoggerVerbosity::ERROR: return "ERROR";
//	case LoggerVerbosity::WARNING: return "WARNING";
//	case LoggerVerbosity::INFO: return "INFO";
//	case LoggerVerbosity::DEBUG: return "DEBUG";
//	case LoggerVerbosity::NOTSET: return "NOTSET";
//    default: 
//		std::string level = std::format("LVL={}", static_cast<int>(v));
//		return level;
//	}
//}

class MyLogger {
private:
	std::mutex mtx; // Mutex to protect access to configuration data
	std::chrono::time_point<std::chrono::system_clock> last_log_time;
	bool console_logging_enabled = true;
	MyLogger() {
		last_log_time = std::chrono::system_clock::now();
		SetLogFile("default.log");
	}
public:
	LoggerVerbosity verbosity = LoggerVerbosity::DEBUG;
	std::string	log_filename;
	std::ofstream log_file;
	const MenuItem cli_menu = {
		.name = "logger",
		.description = "Commands to control logger",
		.subMenus = {
			{
				.name = "on", 
				.description = "Turn ON log to file, arg is filename", 
				.subMenus = {}, 
				.valType = MenuItemValueTypes::STRING,
				.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->SetLogFile(argument); },
			},
			{
				.name = "off", 
				.description = "Turn OFF file logging", 
				.subMenus = {}, 
				.valType = MenuItemValueTypes::NONE,
				.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->CloseLogFile(); },
			},
			{
				.name = "verbosity", 
				.description = "Set logging verbosity level, \n\t\t\targ is level (higher value == higher verbosity, i.e. CRITICAL=50", 
				.subMenus = {}, 
				.valType = MenuItemValueTypes::ANY,
				.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->SetVerbosityStr(argument); },
			},
			{
				.name = "console",
				.description = "Set console logging [on | off]",
				.subMenus = {},
				.valType = MenuItemValueTypes::ANY,
				.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->SetConsole(argument); },
			},
		},

		.valType = MenuItemValueTypes::SUBMENU,
		.executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->cli_menu.Help(); }
	};

	MyLogger(const MyLogger&) = delete; // Delete copy constructor
	MyLogger& operator=(const MyLogger&) = delete; // Delete copy assignment operator
	MyLogger(MyLogger&&) = delete; // Delete move constructor
	MyLogger& operator=(MyLogger&&) = delete; // Delete move assignment operator

	~MyLogger() {
		CloseLogFile();
	}

	static MyLogger& GetInstance() {
		static MyLogger instance; // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}

	void SetConsole(const std::string& arg) {
		if (arg.empty()) {
			std::cout << "\nConsole logging is currently turned " << (console_logging_enabled ? "ON" : "OFF") << "\n";
		} else if (arg == "on") {
			if (console_logging_enabled) {
				std::cout << "Console logging already on!!\n";
				return;
			}
			console_logging_enabled = true;
			std::cout << "\nTurning Console logging ON\n";
			Log(LoggerVerbosity::CRITICAL, "Turning Console logging ON");
		}
		else if (arg == "off") {
			if (!console_logging_enabled) {
				std::cout << "Console logging already off!!\n";
				return;
			}
			console_logging_enabled = false;
			std::cout << "\nTurning Console logging OFF\n";
			Log(LoggerVerbosity::CRITICAL, "Turning Console logging OFF");
		}
		else {
			std::cout << "\nUnhandled argument to Console Logging: " << arg << "\n";
		}
	}

	void SetVerbosityStr(const std::string& arg) {
		std::lock_guard<std::mutex> lock(mtx);
		if (!arg.empty()) {
			try {
				int v_int = std::stoi(arg);
				verbosity = static_cast<LoggerVerbosity>(v_int);
			}
			catch (const std::exception& e) {
				try {
					LoggerVerbosity conv_verbosity = magic_enum::enum_cast<LoggerVerbosity>(arg).value_or(LoggerVerbosity::NOTSET);
					if (conv_verbosity == LoggerVerbosity::NOTSET) {
						std::cerr << "Invalid verbosity level: " << arg << ". Setting to NOTSET.\n";
					}
					else {
						verbosity = conv_verbosity;
					}
				}
				catch (const std::exception& e2) {
					std::cerr << "Invalid verbosity level: " << arg << ". Setting to NOTSET.\n";
					std::cerr << "Error details: " << e.what() << "\n";
					std::cerr << "Error details: " << e2.what() << "\n";
				}
			}
		}
		std::cout << "Log verbosity set to: "<< static_cast<int>(verbosity) << "(" << std::string(magic_enum::enum_name(verbosity)) << ")\n";
	}

	int SetLogFile(const std::string& fn) {
		std::string filename = fn;
		if (filename.find('/') == std::string::npos) { // No path applied, save to current directory
			filename = "./" + filename;
		}

		if (filename == log_filename) return 0; // nothing to do as it is the same file
		CloseLogFile();
		std::unique_lock<std::mutex> lock(mtx);
		log_file.open(filename, std::ios::out | std::ios::trunc);
		if (!log_file.is_open()) {
			std::cerr << "Failed to open log file: " << filename << std::endl;
			return -1;
		}
		log_filename = filename;
		lock.unlock();
		Log(LoggerVerbosity::CRITICAL, std::format("Opening log file={}", log_filename));
		return 0;
	}

	void CloseLogFile() {
		std::unique_lock<std::mutex> lock(mtx);
		if (log_file.is_open()) {
			lock.unlock();
			Log(LoggerVerbosity::CRITICAL, std::format("Closing log file={}", log_filename));
			lock.lock();
			log_file.flush();
			log_file.close();
		}
		log_filename.clear();
		std::cout << "\nLog file closed.\n";
	}

	bool IsLogFileOpen() const {
		return log_file.is_open();
	}

	void Log(LoggerVerbosity v, std::string msg, const std::source_location location = std::source_location::current()) {
		if (v >= verbosity) {
			std::lock_guard<std::mutex> lock(mtx);
			auto now = std::chrono::system_clock::now();
			std::chrono::duration<float> deltaT = now - last_log_time;
			
			std::time_t now_c = std::chrono::system_clock::to_time_t(now);
			// Convert to local time (thread-safe version)
			std::tm local_tm = {};
#if defined(_WIN32)
			localtime_s(&local_tm, &now_c); // Windows secure version
#else
			localtime_r(&now_c, &local_tm); // POSIX thread-safe version
#endif
			//std::string td = std::format("{:%F %T}", local_tm);
			//std::string td = util::tmToString(local_tm);
			
			std::ostringstream oss;
			oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S")
				//<< "("<< deltaT.count() << ")"
				<< " : ";
			std::string td = oss.str();


			auto lvl = std::string(magic_enum::enum_name(v));
			std::string short_name = std::filesystem::path(location.file_name()).filename().string();
			std::stringstream ss;
			ss << std::endl << td << lvl << ": "
				<< short_name << " @ " << location.line() << ": "
				<< msg;
			if (console_logging_enabled) std::cout << ss.str();
			if (log_file.is_open()) {
				log_file << ss.str();
				if (deltaT.count() > 20.0f) {
					log_file.flush();
					last_log_time = now;
					//std::cout << "Flushing log file after " << deltaT.count() << " seconds since last flush.\n";
				}
			} else {
				std::cerr << "Log file is not open. Unable to write log message: " << ss.str() << std::endl;
			}
		}
	}
};

#pragma once
/*------------------------------------------------------------------
 * Watchdog Header File
 *
 * Defines functions and classes for monitoring and managing system processes.
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <mutex>
#include <ctime>
#include <chrono>
#include <atomic>
#include <thread>

#include "logger.h"
#include "threadManager.h"

using namespace my_logger;

class Watchdog {
private:
	std::atomic<bool> activity{ true };
	std::atomic<bool> local_force_stop{ false };
	double timeout_s = 10;
	bool onTimeoutForceExit = false;
	std::vector<std::function<void()>> onTimeoutCallbacks;

	Watchdog() {
		// Initialize watchdog resources, such as starting monitoring threads or setting up timers.
	}

public:
	const MenuItem cli_menu =
    	{
		.name = "watchdog",
		.description = "Commands to control watchdog",
		.subMenus = {
			{
				.name = "timer", 
				.description = "Set watchdog timer in seconds", 
				.subMenus = {}, 
				.valType = MenuItemValueTypes::DOUBLE,
				.executeCommand = [](MenuItemValueTypes vt, const std::string& argument) {
					Watchdog::GetInstance().SetTimeout(std::stod(argument));
					LOG(my_logger::LoggerVerbosity::ERR, "Watchdog Timeout has been set to " + std::to_string(Watchdog::GetInstance().GetTimeout()) + " seconds");
					std::cout << "\nWatchdog Timeout has been set to " << Watchdog::GetInstance().GetTimeout() << " seconds\n";
				},
			},
		},

		.valType = MenuItemValueTypes::SUBMENU,
		.executeCommand = [](MenuItemValueTypes vt, const std::string& argument) {
			std::cout << "\nWatchdog Timeout is " << Watchdog::GetInstance().GetTimeout() << " seconds\n";
		},
	};


	static Watchdog& GetInstance() {
		static Watchdog instance; // Guaranteed to be destroyed and instantiated on first use.
		return instance;
	}
	Watchdog(Watchdog const&) = delete; // Delete copy constructor
	Watchdog& operator=(Watchdog const&) = delete; // Delete copy assignment operator
	Watchdog(Watchdog&&) = delete; // Delete move constructor
	Watchdog& operator=(Watchdog&&) = delete; // Delete move assignment operator

	double GetTimeout() { return timeout_s;}
	void SetTimeout(double to) {
		timeout_s = to;
	}
	static void monitor_thread() {
		ThreadManager& TM = ThreadManager::GetInstance();
		auto last_active_time = std::chrono::high_resolution_clock::now();
		bool warning_issued = false;
		while (!TM.force_stop && !GetInstance().local_force_stop) {
			if (GetInstance().activity) {
				GetInstance().activity = false;
				warning_issued = false;
				last_active_time = std::chrono::high_resolution_clock::now();
			}
			else {
				auto now = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double> deltaTime = now - last_active_time;
				if (deltaTime.count() > GetInstance().timeout_s*0.80) {
					if (deltaTime.count() > GetInstance().timeout_s) {
						LOG(LoggerVerbosity::CRITICAL, "\n\nWatchdog Timer Expired with inactivity at " + std::to_string(deltaTime.count()));
						// Force closing the program
						TM.StopAllThreads();

						if (GetInstance().onTimeoutCallbacks.size() > 0) {
							for (auto& callback : GetInstance().onTimeoutCallbacks) {
								callback();
							}
						}
						if (GetInstance().onTimeoutForceExit) {
							LOG(LoggerVerbosity::CRITICAL, "\nForcing program exit due to watchdog timeout.\n");
							std::cout << "\nForcing program exit due to watchdog timeout.\n";
							exit(0);
						}
						break;
					}
					if (!warning_issued) {
						warning_issued = true;
						LOG(LoggerVerbosity::WARNING, "Watchdog timer is about to expire in " +
							std::to_string(GetInstance().timeout_s - deltaTime.count()) + " seconds" +
							" : Watchdog Time=" + std::to_string(deltaTime.count()) + " TO=" +
							std::to_string(GetInstance().timeout_s)
						);
						LOG(LoggerVerbosity::CRITICAL, "WARNING: Watchdog timer is about to expire in "
							+ std::to_string(GetInstance().timeout_s - deltaTime.count()) 
							+ " seconds : Watchdog Time=" + std::to_string(deltaTime.count()) 
							+ " TO=" + std::to_string(GetInstance().timeout_s)
						);
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	void StopMonitoring() {
		// Stop the monitoring process and clean up resources.
		local_force_stop = true;
	}
	void CheckIn() {
		activity = true;
	}

	void SetOnTimeoutCallback(std::function<void()> callback) {
		onTimeoutCallbacks.push_back(callback);
	}
	void SetOnTimeoutForceExit(bool forceExit) {
		onTimeoutForceExit = forceExit;
	}
};
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


class Watchdog {
private:
	std::atomic<bool> activity{ true };
	std::atomic<bool> local_force_stop{ false };
	double timeout_s = 10;

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
						std::cout<< "\n\nWatchdog Timer Expired with inactivity at " << deltaTime.count() << "\n";

						// Force closing the program
						TM.StopAllThreads();
						break;
					}
					if (!warning_issued) {
						warning_issued = true;
						std::cout << "\n\nWARNING: Watchdog timer is about to expire in " 
							<< (GetInstance().timeout_s - deltaTime.count()) << " seconds";
						std::cout << " : Watchdog Time=" <<  deltaTime.count() << " TO="<< GetInstance().timeout_s << "\n";
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			//LOG(LoggerVerbosity::CRITICAL, std::format("Watchdog Timeout: activity={}", GetInstance().activity));
			//std::cout << "\nWatchdog Timeout: activity=" << GetInstance().activity << "\n";
		}
	}

	void StopMonitoring() {
		// Stop the monitoring process and clean up resources.
		local_force_stop = true;
	}
	void CheckIn() {
		activity = true;
	}
};
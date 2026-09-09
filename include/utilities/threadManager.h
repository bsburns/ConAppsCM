#pragma once
/*------------------------------------------------------------------
 * Thread Manager Header File
 * 
 * Manages the creation, execution, and termination of threads in the application.
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <thread>
#include <map>
#include "logger.h"

#ifdef _WIN32
#include <windows.h>
#endif


class ThreadManager {
private:
	std::map<std::string, std::thread> threads;

	ThreadManager() {}
public:
	std::atomic<bool> force_stop{ false };

	static ThreadManager& GetInstance() {
		static ThreadManager instance; // Guaranteed to be destroyed and instantiated on first use.
		return instance;
	}
	ThreadManager(ThreadManager const&) = delete; // Delete copy constructor
	ThreadManager& operator=(ThreadManager const&) = delete; // Delete copy assignment operator

	void StartThread(std::string threadName, std::function<void()> threadFunc) {
		auto it = threads.find(threadName);
		if (it != threads.end()) {
			std::cerr << "\nThread with name \"" << threadName << "\" already exists. Cannot start another thread with the same name.\n";
			return;
		}
		threads[threadName] = std::thread(threadFunc);
	}

	void StopThread(std::string threadName) {
		auto it = threads.find(threadName);
		if (it != threads.end()) {
			if (it->second.joinable()) {
				it->second.join();
				LOG(my_logger::LoggerVerbosity::CRITICAL, "Thread \"" + threadName + "\" has been stopped and joined successfully.");
			}
			threads.erase(it);
		} else {
			std::cerr << "\nNo thread with name \"" << threadName << "\" found. Cannot stop non-existent thread.\n";
		}
	}

	void StopAllThreads() {
		LOG(my_logger::LoggerVerbosity::CRITICAL, "Setting Force Stop to TRUE");
		force_stop.store(true);
	}
	void ForceStopAllThreads() {
        for (auto& [name, thread] : threads) {
            LOG(my_logger::LoggerVerbosity::CRITICAL, "Force stopping thread: " + name);
#ifdef _WIN32
            // WARNING: TerminateThread is dangerous and should be avoided if possible.
            // Prefer cooperative cancellation. This is for emergency use only.
            TerminateThread(thread.native_handle(), 0);
#else
            // On non-Windows platforms, there is no direct equivalent.
            // You may need to implement cooperative cancellation.
            LOG(my_logger::LoggerVerbosity::CRITICAL, "TerminateThread is not supported on this platform.");
#endif
        }
    }

	void WaitAllThreads() {
		for (auto& [name, thread] : threads) {
			LOG(my_logger::LoggerVerbosity::CRITICAL, "Waiting for thread: " + name);
			if (thread.joinable()) {
				thread.join();
				LOG(my_logger::LoggerVerbosity::INFO, "Thread \"" + name + "\" has been joined successfully.");
			}
		}
		threads.clear();
	}
};
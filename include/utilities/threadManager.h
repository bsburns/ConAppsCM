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
			std::cout << "\nThread with name " << threadName << " already exists. Cannot start another thread with the same name.\n";
			return;
		}
		threads[threadName] = std::thread(threadFunc);
	}

	void StopThread(std::string threadName) {
		auto it = threads.find(threadName);
		if (it != threads.end()) {
			if (it->second.joinable()) {
				it->second.join();
				std::cout << "\nThread " << threadName << " has been stopped and joined successfully.\n";
			}
			threads.erase(it);
		} else {
			std::cout << "\nNo thread with name " << threadName << " found. Cannot stop non-existent thread.\n";
		}
	}

	void StopAllThreads() {
		force_stop = true;
	}

	void WaitAllThreads() {
		for (auto& [name, thread] : threads) {
			if (thread.joinable()) {
				thread.join();
				std::cout << "\nThread " << name << " has been joined successfully.\n";
			}
		}
		threads.clear();
	}
};
#pragma once
/*------------------------------------------------------------------
 * Root Simulator Header File
 *
 * Main Simulation engine
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <map>
#include <thread>
#include <atomic>

#include "event.h"
#include "utilities/statistics.h"

#include "Devices.h"
#include "DevScheduler.h"

class RootSim {
private:
	std::map<int, std::shared_ptr<DeviceBase>> Devices; // Map of device ID to device instance
	std::multimap<uint64_t, std::shared_ptr<EventBase>> Events; // Map of event ID to event instance, key is event time
    std::ofstream stats_file;

    // Stats
    StatisticsNone rootStats;
    StatisticsNone trafGenStats;
    StatisticsNone stripesStats;

	// Simulation control variables
    std::mutex mtx; // Mutex to protect access to configuration data
	uint64_t currentTime_ns = 0; // Current simulation time in nanoseconds
    std::atomic<bool> running{ false };
	std::thread simulationThread;

    void buildSimulatiomStructures();
    void simulation_thread();

public:
	RootSim();

    ~RootSim() {
        LOG(LoggerVerbosity::INFO, "RootSim Object being destroyed");
        Stop();
        Devices.clear();
		Events.clear();
	}

    void Start(double duration_sec = 0, uint32_t num_events = 0)
    {
        LOG(LoggerVerbosity::INFO, std::format("Starting simulation: duration={} seconds, events={} ", duration_sec, num_events));
        if (duration_sec > 0) {
            //std::this_thread::sleep_for(std::chrono::duration<double>(duration_sec));
        } else {
            //while (true) {
            //    //std::this_thread::sleep_for(std::chrono::seconds(1));
            //}
        }
        LOG(LoggerVerbosity::INFO, "Building Simulation structures...");
        buildSimulatiomStructures();
		simulationThread = std::thread(&RootSim::simulation_thread, this);

        LOG(LoggerVerbosity::INFO, "Simulation started");
    }
    void Stop()
    {
        if (running) {
            LOG(LoggerVerbosity::INFO, "Stopping simulation");
            running = false;
            if (simulationThread.joinable()) {
                simulationThread.join();
            }
        }
	}

    void ScheduleEvent(std::shared_ptr<EventBase> event)
    {
        std::lock_guard<std::mutex> lock(mtx);
        Events.emplace(event->timestamp_ns, event);
        LOG(LoggerVerbosity::INFO, 
            std::format("Added event to Events, cnt={}, evt={}", Events.size(), event->ToStringBase()));
	}

    std::string ToString()
    {
        std::string str = std::format("RootSimStr:") ;
        return str;
    }

	friend std::ostream&operator<<(std::ostream& os, const RootSim& sim) {
        os << "RootSim{" << 
			"}";
        return os;
    }
};

class SimulationManager {
private:
    RootSim* Sim;
    SimulationManager() : Sim(nullptr) {}

public:
    MenuItem cli_menu = {
        .name = "simulation",
        .description = "Commands to control configuration",
        .subMenus = {
            {
                .name = "start",
                .description = "Start the simulation, RTC",
                .subMenus = {
                    {
                        .name = "duration",
                        .description = "Start the simulation for a specific duration in seconds, \n\t\t\te.g. 'start duration 10' to run for 10 seconds",
                        .subMenus = {},
                        .valType = MenuItemValueTypes::DOUBLE,
                        .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->Start(std::stod(argument)); },
                    },
                    {
                        .name = "events",
                        .description = "Start the simulation for a specific number of events, \n\t\t\te.g. 'start events 10' to run for 10 events",
                        .subMenus = {},
                        .valType = MenuItemValueTypes::UINT,
                        .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->Start(0, std::stoul(argument)); },
                    },
                },
                .valType = MenuItemValueTypes::NONE,
                .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->Start(); },
            },
            {
                .name = "stop",
                .description = "Stop the simulation",
                .subMenus = {},
                .valType = MenuItemValueTypes::NONE,
                .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->Stop(); },
            },
            {
                .name = "pause",
                .description = "Pause the simulation",
                .subMenus = {},
                .valType = MenuItemValueTypes::NONE,
                .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->Pause(); },
            },
        },
        .valType = MenuItemValueTypes::SUBMENU,
        .executeCommand = [](MenuItemValueTypes vt, const std::string& argument) {},
    };

    static SimulationManager& GetInstance() {
        static SimulationManager instance; // Guaranteed to be destroyed and instantiated on first use
        return instance;
    }
    SimulationManager(const SimulationManager&) = delete; // Delete copy constructor
    SimulationManager& operator=(const SimulationManager&) = delete; // Delete copy assignment operator
    SimulationManager(SimulationManager&&) = delete; // Delete move constructor
    SimulationManager& operator=(SimulationManager&&) = delete; // Delete move assignment operator

    MenuItem GetCliMenu() {
        cli_menu.AddSubMenu(SimConfigManager::GetInstance().cli_menu);

        return cli_menu;
    }
    void Start(double duration_sec = 0.0, uint32_t num_events = 0) {
        if (Sim != nullptr) {
            LOG(LoggerVerbosity::WARNING, "Simulation is already running. Restarting with new parameters.");
            Stop();
        }
        LOG(LoggerVerbosity::INFO, std::format("Starting simulation: duration={} sec, num_events={}", duration_sec, num_events));
        Sim = new RootSim();
        Sim->Start(duration_sec, num_events);
    }
    void Stop() {
        LOG(LoggerVerbosity::INFO, "SimMgr: Stopping simulation");
        if (Sim != nullptr) {
            Sim->Stop();
            delete Sim;
            Sim = nullptr;
        }
    }
    void Pause() {
        LOG(LoggerVerbosity::INFO, "Pausing simulation");
    }

    void ScheduleEvent(std::shared_ptr<EventBase> event) {
        if (Sim != nullptr) {
            Sim->ScheduleEvent(event);
            LOG(LoggerVerbosity::DEBUG, std::format("Scheduled Event: {}", event->ToStringBase()));
        } else {
            LOG(LoggerVerbosity::WARNING, "Cannot schedule event: Simulation is not running");
        }
	}
};


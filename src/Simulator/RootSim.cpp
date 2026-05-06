/*------------------------------------------------------------------
 * Root Simulator Source File
 *
 * Main Simulation engine
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "RootSim.h"
#include "DevTrafficGenerator.h"
#include "DevStripe.h"
#include "DevOutput.h"

// Declare Globals
dev_id_t DeviceBase::globalDeviceIdCounter = 0;
uint64_t EventBase::globalEventIdCounter = 0;
uint64_t EventBase::globalPacketIdCounter = 0;


RootSim::RootSim() 
    : rootStats("SIM", nullptr), 
    trafGenStats("TRAFFIC_GEN", &rootStats),
    stripesStats("STRIPES", &rootStats)
{
    std::cout << "\n\nCreate Root: rootStats=" << rootStats.FormatChildren();
}

void RootSim::buildSimulatiomStructures() {
    // Reset IDs
    EventBase::ResetIDs();
    DeviceBase::globalDeviceIdCounter = 0; // reset device IDs back to 0

    //rootStats.clear();

    // Build the simulation structures based on the configuration
    auto& cfgMgr = SimConfigManager::GetInstance();
    auto& rootSimCfg = cfgMgr.GetConfig().rootCfg;
    auto& simCfg = cfgMgr.GetConfig().rootCfg;
    auto& schedCfg = cfgMgr.GetConfig().schedulerCfg;   
    auto& stripeCfg = cfgMgr.GetConfig().stripeCfg;
    auto& outputCfg = cfgMgr.GetConfig().outputCfg;

    // Create Output Device 
    auto outputDev = std::make_shared<DeviceOutputBase>("OUTPUT", outputCfg, &rootStats);
    Devices.emplace(outputDev->GetDeviceID(), outputDev);

    // Create Scheduler
    std::shared_ptr<DeviceBase> devBasePtr = nullptr;
    std::shared_ptr<DeviceSchedulerBase> schedBasePtr = nullptr;
    switch (schedCfg.Mode) {
    case SchedulerMode::FIXED_STRIPES:
        schedBasePtr = std::make_shared<DeviceSchedulerFixed>("FIXED_SCHED", schedCfg, &rootStats);
        break;
    case SchedulerMode::DYNAMIC_STRIPES:
        schedBasePtr = std::make_shared<DeviceSchedulerDynamic>("DYNAMIC_SCHED", schedCfg, &rootStats);
        break;
    case SchedulerMode::STRM_AFFINITY:
        schedBasePtr = std::make_shared<DeviceSchedulerAffinity>("STREAM_AFFINITY_SCHED", schedCfg, &rootStats);
        break;
    default:
        LOG(LoggerVerbosity::ERROR, std::format("Unknown Scheduler mode!! mode={}", std::string(magic_enum::enum_name(schedCfg.Mode))));
        return;
    }
    devBasePtr = schedBasePtr;
    if (devBasePtr == nullptr) {
        LOG(LoggerVerbosity::ERROR, std::format("Scheduler device was not created!! mode={}", std::string(magic_enum::enum_name(schedCfg.Mode))));
        return;
    }
    auto SchedID = schedBasePtr->GetDeviceID();
    Devices.emplace(SchedID, schedBasePtr);
    std::cout << "\nCreated Scheduler: " << schedBasePtr->ToString();

    // Create Stripe Devices 
    for (int i = 0; i < schedCfg.MaxNumberStripes; i++) {
        std::string name = std::format("STRIPE-{}", i); 
        auto stripePtr = std::make_shared<DeviceStripeNominal>
            (name, stripeCfg, SchedID, outputDev->GetDeviceID(),  & stripesStats);
        Devices.emplace(stripePtr->GetDeviceID(), stripePtr);
        schedBasePtr->AddStripe(stripePtr->GetDeviceID());
    }
    // Setup Traffic Streams
    auto& dataStrmCfgList = cfgMgr.GetConfig().dataStrmCfgList;
    for (auto& dataStrmCfg : dataStrmCfgList) {
		auto strm = std::make_shared<DeviceTrafficGeneratorBase>(dataStrmCfg.StreamName, dataStrmCfg, SchedID, &trafGenStats);
		Devices.emplace(strm->GetDeviceID(), strm);
        LOG(LoggerVerbosity::INFO, std::format("Data Stream Config: {}", dataStrmCfg.ToString()));
	}
    LOG(LoggerVerbosity::INFO, std::format("Number of Devices: {}", Devices.size()));

    // Create Statistics SnapShot event
    if (stats_file.is_open()) stats_file.close();
    auto ssFile = TestName + "_stats.csv";
    stats_file.open(ssFile, std::ios::out | std::ios::trunc);
    stats_file << "time_ns, " << rootStats.GetColumnHeader() << "\n";
    auto ss_event = std::shared_ptr<EventBase>(new EventBase(EventType::STATS_SNAPSHOT, DeviceType::SIMULATOR, EventAction::COLLECT_STATS, 0));
    ss_event->timestamp_ns = rootSimCfg.StatisticsUpdateInterval_ns; 
    ScheduleEvent(ss_event);

    std::cout << "End Build Devices: " << rootStats.FormatChildren();
}

void RootSim::simulation_thread() {
    Watchdog& watchdog = Watchdog::GetInstance();
	ThreadManager& TM = ThreadManager::GetInstance();
	running = true;
    // Main simulation loop
    while (running && !TM.force_stop) {
        // Process events, update device states, and interact with the scheduler
        if (Events.size() > 0) {
            // Process events in order of their scheduled time
            auto it = Events.begin();
            currentTime_ns = it->first;
            while (it != Events.end() && it->first <= currentTime_ns) {
                auto event = it->second;
                auto dev = it->second->GetDeviceID();

                // Remove event from Events
                it = Events.erase(it);
                if (dev == 0 && event->GetEventType() == EventType::STATS_SNAPSHOT && event->GetDeviceType() == DeviceType::SIMULATOR && event->GetEventAction() == EventAction::COLLECT_STATS) {
                    LOG(LoggerVerbosity::INFO, std::format("Stat Snapshot Event: {}", event->ToStringBase()));
                    if (stats_file.is_open()) {
                        stats_file << event->timestamp_ns <<", " << rootStats.GetColumnData() << "\n";
                    }
                }

                auto dev_it = Devices.find(dev);
                if (dev_it == Devices.end()) {
                    LOG(LoggerVerbosity::ERROR, std::format("SimThread: Events device ID does not exist: devId={}, evt={}", dev, event->ToStringBase()));
                } else {
                    LOG(LoggerVerbosity::DEBUG, std::format("Processing Event: {}", event->ToStringBase()));
                    dev_it->second->ExecuteEvent(event);
                }
            }
        } else {
            LOG(LoggerVerbosity::DEBUG, "No events to process at this time");
            std::this_thread::sleep_for(std::chrono::milliseconds(10000)); // Simulate time passing
        }
    }
}

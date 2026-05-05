/*------------------------------------------------------------------
 * Simulator Device Traffic Generator Source File
 *
 * Defines functions and classes for Traffic Generator device
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <random>

#include "event.h"
#include "DevTrafficGenerator.h"
#include "RootSim.h"


DeviceTrafficGeneratorBase::DeviceTrafficGeneratorBase(const std::string& name, 
    DataStreamConfig config, 
    dev_id_t schedId, 
    StatisticsBase* prtStats)
        : DeviceBase(name, DeviceType::TRAFFIC_GEN), 
        SchedulerDeviceId(schedId), 
        cfg(config),
        StatsTxPackets(name+".TX", prtStats)
{
    // Validate configuration
    if (cfg.MinBurstSizePackets > cfg.MaxBurstSizePackets) {
        LOG(LoggerVerbosity::ERROR, std::format("{}: Config Validation:: Min Burst Size > Max BurstSize", GetDeviceName()));
        cfg.MaxBurstSizePackets = cfg.MinBurstSizePackets;
    }
    
    double txTime = cfg.PacketRate_pps;
    txTime = 1/txTime;
    txTime *= 1'000'000'000;
    timeToTransmitPacket_ns = static_cast<uint64_t>(txTime);

    if (cfg.StartTime_ns > 0) {
        nextPacketTransmitTime_ns = cfg.StartTime_ns;
    } else {
        nextPacketTransmitTime_ns = timeToTransmitPacket_ns; // Start immediately
	}
	ScheduleNextPacket(nextPacketTransmitTime_ns);
}

int DeviceTrafficGeneratorBase::ScheduleNextPacket(uint64_t currTime_ns) 
{
    uint64_t nextTime = currTime_ns + 10;

    auto event = std::shared_ptr<EventPacket>(new EventPacket(DeviceBase::GetDeviceID(), cfg.PacketSize_bytes));
	event->timestamp_ns = nextTime;
    event->SetDeviceId(SchedulerDeviceId); // send packet to scheduler
    event->SetDeviceType(DeviceType::SCHEDULER);
    event->SetFromDeviceId(GetDeviceID());
    DeviceBase::simMgr.ScheduleEvent(event);
	StatsTxPackets.addValue(cfg.PacketSize_bytes);
    LOG(LoggerVerbosity::INFO, std::format("{}:ScheduleNextPacket: {}", GetDeviceName(), event->ToString()));

    // now create event for next packet
    if (cfg.NumberPacketsToGenerate > 0 && StatsTxPackets.count() >= cfg.NumberPacketsToGenerate) {
        LOG(LoggerVerbosity::INFO, std::format("{}: Device id={} has generated the configured number of packets ({}), stopping generation.", GetDeviceName(), GetDeviceID(), StatsTxPackets.count()));
        return 1; // Stop generating packets
	}
    if (cfg.MaxBurstSizePackets > 1) {
        if (currBurstSize == 0) {
            const unsigned int seed = 12345;
            std::mt19937 gen(seed);
            //std::random_device rd;
            //std::mt19937 gen(rd()); // Mersenne Twister engine
            std::uniform_int_distribution<> dist(cfg.MinBurstSizePackets, cfg.MaxBurstSizePackets);
            currBurstSize = dist(gen);
            burstCount = 0;
            LOG(LoggerVerbosity::INFO, std::format("{}:ScheduleNextPacket: Creating Burst: size={}", GetDeviceName(), currBurstSize));
        }
        currBurstSize--;
        burstCount++;
        if (currBurstSize == 0) {
            nextTime += burstCount*timeToTransmitPacket_ns; // Wait to schedule next gen event until after current Burst completes
        }
        else {
            nextTime += 15;
        }
    } else {
        nextTime += timeToTransmitPacket_ns;
    }
    LOG(LoggerVerbosity::INFO, std::format("{}:ScheduleNextPacket: CurrBurst={} count={}", GetDeviceName(), currBurstSize, burstCount));
    
    // Schedule next generation event
    auto gen_event = std::shared_ptr<EventTrafGen>(new EventTrafGen(DeviceBase::GetDeviceID()));
	gen_event->timestamp_ns = nextTime;
    gen_event->SetFromDeviceId(GetDeviceID());
    DeviceBase::simMgr.ScheduleEvent(gen_event);
    LOG(LoggerVerbosity::INFO, std::format("{}:ScheduleNextPacket: Gen Evt={}", GetDeviceName(), gen_event->ToString()));

    return 0;
}

void DeviceTrafficGeneratorBase::ExecuteEvent(std::shared_ptr<EventBase> event)
{
    if (event->GetDeviceType() != DeviceType::TRAFFIC_GEN) {
        LOG(LoggerVerbosity::ERROR, std::format("{}:ExecuteEvent:: Invalid Device Type evt={}", GetDeviceName(), event->ToStringBase()));
        return;
    }
    if (event->GetEventType() != EventType::TRAFFIC_GENERATOR || event->GetEventAction() != EventAction::GENERATE_PKT) {
        LOG(LoggerVerbosity::ERROR, std::format("{}:ExecuteEvent:: Invalid event evt={}", GetDeviceName(), event->ToStringBase()));
        return;
    }
    ScheduleNextPacket(event->timestamp_ns);
}

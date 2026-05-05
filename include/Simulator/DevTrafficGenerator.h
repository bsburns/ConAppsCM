#pragma once
/*------------------------------------------------------------------
 * Simulator Device Traffic Generator Header File
 *
 * Defines functions and classes for Traffic Generator device
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "SimCommon.h"
#include "Devices.h"

class DeviceTrafficGeneratorBase : public DeviceBase {
private:
    DataStreamConfig cfg;
	uint64_t timeToTransmitPacket_ns = 0;
	uint64_t nextPacketTransmitTime_ns = 0;
	int SchedulerDeviceId = 0; // ID of the scheduler device to send packet to when generated

    // State
    int burstCount = 0;
    int currBurstSize = 0;
    uint64_t lastTxTime = 0;

    // Statistics
    StatisticsBasic<int> StatsTxPackets;


    int ScheduleNextPacket(uint64_t currTime_ns);

public:
    DeviceTrafficGeneratorBase(const std::string& name, DataStreamConfig config, dev_id_t schedId, StatisticsBase* prtStats);
    
    const DataStreamConfig& GetConfig() const { return cfg; }
    
    void ExecuteEvent(std::shared_ptr<EventBase> event);
    
    const std::string ToString() {
        std::string str = DeviceBase::ToString();
        str += std::format("Strm=[ID={}, cfg={}]", DeviceBase::GetDeviceID(), cfg.ToString());
        return str;
    }

};



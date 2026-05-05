#pragma once
/*------------------------------------------------------------------
 * Simulator Device Output Header File
 *
 * Defines functions and classes for Output device
 * Aggregates all packets from all stripes into a single pipe 
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "SimCommon.h"
#include "Devices.h"


class DeviceOutputBase : public DeviceBase {
private:
    const OutputConfig& cfg;
	std::ofstream log_file;

        // Statistics

public:
    // Statistics
    StatisticsBasic<int> StatsRxPackets;

    DeviceOutputBase(
        const std::string& name, 
        const OutputConfig& config, 
        StatisticsBase* prtStats);

    void ExecuteEvent(std::shared_ptr<EventBase> event) override;

    std::string ToString() const {
        std::stringstream ss;
        ss << GetDeviceName()<< "={"  << "}";
        return ss.str();
    }

    friend std::ostream&operator<<(std::ostream& os, const DeviceOutputBase& output) {
        os << output.ToString();
        return os;
    }
    virtual ~DeviceOutputBase() {
        if (log_file.is_open()) log_file.close();
    };
};






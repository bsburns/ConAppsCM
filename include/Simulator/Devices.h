#pragma once
/*------------------------------------------------------------------
 * Simulator Devices Header File
 *
 * Defines Base class for all simulation devices, 
 * such as Traffic Generators, Schedulers, Stripers, etc
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <mutex>
#include <ctime>
#include <chrono>
#include <atomic>

#include "SimCommon.h"

class EventBase; // Forward declaration
class SimulationManager; // Forward declaration

class DeviceBase {
private:
	dev_id_t deviceId = 0;
	std::string deviceName;
	DeviceType deviceType = DeviceType::NONE;
public:
	static dev_id_t globalDeviceIdCounter;
	SimulationManager& simMgr;

    DeviceBase(const std::string& name, DeviceType dType);

	dev_id_t GetDeviceID() const { return deviceId; }
	const std::string& GetDeviceName() const { return deviceName; }
	const DeviceType GetDeviceType() const { return deviceType; }
    const std::string ToString() {
		return std::format("Device[ID={}, Name={}, Type={}]", deviceId, deviceName, std::string(magic_enum::enum_name(deviceType)));
	}
	virtual void ExecuteEvent(std::shared_ptr<EventBase> event) = 0;
	virtual ~DeviceBase() = default;
};


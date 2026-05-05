/*------------------------------------------------------------------
 * Simulator Devices Source File
 *
 * Defines Base class for all simulation devices, 
 * such as Traffic Generators, Schedulers, Stripers, etc
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */


#include "Devices.h"
#include "RootSim.h"

DeviceBase::DeviceBase(const std::string& name, DeviceType dType) 
    : deviceName(name), deviceType(dType) , simMgr(SimulationManager::GetInstance())
{
    deviceId = ++globalDeviceIdCounter;
}



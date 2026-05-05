#pragma once
/*------------------------------------------------------------------
 * Simulator Device Stripe Header File
 *
 * Defines functions and classes for Stripe device
 * Stripe Device converts Packets to FEC protocol
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "SimCommon.h"
#include "Devices.h"

class DeviceStripeBase : public DeviceBase {
private:
    const StripeConfig& cfg;
    dev_id_t SchedID;
    dev_id_t OutputID;

public:
    // State
    uint64_t TargetPacketInterval_ns;
    uint64_t AbsMaxPacketInterval_ns;
    bool RxPacketInTargetInterval = false;

    // Statistics
    StatisticsBasic<int> StatsTxPackets;
    StatisticsBasic<int> StatsRxPackets;

    DeviceStripeBase(
        const std::string& name, 
        const StripeConfig& config, 
        dev_id_t schedID,
        dev_id_t outputID,
        StatisticsBase* prtStats);

    dev_id_t GetSchedulerId() { return SchedID; }
    dev_id_t GetOutputId() { return OutputID;}
    void ExecuteEvent(std::shared_ptr<EventBase> event) override;

    std::string ToString() const {
        std::stringstream ss;
        ss << GetDeviceName()<< "={"  << "}";
        return ss.str();
    }

    friend std::ostream&operator<<(std::ostream& os, const DeviceStripeBase& stripe) {
        os << stripe.ToString();
        return os;
    }
    virtual ~DeviceStripeBase() = default;
};

class DeviceStripeNominal : public DeviceStripeBase {
public:
    DeviceStripeNominal (
        const std::string& name, 
        const StripeConfig& config, 
        dev_id_t schedID, 
        dev_id_t outputID,
        StatisticsBase* prtStats);
    void ExecuteEvent(std::shared_ptr<EventBase> event) override;

    ~DeviceStripeNominal() = default;
};





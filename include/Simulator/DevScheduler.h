#pragma once
/*------------------------------------------------------------------
 * Simulator Device Scheduler Header File
 *
 * Defines functions and classes for Scheduler device
 * Scheduler determines which Stripe packets should be sent too
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <deque>

#include "SimCommon.h"
#include "Devices.h"
#include "RootSim.h"

class DeviceSchedulerBase : public DeviceBase {
private:
    const SchedulerConfig& cfg;
    int CurrentNumberStripes = 0;
    std::vector<dev_id_t> StripeIDs;

public:
    // Statistics
    StatisticsBasic<int> StatsTxPackets;
    StatisticsBasic<int> StatsRxPackets;

    DeviceSchedulerBase(
        const std::string& name, 
          const SchedulerConfig& config, 
          StatisticsBase* prtStats)
        : DeviceBase(name, DeviceType::SCHEDULER),
        cfg(config),
        StatsTxPackets(name+".TX", prtStats),
        StatsRxPackets(name+".RX", prtStats)
    {
    }

    virtual void AddStripe(dev_id_t id) {
        StripeIDs.emplace_back(id);
    }

    void ExecuteEvent(std::shared_ptr<EventBase> event) override {
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << "DevSchedulerBase{curr_stripes=" << CurrentNumberStripes << "}";
        return ss.str();
    }

    friend std::ostream&operator<<(std::ostream& os, const DeviceSchedulerBase& rr) {
        os << rr.ToString();
        return os;
    }
    virtual ~DeviceSchedulerBase() = default;
};

class DeviceSchedulerFixed : public DeviceSchedulerBase {
public:
    DeviceSchedulerFixed (
        const std::string& name, 
        const SchedulerConfig& config,
        StatisticsBase* prtStats)
        : DeviceSchedulerBase(name, config, prtStats)
    {
    }
    void ExecuteEvent(std::shared_ptr<EventBase> event) override;
};

class DeviceSchedulerDynamic : public DeviceSchedulerBase {
private:

    // State
    std::vector<dev_id_t> ActiveStripes;
    std::vector<dev_id_t> InactiveStripes;
    std::vector<dev_id_t>::iterator actIT;

public:
    DeviceSchedulerDynamic (const std::string& name, 
      const SchedulerConfig& config,
      StatisticsBase* prtStats)
      : DeviceSchedulerBase(name, config, prtStats)
    {
        actIT = ActiveStripes.begin();
    }

    void AddStripe(dev_id_t id) override {
        InactiveStripes.emplace_back(id);
        DeviceSchedulerBase::AddStripe(id);
    }

    dev_id_t GetNextStripeID() {
        if (ActiveStripes.size() == 0) {
            ActiveStripes.push_back(InactiveStripes.front());
            InactiveStripes.erase(InactiveStripes.begin());
            actIT = ActiveStripes.begin();
        }
        dev_id_t stripeID = *actIT;
        actIT++;
        if (actIT == ActiveStripes.end()) actIT = ActiveStripes.begin();
        return stripeID;
    }

    void ExecuteEvent(std::shared_ptr<EventBase> event) override;
};

class DeviceSchedulerAffinity : public DeviceSchedulerBase {
public:
    DeviceSchedulerAffinity (const std::string& name, 
      const SchedulerConfig& config,
      StatisticsBase* prtStats)
      : DeviceSchedulerBase(name, config, prtStats)
    {
    }
    void ExecuteEvent(std::shared_ptr<EventBase> event) override;
};




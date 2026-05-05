/*------------------------------------------------------------------
 * Simulator Device Stripe Source File
 *
 * Defines functions and classes for Stripe device
 * Stripe Device converts Packets to FEC protocol
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "Devices.h"
#include "DevStripe.h"
#include "RootSim.h"

DeviceStripeBase::DeviceStripeBase(
    const std::string& name, 
    const StripeConfig& config, 
    dev_id_t schedID,
    dev_id_t outputID,
    StatisticsBase* prtStats)
    : DeviceBase(name, DeviceType::STRIPER),
    cfg(config),
    SchedID(schedID),
    OutputID(outputID),
    StatsTxPackets(name+".TX", prtStats),
    StatsRxPackets(name+".RX", prtStats)
{
    TargetPacketInterval_ns = static_cast<uint64_t>(1'000'000'000 / cfg.TargetPacketRate_pps);
    AbsMaxPacketInterval_ns = static_cast<uint64_t>(1'000'000'000 / cfg.AbsMaxPacketRate_pps);
    auto event = std::shared_ptr<EventStriper>(new EventStriper(DeviceBase::GetDeviceID()));
    event->SetDeviceAction(EventAction::CHECK_IDLE);
    event->SetFromDeviceId(GetDeviceID());
    event->timestamp_ns = TargetPacketInterval_ns;
    DeviceBase::simMgr.ScheduleEvent(event);
}

void DeviceStripeBase::ExecuteEvent(std::shared_ptr<EventBase> event) {
    LOG(LoggerVerbosity::INFO, std::format("{}: Exec: rx evt=[{}]", GetDeviceName(), event->ToStringBase()));

    if (event->GetEventType() == EventType::PACKET) {
        if (std::shared_ptr<EventPacket> pktEvt = std::dynamic_pointer_cast<EventPacket>(event)) {
            StatsRxPackets.addValue(pktEvt->packet_length);

            // Send Packet to Output Device
            event->SetDeviceId(DeviceStripeBase::GetOutputId());
            event->SetDeviceType(DeviceType::OUTPUT);
            event->SetFromDeviceId(GetDeviceID());
            event->timestamp_ns += AbsMaxPacketInterval_ns;
            LOG(LoggerVerbosity::INFO, std::format("{}:Exec: Send packet to Output device evt={}", GetDeviceName(), pktEvt->ToStringBase()));
            DeviceBase::simMgr.ScheduleEvent(event);
            StatsTxPackets.addValue(pktEvt->packet_length);
        } else {
            LOG(LoggerVerbosity::ERROR, std::format("{}: Unable to cast event to EventPacket: evt={}", GetDeviceName(), event->ToStringBase()));
        }
    }
}

DeviceStripeNominal::DeviceStripeNominal (
    const std::string& name, 
    const StripeConfig& config, 
    dev_id_t schedID, 
    dev_id_t outputID,
    StatisticsBase* prtStats)
    : DeviceStripeBase(name, config, schedID, outputID, prtStats)
{
}

void DeviceStripeNominal::ExecuteEvent(std::shared_ptr<EventBase> event) {
    LOG(LoggerVerbosity::INFO, std::format("{}:Execute Event: evt=[{}]", GetDeviceName(), event->ToStringBase()));
    if (event->GetEventType() == EventType::PACKET) {
        if (std::shared_ptr<EventPacket> pktEvt = std::dynamic_pointer_cast<EventPacket>(event)) {
            StatsRxPackets.addValue(pktEvt->packet_length);
            event->SetDeviceId(DeviceStripeBase::GetOutputId());
            event->SetFromDeviceId(GetDeviceID());
            event->SetDeviceType(DeviceType::STRIPER);
            event->timestamp_ns += AbsMaxPacketInterval_ns;
            LOG(LoggerVerbosity::INFO, std::format("{}:Exec: Send packet to Output device evt={}", GetDeviceName(), pktEvt->ToStringBase()));
            DeviceBase::simMgr.ScheduleEvent(event);

        } else {
            LOG(LoggerVerbosity::ERROR, std::format("{}: Unable to cast event to EventPacket: evt={}", GetDeviceName(), event->ToStringBase()));
        }
    }
}





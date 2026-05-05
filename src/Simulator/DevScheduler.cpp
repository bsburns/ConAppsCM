/*------------------------------------------------------------------
 * Simulator Device Scheduler Source File
 *
 * Defines functions and classes for Scheduler device
 * Scheduler determines which Stripe packets should be sent too
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "DevScheduler.h"
#include "RootSim.h"


void DeviceSchedulerFixed::ExecuteEvent(std::shared_ptr<EventBase> event) {
    LOG(LoggerVerbosity::INFO, std::format("{}:Execute Event: evt={}", GetDeviceName(), event->ToStringBase()));
    if (event->GetEventType() == EventType::PACKET) {
        if (std::shared_ptr<EventPacket> pktEvt = std::dynamic_pointer_cast<EventPacket>(event)) {
            StatsRxPackets.addValue(pktEvt->packet_length);
        } else {
            LOG(LoggerVerbosity::ERROR, std::format("{}: Unable to cast event to EventPacket: evt={}", GetDeviceName(), event->ToStringBase()));
        }
    }
}


void DeviceSchedulerDynamic::ExecuteEvent(std::shared_ptr<EventBase> event) {
    LOG(LoggerVerbosity::INFO, std::format("{}:Exec: evt={}", GetDeviceName(), event->ToStringBase()));
    if (event->GetEventType() == EventType::PACKET) {
        if (std::shared_ptr<EventPacket> pktEvt = std::dynamic_pointer_cast<EventPacket>(event)) {
            StatsRxPackets.addValue(pktEvt->packet_length);
            auto stripeID = GetNextStripeID();
            event->SetDeviceId(stripeID);
            event->SetFromDeviceId(GetDeviceID());
            event->SetDeviceType(DeviceType::STRIPER);
            event->timestamp_ns += 20;
            LOG(LoggerVerbosity::INFO, std::format("{}:Exec: Send packet to Stripe={} evt={}", GetDeviceName(), stripeID, pktEvt->ToStringBase()));
            DeviceBase::simMgr.ScheduleEvent(event);
        } else {
            LOG(LoggerVerbosity::ERROR, std::format("{}: Unable to cast event to EventPacket: evt={}", GetDeviceName(), event->ToStringBase()));
        }
    }
}


void DeviceSchedulerAffinity::ExecuteEvent(std::shared_ptr<EventBase> event) {
    LOG(LoggerVerbosity::INFO, std::format("{}:Execute Event: evt={}", GetDeviceName(), event->ToStringBase()));
    if (event->GetEventType() == EventType::PACKET) {
        if (std::shared_ptr<EventPacket> pktEvt = std::dynamic_pointer_cast<EventPacket>(event)) {
            StatsRxPackets.addValue(pktEvt->packet_length);
        } else {
            LOG(LoggerVerbosity::ERROR, std::format("{}: Unable to cast event to EventPacket: evt={}", GetDeviceName(), event->ToStringBase()));
        }
    }
}





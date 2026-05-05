#pragma once
/*------------------------------------------------------------------
 * Simulator Event Header File
 *
 * Defines functions and classes for Simulation Events
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <mutex>
#include <ctime>
#include <chrono>
#include <atomic>
#include "magic_enum.hpp"
#include "SimTypes.h"

class EventBase {
private:
	static uint64_t globalEventIdCounter;
	static uint64_t globalPacketIdCounter;
	uint64_t eventId = 0;
    EventType eventType;
    DeviceType deviceType;
	EventAction action;
	dev_id_t deviceId; // ID of the device associated with this event (if applicable)
    dev_id_t fromDeviceId; // ID of device that sent sent event

public:
    uint64_t timestamp_ns; // Epoch time in nanoseconds
    EventBase() : EventBase(EventType::NONE, DeviceType::NONE, EventAction::NONE, -1) {}
    EventBase(EventType type_, DeviceType deviceType_, EventAction action_, uint32_t devId)
        : eventType(type_), 
        deviceType(deviceType_), 
        action(action_) ,
        deviceId(devId)
    {
        eventId = ++globalEventIdCounter;
        timestamp_ns = 0;
    }
    EventBase(EventBase const&) = delete; // Delete copy constructor
    EventBase& operator=(EventBase const&) = delete; // Delete copy assignment operator
	EventBase(EventBase&&) = delete; // Delete move constructor
	EventBase& operator=(EventBase&&) = delete; // Delete move assignment operator

    static void ResetIDs() {
        globalEventIdCounter = 0;
        globalPacketIdCounter = 0;
    }
	uint64_t GetEventId() const { return eventId; }
	void SetDeviceId(dev_id_t id) { deviceId = id; }
    dev_id_t GetDeviceID() const { return deviceId; }
	void SetFromDeviceId(dev_id_t id) { fromDeviceId = id; }
    dev_id_t GetFromDeviceID() const { return fromDeviceId; }
    DeviceType GetDeviceType() const { return deviceType; }
    void SetDeviceType(DeviceType val) { deviceType = val; }
    EventType GetEventType() const { return eventType; }
    void SetEventType(EventType val) { eventType = val; }
    EventAction GetEventAction() const { return action; }
    void SetDeviceAction(EventAction val) { action = val; }
    uint64_t GetNewPacketId() {return ++globalPacketIdCounter;}


    std::string ToStringBase() const {
        return "EvtID=" + std::to_string(eventId)+
            //" DevType=" + std::string(magic_enum::enum_name(deviceType)) +
            " EvtType=" + std::string(magic_enum::enum_name(eventType)) +
            " Action=" + std::string(magic_enum::enum_name(action)) +
            " ToDevID=" + std::to_string(deviceId) +
            " DevType=" + std::string(magic_enum::enum_name(deviceType)) +
            " time=" + std::to_string(timestamp_ns);
    }

    virtual ~EventBase() = default;
};

template <typename Derived> 
class EventBaseCRTP : public EventBase {
public:
    EventBaseCRTP() : EventBase() {}
    EventBaseCRTP(EventType type_, DeviceType deviceType_, EventAction action_, uint32_t devId=-1) 
        : EventBase(type_, deviceType_, action_, devId) {}

    std::string ToString() {
        return static_cast<Derived*>(this)->ToString();
    }
};

class EventPacket : public EventBaseCRTP<EventPacket> {
    using Base = EventBaseCRTP<EventPacket>;
    uint64_t PacketId;
	uint32_t TrafficGenDeviceId; // ID of the device that generated this packet event
    dev_id_t StripeDeviceId = -1;
public:
    // State
    uint32_t FEC_SequenceNum = 0;
    int FEC_row = -1;
    int FEC_col = -1;
    bool FEC_cell = false;
    bool FillCell = false;


    uint32_t packet_length; // Packet Length in Bytes
    EventPacket(uint32_t trfGenId, uint32_t length) 
        : TrafficGenDeviceId(trfGenId), 
        packet_length(length),
        PacketId(Base::EventBase::GetNewPacketId()),
        Base(EventType::PACKET, DeviceType::NONE, EventAction::NONE) 
    {
    }

    uint64_t GetPacketId() const { return PacketId;}
    dev_id_t GetTrafGenId() const { return TrafficGenDeviceId; }
    void SetStripeID(dev_id_t id) { StripeDeviceId = id; }
    dev_id_t GetStripeID() { return StripeDeviceId; }

    std::string ToString() {
        return "EventPacket: " + EventBase::ToStringBase() +
            " PktId=" + std::to_string(PacketId) +
            " length=" + std::to_string(packet_length);
    }
};

class EventTrafGen : public EventBaseCRTP<EventTrafGen> {
    using Base = EventBaseCRTP<EventTrafGen>;
	uint32_t TrafficGenDeviceId; // ID of the device that generated this packet event
public:
    EventTrafGen(uint32_t trfGenId) 
        : TrafficGenDeviceId(trfGenId), 
        Base(EventType::TRAFFIC_GENERATOR, DeviceType::TRAFFIC_GEN, EventAction::GENERATE_PKT, trfGenId) {}
    
    std::string ToString() {
        return "EventTrafGen: "+ EventBase::ToStringBase();
    }
};

class EventStriper : public EventBaseCRTP<EventStriper> {
    using Base = EventBaseCRTP<EventStriper>;
	uint32_t StriperId; // ID of the device that generated this event
public:
    EventStriper(uint32_t striperId) 
        : StriperId(striperId), 
        Base(EventType::DEVICE_ACTION, DeviceType::STRIPER, EventAction::CHECK_IDLE, striperId) {}
    
    std::string ToString() {
        return "EventStriper: "+ EventBase::ToStringBase();
    }
};



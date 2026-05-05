#pragma once
/*------------------------------------------------------------------
 * Simulator Tyoes Header File
 *
 * Defines enums and data types used by simulator
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <nlohmann/json.hpp>

enum class DeviceType : int {
    NONE,
    TRAFFIC_GEN,
    SCHEDULER,
	STRIPER,
    OUTPUT,
    SIMULATOR, // Base Simulator
};

enum class EventType : int {
    NONE,
    PACKET,
    DEVICE_ACTION,
    TRAFFIC_GENERATOR,
    STATS_SNAPSHOT,
};

enum class EventAction : int {
    NONE,
    GENERATE_PKT,
    RX_PACKET,
    CHECK_IDLE,
    COLLECT_STATS,
};

enum class SchedulerMode : int {
    FIXED_STRIPES, // # of Stripes constant incoming packets are evenly distributed
    DYNAMIC_STRIPES, // # of stripes based on current load, streams are evenly distributed
    STRM_AFFINITY, // Streams are assigned to stripe
};
NLOHMANN_JSON_SERIALIZE_ENUM(SchedulerMode, {
    {SchedulerMode::FIXED_STRIPES,   "FIXED_STRIPES"},
    {SchedulerMode::DYNAMIC_STRIPES, "DYNAMIC_STRIPES"},
    {SchedulerMode::STRM_AFFINITY,   "STRM_AFFINITY"},
})

enum class FEC_Mode : int {
    NONE, // No FEC
    OptionA, // Column Only FEC
    OptionB, // Row and Column FEC
};
NLOHMANN_JSON_SERIALIZE_ENUM(FEC_Mode, {
    {FEC_Mode::NONE,    "NONE"},
    {FEC_Mode::OptionA, "OptionA"},
    {FEC_Mode::OptionB, "OptionB"},
})

typedef int dev_id_t;

extern std::string TestName;


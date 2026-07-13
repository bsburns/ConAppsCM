#pragma once
/*------------------------------------------------------------------
 * bStriperConfig.h
 *
 * Boost based Striper header
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

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

struct StripeConfig {
    FEC_Mode Mode = FEC_Mode::NONE;     // FEC Mode
    uint32_t Parm_L_cols = 5;           // FEC Parameter L - number of columns in matrix
    uint32_t Parm_D_rows = 100;         // FEC Parameter D - number of rows in matrix
    bool AutoFillEnable = false;        // Enable Auto fill, if packet rate drops below TargetPacketRate_pps
    double AbsMaxPacketRate_pps = 0;    // Absolute maximum packet Rate that a stripe can generate
	double MinPacketRate_pps = 0;       // Once actual Rate drops below this amount inject fill packets to maintain this rate
    std::string StripeReceiverIpAddress = ""; // IP Address that stripe data will be sent to
    int StartUdpSrcPortNumber = 6'000;  // Starting UDP Source Port number for first Stripe (Stripe number == Source Port - this value)
    int StartUdpDstPortNumber =5'000;   // DPORT= N>5000(even), Col FEC=N+2 Row FEC=N+4
    std::string DeStripeIpAddress = ""; // IP Address that stripe data will be sent to after destriping
    int DeStripeDstPortNumber = 7'000;  // After Destriping, the packet will be sent to UDP Destination Port
    int RxMaxOutstandingsBlocks = 10;   // Number of blocks that have not completed before giving up
    double RxBlockTimeout_sec = 1.5;    // Time to wait for block to complete in seconds
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StripeConfig,
    Mode,
    Parm_L_cols,
    Parm_D_rows,
    AbsMaxPacketRate_pps,
    MinPacketRate_pps,
    StripeReceiverIpAddress,
    StartUdpSrcPortNumber,
    StartUdpDstPortNumber,
    DeStripeIpAddress,
    DeStripeDstPortNumber,
    RxMaxOutstandingsBlocks,
    RxBlockTimeout_sec
    )

struct SchedulerConfig {
    SchedulerMode Mode = SchedulerMode::DYNAMIC_STRIPES;
    int MinNumberStripes = 1; // Minimum number of active stripes
    int MaxNumberStripes = 16; // Maximum number of active stripes
    uint32_t RevaluationInterval_ns = 100; // period between modifing the number of active stripes
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SchedulerConfig, Mode, MinNumberStripes, MaxNumberStripes, RevaluationInterval_ns)


struct AllStriperConfig {
    SchedulerConfig schedulerCfg;
    StripeConfig stripeCfg;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AllStriperConfig, schedulerCfg, stripeCfg)

#pragma once
/*------------------------------------------------------------------
 * SXTP_common.h
 *
 * Space X Thruput Test Common header file
 *
 * August 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <bit> // Required for std::bit_cast
#include <span>
#include <chrono>

#include "logger.h"
using namespace my_logger;


#define VERSION "0.1"

enum class UdpSendMode : int {
    NOTSET = 0,
    MESSAGE = 1,
    SEND_FILE = 2,
    PACKET = 3,
    TEST_DATA_MODE = 4
};

inline std::string to_engineering(double value, int precision = 2) {
    if (value == 0.0) return "0.0";

    // 1. Calculate the standard scientific exponent
    int exp_sci = std::floor(std::log10(std::abs(value)));

    // 2. Roll it back to the nearest lower multiple of 3
    int exp_eng = (exp_sci >= 0) ? (exp_sci / 3) * 3 : ((exp_sci - 2) / 3) * 3;

    // 3. Adjust the mantissa value
    double mantissa = value / std::pow(10, exp_eng);

    // 4. Format into stringstream using fixed notation
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << mantissa << "e"
        << (exp_eng >= 0 ? "+" : "") << exp_eng;

    return ss.str();
}

inline auto steady_to_system(std::chrono::steady_clock::time_point target) {
    auto steady_now = std::chrono::steady_clock::now();
    auto system_now = std::chrono::system_clock::now();
    auto duration_passed = target - steady_now;
    return system_now + duration_passed;
}

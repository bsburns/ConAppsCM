#pragma once
/*------------------------------------------------------------------
 * bStripes.h
 *
 * Boost based packet handler to subdivide flows into Stripes and 
 * apply SMPTE FEC to packets
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <memory> // for std::unique_ptr

enum class StriperModeE : int {
    NOTSET = 0,
    TRANSMITTER = 1,
    RECEIVER = 2
};


// Now define the message using an interprocess vector for the payload
enum class DeqMsgType : int {
    NOTSET = 0,
    MESSAGE = 1,
    PACKET = 2,
    EXIT_PROCESS = 3
};

struct StripeDequeMessageNew; // forward declaration

class StripesManagerImpl;

class StripesManager {
public:
    StripesManager() = delete;
    StripesManager(AllStriperConfig* striper_config_);
    ~StripesManager();

    int Initialize(StriperModeE mode_, std::string path_, std::string args_);
    void WaitForComplete();

    // Rule of five (disable copy, allow move)
    StripesManager(const StripesManager&) = delete;
    StripesManager& operator=(const StripesManager&) = delete;
    StripesManager(StripesManager&&) noexcept;
    StripesManager& operator=(StripesManager&&) noexcept;

private:
    std::unique_ptr<StripesManagerImpl> impl; // Pointer to hidden implementation
};

class StripeProcess {
private:
    std::string name;

public:
    StripeProcess() = delete;
    StripeProcess(std::string name_);

    // Rule of five (disable copy, allow move)
    StripeProcess(const StripeProcess&) = delete;
    StripeProcess& operator=(const StripeProcess&) = delete;

    StripeProcess(StripeProcess&&) noexcept;
    StripeProcess& operator=(StripeProcess&&) noexcept;
};

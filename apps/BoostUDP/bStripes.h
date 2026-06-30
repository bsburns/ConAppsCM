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
#include <boost/interprocess/managed_shared_memory.hpp>
#include "bStriperConfig.h"
#include "PacketHeader/PacketHeader.h"


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

struct StripeShmDequeMessage; // forward declaration

class StripesManagerImpl;

class StripesManager {
public:
    StripesManager() = delete;
    StripesManager(AllStriperConfig* striper_config_);
    ~StripesManager();


    // Rule of five (disable copy, allow move)
    StripesManager(const StripesManager&) = delete;
    StripesManager& operator=(const StripesManager&) = delete;
    StripesManager(StripesManager&&) noexcept;
    StripesManager& operator=(StripesManager&&) noexcept;

    int Initialize(StriperModeE mode_, std::string path_, std::string args_);
    void SendMessage(std::string msg, int stripe_num=-1); // stripe_num == -1 means all stripes
    void SendPacket(PacketHeaders& pkt, std::vector<uint8_t>& data, std::size_t length);
    void SendExit(int stripe_num = -1); // stripe_num == -1 means all stripes
    void WaitForComplete();

private:
    std::unique_ptr<StripesManagerImpl> impl; // Pointer to hidden implementation
};

//class SMPTE_FEC_Engine; // Forward declaration

class SMPTE_FEC_Engine {
private:
    StriperModeE mode;
    AllStriperConfig* striperConfig;

public:
    SMPTE_FEC_Engine(StriperModeE mode_, AllStriperConfig* striper_config_)
        : mode(mode_)
        , striperConfig(striper_config_)
    {

    }
};

class StripeProcess {
private:
    std::string name;
    StriperModeE mode;
    AllStriperConfig* striperConfig;
    boost::interprocess::managed_shared_memory segment;
    SMPTE_FEC_Engine fecEngine;

    void ReceivedPacket(StripeShmDequeMessage& msg);
public:
    StripeProcess() = delete;
    StripeProcess(std::string name_, StriperModeE mode_, AllStriperConfig* striper_config_);

    // Rule of five (disable copy, allow move)
    StripeProcess(const StripeProcess&) = delete;
    StripeProcess& operator=(const StripeProcess&) = delete;

    StripeProcess(StripeProcess&&) noexcept;
    StripeProcess& operator=(StripeProcess&&) noexcept;

};

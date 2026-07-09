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
#include "bUDP-Client.h"
#include "statistics.h"
#include "common.h"
#include "FecEngine.h"


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
    void SendPacket(PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length);
    void ReceivePacket(UdpStriperPortE port_mode, PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length);
    void SendExit(int stripe_num = -1); // stripe_num == -1 means all stripes
    void WaitForComplete();

private:
    std::unique_ptr<StripesManagerImpl> impl; // Pointer to hidden implementation
};

class StripeProcess {
private:
    std::string name;
    std::string SharedDataStr;
    uint16_t stripe_num;
    StriperModeE mode;
    AllStriperConfig* striperConfig;
    boost::interprocess::managed_shared_memory segment;
    std::unique_ptr<SMPTE_FEC_Engine> fecEngine = nullptr;

    void ReceivedPacket(PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length);
public:
    StripeProcess() = delete;
    StripeProcess(std::string name_, uint16_t stripe_num_, StriperModeE mode_, AllStriperConfig* striper_config_);

    // Rule of five (disable copy, allow move)
    StripeProcess(const StripeProcess&) = delete;
    StripeProcess& operator=(const StripeProcess&) = delete;

    StripeProcess(StripeProcess&&) noexcept;
    StripeProcess& operator=(StripeProcess&&) noexcept;

};


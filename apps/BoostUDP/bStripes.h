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
#include "statistics.h"


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
    void SendPacket(PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length);
    void SendExit(int stripe_num = -1); // stripe_num == -1 means all stripes
    void WaitForComplete();

private:
    std::unique_ptr<StripesManagerImpl> impl; // Pointer to hidden implementation
};

enum class RTP_PayloadTypeE : uint8_t {
    NOTSET = 0,
    NO_FEC = 98,
    MODE_A_FEC = 97, // Column FEC
	MODE_B_FEC = 96,  // Row and Column FEC  
    FEC_DATAGRAM = 99,
    FILL_DATA = 100
};

class FEC_Packet {
public:
	PacketHeaders headers;
    PacketHeaderSMPTE fec_header;
    std::vector<uint8_t> payload;

    FEC_Packet() : payload(1550) { Clear(); }
	
    void Clear() { 
        headers.Clear(); 
        std::fill(payload.begin(), payload.end(), 0); 
        fec_header.Reset();
    }

    void AddPacket(const std::shared_ptr<PacketHeaderRTP> rtpHdr, const std::vector<uint8_t>& data, std::size_t length) {
        fec_header.payload_type_recovery ^= rtpHdr->payload_type;
		fec_header.sequence_base ^= rtpHdr->sequence_number;
		fec_header.timestamp_recovery ^= rtpHdr->timestamp;

        if (length > payload.size()) {
            throw std::runtime_error("Data length exceeds FEC payload size");
		}
        for (std::size_t i = 0; i < length && i < payload.size(); ++i) {
            payload[i] ^= data[i];
		}
    }
};

class SMPTE_FEC_Engine {
private:
	std::string owning_stripe_name;
    StriperModeE mode;
    AllStriperConfig* striperConfig;
    std::atomic<uint16_t> sequence_number{ 0 };
    RTP_PayloadTypeE base_payload_type = RTP_PayloadTypeE::NOTSET;
	uint16_t number_rows = 0; // Number of rows in the FEC matrix including the FEC row
	uint16_t number_columns = 0; // Number of columns in the FEC matrix including the FEC column
	uint32_t block_size = 0;
    uint16_t dport_data = 0;
	uint16_t dport_col_fec = 0;
    uint16_t dport_row_fec = 0;

    // State
    std::atomic<bool> block_started{ false };
    std::atomic<bool> idle{ true };
	FEC_Packet current_row_fec_packet;
	std::vector<FEC_Packet> current_column_fec_packets;

    // Statistics
    StatisticsBasic<uint64_t> StatsRxPackets;
    StatisticsBasic<uint64_t> StatsTxPackets;
    StatisticsBasic<uint64_t> StatsFillPackets;
    StatisticsBasic<uint64_t> StatsFecColPackets;
    StatisticsBasic<uint64_t> StatsFecRowPackets;

public:
    SMPTE_FEC_Engine(std::string owning_stripe_name_, StriperModeE mode_, AllStriperConfig* striper_config_);

	int PerformFEC(PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length);
};

class StripeProcess {
private:
    std::string name;
    StriperModeE mode;
    AllStriperConfig* striperConfig;
    boost::interprocess::managed_shared_memory segment;
    SMPTE_FEC_Engine fecEngine;

    void ReceivedPacket(PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length);
public:
    StripeProcess() = delete;
    StripeProcess(std::string name_, StriperModeE mode_, AllStriperConfig* striper_config_);

    // Rule of five (disable copy, allow move)
    StripeProcess(const StripeProcess&) = delete;
    StripeProcess& operator=(const StripeProcess&) = delete;

    StripeProcess(StripeProcess&&) noexcept;
    StripeProcess& operator=(StripeProcess&&) noexcept;

};

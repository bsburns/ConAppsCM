#pragma once
/*------------------------------------------------------------------
 * FecEngine.h
 *
 * FEC engine for TX/RX SMPTE FEC packet streams
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <memory> // for std::unique_ptr
#include <chrono>
#include <boost/asio.hpp>


#include "bStriperConfig.h"
#include "PacketHeader/PacketHeader.h"
#include "bUDP-Client.h"
#include "statistics.h"
#include "common.h"
#include "logger.h"

enum class RTP_PayloadTypeE : uint8_t {
    NOTSET       = 0,
    MODE_B_FEC   = 96,  // Row and Column FEC  
    MODE_A_FEC   = 97,  // Column FEC
    NO_FEC       = 98,
    FEC_DATAGRAM = 99,
    FILL_DATA    = 100
};

class FEC_Datagram {
public:
    PacketHeaders headers;
    std::shared_ptr<PacketHeaderRTP> rtp_header = nullptr;
    std::vector<uint8_t> payload;
    size_t payload_length = 0;
    
    FEC_Datagram() {
        headers.Clear();
        payload.clear();
    }
    FEC_Datagram(const std::shared_ptr<PacketHeaderRTP> rtpHdr_, const PacketHeaders& headers_, const std::vector<uint8_t>& data_, std::size_t length_)
        : rtp_header(rtpHdr_)
        , headers(headers_)
        , payload(data_)
        , payload_length(length_)
    {

    }
};

class FEC_Packet {
public:
    PacketHeaders headers;
    std::shared_ptr<PacketHeaderSMPTE> fec_header = nullptr;
    std::vector<uint8_t> payload;
    size_t payload_length = 0;
    bool first = true;

    FEC_Packet() 
        : payload(1550, 0) 
    { 
        fec_header = std::make_shared<PacketHeaderSMPTE>();
    }

    FEC_Packet(const std::shared_ptr<PacketHeaderSMPTE> fecHdr_, const PacketHeaders& headers_, const std::vector<uint8_t>& data_, std::size_t length_)
        : fec_header(fecHdr_)
        , headers(headers_)
        , payload(data_)
        , payload_length(length_)
    { }

    void Clear() {
        headers.Clear();
        std::fill(payload.begin(), payload.end(), 0);
        payload_length = 0;
        fec_header->Reset();
        first = true;
    }

    void AddPacket(const std::shared_ptr<PacketHeaderRTP> rtpHdr, const std::vector<uint8_t>& data, std::size_t length) {
        if (first) {
            fec_header->sequence_base = rtpHdr->sequence_number;
            first = false;
        }
        fec_header->extension_recovery ^= rtpHdr->extension;
        fec_header->CSRC_recovery ^= rtpHdr->CSRC_Count;
        fec_header->payload_type_recovery ^= rtpHdr->payload_type;
        fec_header->timestamp_recovery ^= rtpHdr->timestamp;
        fec_header->length_recovery ^= length;
        payload_length = std::max(payload_length, length);

        if (length > payload.size()) {
            throw std::runtime_error("Data length exceeds FEC payload size");
        }
        for (std::size_t i = 0; i < length && i < payload.size(); ++i) {
            payload[i] ^= data[i];
        }
    }

    void PrepForSend(RTP_PayloadTypeE pt, uint16_t offset, uint16_t NA) {
        std::shared_ptr<PacketHeaderRTP> rtp_hdr = std::make_shared<PacketHeaderRTP>();
        rtp_hdr->payload_type = static_cast<uint8_t>(pt);
        rtp_hdr->sequence_number = fec_header->sequence_base;
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

        rtp_hdr->timestamp = millis.count(); // Set appropriate timestamp
        rtp_hdr->sync_src = 0; // Set appropriate sync source
        headers.AddHeader(rtp_hdr, 0); // Add RTP header at the beginning
        headers.AddHeader(fec_header, -1);
    }

};


class SMPTE_FEC_Engine; // Forward Declaration

// class FEC_TX_Block - maintains data structures for FEC for transmitter
class FEC_TX_Block {
private:
    SMPTE_FEC_Engine* FEC_Engine;
    FEC_Packet row_fec;
    std::vector<FEC_Packet> col_fec;
public:
    FEC_TX_Block() {}
    FEC_TX_Block(SMPTE_FEC_Engine* FEC_Engine_);

    void AddPacket(const std::shared_ptr<PacketHeaderRTP> rtpHdr, const PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length, uint16_t seq_num);
};

// class FEC_RX_Block - maintains data structures for FEC for receiver
class FEC_RX_Block {
private:
    SMPTE_FEC_Engine* FEC_Engine;
    uint32_t block_num;
    bool rx_last_cell = false;
    bool rx_last_fec_row = false;
    bool rx_last_fec_col = false;
    std::vector<std::map<uint16_t, FEC_Datagram>> matrixOfDataPkts;
    std::map<uint16_t, FEC_Packet> row_fec;
    std::map<uint16_t, FEC_Packet> col_fec;
    std::chrono::time_point<std::chrono::system_clock> startTime;

public:
    FEC_RX_Block() {}
    FEC_RX_Block(SMPTE_FEC_Engine* FEC_Engine_, uint32_t block_num_);

    int ReceivePacket(const std::shared_ptr<PacketHeaderRTP> rtpHdr, const PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length);
    int ReceivedFecPacket(UdpStriperPortE port_mode, const std::shared_ptr<PacketHeaderRTP> rtpHdr, const std::shared_ptr<PacketHeaderSMPTE> fecHdr, const PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length);
    void CheckBlock();
};

class StripeProcess; // Forward declaration

class SMPTE_FEC_Engine {
public:
    StripeProcess* owning_process = nullptr;
    std::string owning_stripe_name;
    uint16_t stripe_num;
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
    boost::asio::io_context io_context_fec;
    std::atomic<bool> block_started{ false };
    std::atomic<bool> idle{ true };
    std::unique_ptr<FEC_TX_Block> fecTxBlock = nullptr;
    std::map<uint32_t, std::unique_ptr<FEC_RX_Block>> fecRxBlocks; // key = block number, inner vector indexed by cell#
    std::shared_ptr<UdpClient> udpClientData = nullptr;
    std::shared_ptr<UdpClient> udpClientColFEC = nullptr;
    std::shared_ptr<UdpClient> udpClientRowFEC = nullptr;

    // Statistics
    StatisticsRTM<uint64_t> StatsRxPackets;
    StatisticsRTM<uint64_t> StatsTxPackets;
    StatisticsRTM<uint64_t> StatsFillPackets;
    StatisticsRTM<uint64_t> StatsFecColPackets;
    StatisticsRTM<uint64_t> StatsFecRowPackets;
    StatisticsCodedRTM<uint64_t, FecEngineDropCodesE> StatsDropPackets;

public:
    SMPTE_FEC_Engine(StripeProcess* owning_, std::string owning_stripe_name_, uint16_t stripe_num, StriperModeE mode_, AllStriperConfig* striper_config_);

    int PerformFEC(PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length, bool fill = false);
    int ReceivePacket(PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length);
    void SendDataPacket(std::vector<uint8_t>& data, std::size_t length);
};



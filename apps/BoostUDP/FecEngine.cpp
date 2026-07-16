/*
 * FecEngine.cpp
 *
 * FEC engine for TX/RX SMPTE FEC packet streams
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include "FecEngine.h"
#include "utility.h"
#include "bStripes.h"


SMPTE_FEC_Engine::SMPTE_FEC_Engine(StripeProcess* owning_, std::string owning_stripe_name_, uint16_t stripe_num_, StriperModeE mode_, AllStriperConfig* striper_config_)
    : owning_process(owning_)
    , owning_stripe_name(owning_stripe_name_)
    , stripe_num(stripe_num_)
    , mode(mode_)
    , striperConfig(striper_config_)
    , StatsRxPackets(owning_stripe_name + ":FEC_Engine_RxPkts", nullptr)
    , StatsTxPackets(owning_stripe_name + ":FEC_Engine_TxPkts", nullptr)
    , StatsFillPackets(owning_stripe_name + ":FEC_Engine_FillPkts", nullptr, 1, true)
    , StatsFecColPackets(owning_stripe_name + ":FEC_Engine_FecColPkts", nullptr)
    , StatsFecRowPackets(owning_stripe_name + ":FEC_Engine_FecRowPkts", nullptr)
{
    ThreadManager& TM = ThreadManager::GetInstance();
    Watchdog& watchdog = Watchdog::GetInstance();

    StatsDropPackets.clear();

    // Initialize configuration parameters for FEC based on striperConfig
    number_columns = striperConfig->stripeCfg.Parm_L_cols;
    number_rows = striperConfig->stripeCfg.Parm_D_rows;
    switch (striperConfig->stripeCfg.Mode) {
    case FEC_Mode::NONE:
        base_payload_type = RTP_PayloadTypeE::NO_FEC;
        break;
    case FEC_Mode::OptionA: // Column only FEC
        base_payload_type = RTP_PayloadTypeE::MODE_A_FEC;
        break;
    case FEC_Mode::OptionB: // Row and Column FEC
        base_payload_type = RTP_PayloadTypeE::MODE_B_FEC;
        break;
    default:
        LOG(LoggerVerbosity::ERR, "Unknown FEC Mode in configuration");
        base_payload_type = RTP_PayloadTypeE::NOTSET;
        break;
    }
    block_size = number_rows * number_columns;
    dport_data = striperConfig->stripeCfg.StartUdpDstPortNumber;
    dport_col_fec = dport_data + 2;
    dport_row_fec = dport_data + 4;
    double target_packet_rate_period = 1.0;
    if (striperConfig->stripeCfg.MinPacketRate_pps > 0)
        target_packet_rate_period /= striperConfig->stripeCfg.MinPacketRate_pps;

    
    // Open UDP Clients for out-going FEC streams
    // DPORT= N>5000(even), Col FEC=N+2 Row FEC=N+4
    std::string ServerIP = "127.0.0.1";
    if (!striperConfig->stripeCfg.StripeReceiverIpAddress.empty()) ServerIP = striperConfig->stripeCfg.StripeReceiverIpAddress;

    std::string sport_data = std::to_string(striperConfig->stripeCfg.StartUdpSrcPortNumber + (stripe_num << 3));
    std::string sport_ColFEC = std::to_string(striperConfig->stripeCfg.StartUdpSrcPortNumber + (stripe_num << 3) + 2);
    std::string sport_RowFEC = std::to_string(striperConfig->stripeCfg.StartUdpSrcPortNumber + (stripe_num << 3) + 4);
    std::string dport_data = std::to_string(striperConfig->stripeCfg.StartUdpDstPortNumber);
    std::string dport_ColFEC = std::to_string(striperConfig->stripeCfg.StartUdpDstPortNumber + 2);
    std::string dport_RowFEC = std::to_string(striperConfig->stripeCfg.StartUdpDstPortNumber + 4);

    LOG(LoggerVerbosity::INFO, "Initializing FEC engine: Mode="
        + std::string(magic_enum::enum_name(striperConfig->stripeCfg.Mode))
        + " sport_data=" + sport_data
        + " dport_data=" + dport_data
        + " rows=" + std::to_string(number_rows)
        + " cols=" + std::to_string(number_columns)
        + " block_size=" + std::to_string(block_size)
    );
    if (mode == StriperModeE::TRANSMITTER) {
        udpClientData = std::make_shared< UdpClient>(io_context_fec, ServerIP, sport_data, dport_data, UdpSendMode::PACKET);
        udpClientColFEC = std::make_shared< UdpClient>(io_context_fec, ServerIP, sport_ColFEC, dport_ColFEC, UdpSendMode::PACKET);
        udpClientRowFEC = std::make_shared< UdpClient>(io_context_fec, ServerIP, sport_RowFEC, dport_RowFEC, UdpSendMode::PACKET);
        if (udpClientData == nullptr) {
            LOG(LoggerVerbosity::ERR, "Could not create UDP client for stripe data: Stripe=" + owning_stripe_name
                + " ServerIP=" + ServerIP
                + " SrcPort=" + sport_data
                + " DstPort=" + dport_data
            );
            return;
        } else {
            LOG(LoggerVerbosity::INFO, "Created UDP client for stripe data: Stripe=" + owning_stripe_name
                + " ServerIP=" + ServerIP
                + " SrcPort=" + sport_data
                + " DstPort=" + dport_data
            );
        }
        if (udpClientColFEC == nullptr) {
            LOG(LoggerVerbosity::ERR, "Could not create UDP client for stripe column FEC: Stripe=" + owning_stripe_name
                + " ServerIP=" + ServerIP
                + " SrcPort=" + sport_ColFEC
                + " DstPort=" + dport_ColFEC
            );
            return;
        }
        if (udpClientRowFEC == nullptr) {
            LOG(LoggerVerbosity::ERR, "Could not create UDP client for stripe row FEC: Stripe=" + owning_stripe_name
                + " ServerIP=" + ServerIP
                + " SrcPort=" + sport_RowFEC
                + " DstPort=" + dport_RowFEC
            );
            return;
        }

        // Tell UDP Server we are in Packet Mode:
        udpClientData->StartPacketMode();
        udpClientColFEC->StartPacketMode();
        udpClientRowFEC->StartPacketMode();

        fecTxBlock = std::make_unique<FEC_TX_Block>(this);
        if (fecTxBlock == nullptr) {
            LOG(LoggerVerbosity::ERR, "Could not create FEX Tx Block: Stripe=" + owning_stripe_name);
            return;
        }

        // Create IDLE Fill thread to inject fill packets when FEC block is idle
        TM.StartThread("IdleFill", [this, &watchdog, &TM, &target_packet_rate_period]() {
            std::chrono::duration<double> duration(target_packet_rate_period);

            LOG(LoggerVerbosity::CRITICAL, "Idle Fill Thread duration=" + std::to_string(target_packet_rate_period));

            while (!TM.force_stop.load()) {
                // Idle loop to fill FEC block 
                if (this->idle.load() && this->block_started.load()) {
                    LOG(LoggerVerbosity::INFO, "IdleFill: FEC block started but idle, injecting fill packets to maintain rate.");
                    // Inject fill packets here
                    std::vector<uint8_t> fill_packet(40, 0); // Fill packet is empty but it need to be large enough to hold headers (UDP & RTP)
                    PacketHeaders fill_headers;
                    auto ipHdr = std::make_shared<PacketHeaderIPv4>();
                    ipHdr->Version = 4;
                    ipHdr->IHL = 5; // 5 * 4 = 20 bytes
                    ipHdr->TOS = 0;
                    ipHdr->totalLength = 0;
                    ipHdr->TTL = 64;
                    ipHdr->Protocol = static_cast<uint8_t>(PacketHeaderType::UDP); // UDP
                    ipHdr->srcIP = 0;
                    ipHdr->dstIP = 0; // Destination IP can be set to 0 for now, or you can set it to a specific value if needed
                    fill_headers.AddHeader(ipHdr);
                    this->PerformFEC(fill_headers, fill_packet, 0, true);
                }
                this->idle.store(true);

                std::this_thread::sleep_for(duration);
            }
            LOG(LoggerVerbosity::CRITICAL, "Idle Fill Thread exiting");
            });
        LOG(LoggerVerbosity::INFO, "FEC Engine is in TRANSMITTER mode...completed creating UDP clients");

    } else {
        // RECEIVE FEC ENGINE
        LOG(LoggerVerbosity::INFO, "FEC Engine is in RECEIVER mode, not creating UDP clients for FEC streams.");
        fecRxBlocks.clear();
    }
}

int SMPTE_FEC_Engine::PerformFEC(PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length, bool fill)
{
    // Implement SMPTE FEC algorithm here
    block_started.store(true);
    idle.store(false);

    if (!fill) StatsRxPackets.addValue(length);
    else StatsFillPackets.addValue(length);

    // Get IP Header
    auto hdr = headers.headers.back();
    auto ip_hdr = std::dynamic_pointer_cast<PacketHeaderIPv4>(hdr);
    if (!ip_hdr) {
        LOG(LoggerVerbosity::ERR, "First header is not IPv4, cannot perform FEC: type="
            + std::string(magic_enum::enum_name(hdr->header_type)));
        return -1;
    }

    //  Packetize header with data
    if (headers.MakePacket(data, length) != 0) {
        LOG(LoggerVerbosity::ERR, "Failed to packetize data packet");
        return -2;
    }

    // Create RTP Header
    std::shared_ptr<PacketHeaderRTP> rtp_hdr = std::make_shared<PacketHeaderRTP>();
    rtp_hdr->payload_type = fill ? static_cast<uint8_t>(RTP_PayloadTypeE::FILL_DATA) : static_cast<uint8_t>(base_payload_type);
    rtp_hdr->sequence_number = this->sequence_number.fetch_add(1, std::memory_order_relaxed); // atomic increment
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);

    rtp_hdr->timestamp = millis.count(); // Set appropriate timestamp
    rtp_hdr->sync_src = ip_hdr->srcIP; // Set appropriate sync source
    headers.AddHeader(rtp_hdr, 0); // Add RTP header at the beginning

    if (!fill) {
        LOG(LoggerVerbosity::INFO, "Performing FEC on packet with length=" + std::to_string(length)
            + " RTP_SEQ=" + std::to_string(rtp_hdr->sequence_number)
            + " RxPktStats:" + StatsRxPackets.ToString()
        );
    }
    else {
        LOG(LoggerVerbosity::INFO, "Fill packet with length=" + std::to_string(length)
            + " RTP_SEQ=" + std::to_string(rtp_hdr->sequence_number)
            + " FillPktStats:" + StatsFillPackets.ToString()
        );
    }

    fecTxBlock->AddPacket(rtp_hdr, headers, data, length, rtp_hdr->sequence_number);

    // Send packet to UDP Port
    if (udpClientData == nullptr) {
        LOG(LoggerVerbosity::ERR, "UDP Client for data is not initialized, cannot send packet");
        return 1;
    }

    StatsTxPackets.addValue(length);
    LOG(LoggerVerbosity::INFO, "Sending packet to UDP client for data: seq=" + std::to_string(rtp_hdr->sequence_number)
        + " length=" + std::to_string(length)
        + " headers=" + headers.ToString()
        + " TxPktStats:" + StatsTxPackets.ToString()
    );
    udpClientData->SendPacket(headers, data, length);

    return 0; // Return 0 for success
}

int SMPTE_FEC_Engine::ReceivePacket(PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length)
{
    std::size_t org_length = length;

    // Get outer UDP Header - should be headers[1]
    std::shared_ptr<PacketHeaderUDP> udpHdr = std::dynamic_pointer_cast<PacketHeaderUDP>(headers.headers[1]);
    if (udpHdr == nullptr) {
        LOG(LoggerVerbosity::ERR, "FEC:ReceivePacket: Could not cast UDP header: headers="
            + headers.ToString());
        return -1;
    }
    if (length != data.size()) {
        LOG(LoggerVerbosity::ERR, "FEC:ReceivePacket: Data lengthdoes not match data.size: length="
            + std::to_string(length)
            + " data_size=" + std::to_string(data.size())
        );
        return -2;
    }

    // Get Port Mode
    auto pmi = udpHdr->dstPort - striperConfig->stripeCfg.StartUdpDstPortNumber;
    auto snum = pmi >> 2;
    pmi &= 0x3;
    UdpStriperPortE pm;
    if (pmi == 0) {
        pm = UdpStriperPortE::STRIPER_PORT_DATA;
    } else if (pmi == 2) {
        pm = UdpStriperPortE::STRIPER_PORT_FEC_COL;
    } else if (pmi == 4) {
        pm = UdpStriperPortE::STRIPER_PORT_FEC_ROW;
    } else {
        LOG(LoggerVerbosity::ERR, "Could not match UDP DST port to its mode: dstPort="
            + std::to_string(udpHdr->dstPort)
            + " pmi=" + std::to_string(pmi)
        );
        return -3;
    }
    if (snum != stripe_num) {
        LOG(LoggerVerbosity::ERR, "Decoded UDP DST port does not equal stripe number!! Port="
            + std::to_string(udpHdr->dstPort)
            + " pmi=" + std::to_string(pmi)
            + " pm=" + std::string(magic_enum::enum_name(pm))
            + " computed_stripe_num=" + std::to_string(snum)
            + " stripe_num=" + std::to_string(stripe_num)
        );
        return -4;
    }

    // Extract RTP Header
    auto rtpHdr = std::make_shared<PacketHeaderRTP>(data);
    headers.AddHeader(rtpHdr, -1); // place RTP header on headers
    bool fill = static_cast<RTP_PayloadTypeE>(rtpHdr->payload_type) == RTP_PayloadTypeE::FILL_DATA;

    // Now remove RTP header from data
    length -= rtpHdr->Size();
    data.erase(data.begin(), data.begin() + rtpHdr->Size());
    RTP_PayloadTypeE pt = static_cast<RTP_PayloadTypeE>(rtpHdr->payload_type);

    switch (pt) {
    case RTP_PayloadTypeE::NO_FEC:
    case RTP_PayloadTypeE::MODE_A_FEC:
    case RTP_PayloadTypeE::MODE_B_FEC:
    case RTP_PayloadTypeE::FILL_DATA:
        // Received Data Packet
    {
        if (pm != UdpStriperPortE::STRIPER_PORT_DATA) {
            LOG(LoggerVerbosity::ERR, "FEC:ReceivePacket: Payload type("
                + std::string(magic_enum::enum_name(pt))
                + ") does not match port_mode(" + std::string(magic_enum::enum_name(pm)) + ")"
            );
            return -5;
        }
        if (fill) StatsFillPackets.addValue(org_length);
        else StatsRxPackets.addValue(org_length);

        auto blk_num = rtpHdr->sequence_number / block_size;
        auto [it, inserted] = fecRxBlocks.emplace(blk_num, std::move(std::make_unique<FEC_RX_Block>(this, blk_num)));
        if (it == fecRxBlocks.end() || it->second == nullptr) {
            LOG(LoggerVerbosity::ERR, "FEC:ReceivePacket: Failed to create RX Block: blk_num=" + std::to_string(blk_num));
            return -6;
        }
        auto rc = it->second->ReceivePacket(rtpHdr, headers, data, length);
        if (rc == 0) {
            // Valid packet
            if (!fill) {
                StatsTxPackets.addValue(length);
                // Send Packet on
                LOG(LoggerVerbosity::INFO, "FEC:ReceivePacket: Received valid Data Packet: " + headers.ToString());
                // B2 - need to add send code here
                if (!fill) {
                    // Not a FILL packet, so send to downstream device, data with no headers
                    SendDataPacket(data, length);
                }
            }
            else {
                StatsDropPackets.addValue(FecEngineDropCodesE::FILL_PACKET, length);
                LOG(LoggerVerbosity::INFO, "FEC:ReceivePacket: Dropping Fill packet: " + headers.ToString());
            }
        }
        else {
            LOG(LoggerVerbosity::ERR, "FEC:ReceivePacket: Payload type is "
                + std::string(magic_enum::enum_name(pt))
                + ", but port_mode not a FEC PORT!! port_mode="
                + std::to_string(pmi)
            );
            return -7;
        }

        }
        break;
    case RTP_PayloadTypeE::FEC_DATAGRAM:
        if (pm == UdpStriperPortE::STRIPER_PORT_FEC_COL || pm == UdpStriperPortE::STRIPER_PORT_FEC_ROW) {
            // FEC Volumn or Row, Extract FEC Header
            auto fecHdr = std::make_shared<PacketHeaderSMPTE>(data);
            headers.AddHeader(fecHdr, -1); // place RTP header on headers

            // Now remove RTP header from data
            length -= fecHdr->Size();
            data.erase(data.begin(), data.begin() + fecHdr->Size());

            auto blk_num = fecHdr->sequence_base / block_size;
            auto [it, inserted] = fecRxBlocks.emplace(blk_num, std::move(std::make_unique<FEC_RX_Block>(this, blk_num)));
            if (inserted) {
                LOG(LoggerVerbosity::WARNING, "FEC:ReceivePacket: FEC pkt: RX Block does not exist!!: blk_num=" + std::to_string(blk_num));
                
            } else if(it == fecRxBlocks.end() || it->second == nullptr) {
                LOG(LoggerVerbosity::ERR, "FEC:ReceivPacket: FEC pkt: Failed to create RX Block: blk_num=" + std::to_string(blk_num));
                return -8;
            }
            auto rc = it->second->ReceivedFecPacket(pm, rtpHdr, fecHdr, headers, data, length);
        } else {
            LOG(LoggerVerbosity::ERR, "FEC:ReceivePacket: FEC pkt: Payload type is FEC_DATAGRAM, but port_mode not a FEC PORT!! port_mode="
                + std::to_string(pmi)
            );
            return -9;
        }
        break;
    default:
        LOG(LoggerVerbosity::ERR, "FEC:ReceivePacket: Unhandled payload type="
            + std::to_string(rtpHdr->payload_type)
            + " " + headers.ToString()
        );
        return -10;
    }

    return 0; // Return 0 for success
}

void SMPTE_FEC_Engine::SendDataPacket(std::vector<uint8_t>& data, std::size_t length) {
    PacketHeaders pktHeaders;

    // Assume data has original IP header and UDP header
    // Extract IP Header
    auto ipHdr = std::make_shared<PacketHeaderIPv4>(data);

    // Now remove IP header from data
    length -= ipHdr->Size();
    data.erase(data.begin(), data.begin() + ipHdr->Size());
    pktHeaders.AddHeader(ipHdr);

    // Extract UDP Header
    auto udpHdr = std::make_shared<PacketHeaderUDP>(data);

    // Now remove UDP header from data
    length -= udpHdr->Size();
    data.erase(data.begin(), data.begin() + udpHdr->Size());
    pktHeaders.AddHeader(udpHdr, -1);

    owning_process->SendDestripePacket(pktHeaders, data, length);
}

// FEC_TX_BLOCK
FEC_TX_Block::FEC_TX_Block(SMPTE_FEC_Engine* FEC_Engine_)
    : FEC_Engine(FEC_Engine_)
{
    switch (FEC_Engine->striperConfig->stripeCfg.Mode) {
    case FEC_Mode::NONE:
        break;
    case FEC_Mode::OptionA:
    case FEC_Mode::OptionB:
        col_fec.resize(FEC_Engine->striperConfig->stripeCfg.Parm_L_cols);
        break;
    }
}

void FEC_TX_Block::AddPacket(const std::shared_ptr<PacketHeaderRTP> rtpHdr, const PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length, uint16_t seq_num) {
    auto cell_index = seq_num % FEC_Engine->block_size;
    auto row = cell_index / FEC_Engine->number_columns;
    auto column = cell_index % FEC_Engine->number_columns;

    if (cell_index == FEC_Engine->block_size - 1) {
        LOG(LoggerVerbosity::INFO, "Ending FEC block: seq=" 
            + std::to_string(rtpHdr->sequence_number));
        FEC_Engine->block_started.store(false);
    }

    switch (FEC_Engine->base_payload_type) {
    case RTP_PayloadTypeE::NO_FEC:
        break;
    case RTP_PayloadTypeE::MODE_A_FEC: // Column FEC
        col_fec[column].AddPacket(rtpHdr, data, length);
        if (row == FEC_Engine->number_rows - 1) {
            // Last row in block, start sending column FECs  
            LOG(LoggerVerbosity::INFO, "Sending column FEC: col="
                + std::to_string(column)
                + " seq_base=" + std::to_string(col_fec[column].fec_header->sequence_base)
            );
            col_fec[column].PrepForSend(RTP_PayloadTypeE::FEC_DATAGRAM, column, FEC_Engine->number_rows);
            FEC_Engine->StatsFecColPackets.addValue(col_fec[column].payload_length + col_fec[column].headers.length);
            FEC_Engine->udpClientColFEC->SendPacket(col_fec[column].headers, col_fec[column].payload, col_fec[column].payload_length);
            col_fec[column].Clear(); // clear after send
        }
        break;
    case RTP_PayloadTypeE::MODE_B_FEC:
        col_fec[column].AddPacket(rtpHdr, data, length);
        row_fec.AddPacket(rtpHdr, data, length);
        if (row == FEC_Engine->number_rows - 1) {
            // Last row in block, start sending column FECs             
            LOG(LoggerVerbosity::INFO, "Sending column FEC: col="
                + std::to_string(column)
                + " seq_base=" + std::to_string(col_fec[column].fec_header->sequence_base)
            );
            col_fec[column].PrepForSend(RTP_PayloadTypeE::FEC_DATAGRAM, column, FEC_Engine->number_rows);
            FEC_Engine->StatsFecColPackets.addValue(col_fec[column].payload_length + col_fec[column].headers.length);
            FEC_Engine->udpClientColFEC->SendPacket(col_fec[column].headers, col_fec[column].payload, col_fec[column].payload_length);
            col_fec[column].Clear(); // clear after send
        }
        if (column == FEC_Engine->number_columns - 1) {
            LOG(LoggerVerbosity::INFO, "Sending row FEC: row="
                + std::to_string(row)
                + " seq_base=" + std::to_string(row_fec.fec_header->sequence_base)
            );
            row_fec.PrepForSend(RTP_PayloadTypeE::FEC_DATAGRAM, row, FEC_Engine->number_columns);
            FEC_Engine->StatsFecColPackets.addValue(row_fec.payload_length + row_fec.headers.length);
            FEC_Engine->udpClientRowFEC->SendPacket(row_fec.headers, row_fec.payload, row_fec.payload_length);
            row_fec.Clear(); // clear after send
        }
        break;
    }
}

// FEC_RX_BLOCK
FEC_RX_Block::FEC_RX_Block(SMPTE_FEC_Engine* FEC_Engine_, uint32_t block_num_)
    : FEC_Engine(FEC_Engine_)
    , block_num(block_num_)
{
    startTime = std::chrono::system_clock::now();
    matrixOfDataPkts.clear();
    matrixOfDataPkts.resize(FEC_Engine->number_rows);
    row_fec.clear();
    col_fec.clear();
    switch (FEC_Engine->striperConfig->stripeCfg.Mode) {
    case FEC_Mode::NONE:
        rx_last_fec_col = true;
        rx_last_fec_row = true;
        break;
    case FEC_Mode::OptionA:
        rx_last_fec_row = true;
        break;
    }
}

int FEC_RX_Block::ReceivePacket(const std::shared_ptr<PacketHeaderRTP> rtpHdr, const PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length)
{
    auto cell_index = rtpHdr->sequence_number % FEC_Engine->block_size;
    auto row = cell_index / FEC_Engine->number_columns;
    auto column = cell_index % FEC_Engine->number_columns;
    LOG(LoggerVerbosity::INFO, "FEC_RX_BLOCK:ReceivedPacket:"
        " cell=" + std::to_string(cell_index)
        + " row=" + std::to_string(row)
        + " col=" + std::to_string(column)
        + " blk=" + std::to_string(block_num)
    );

    if (row >= matrixOfDataPkts.size()) {
        LOG(LoggerVerbosity::ERR, "FEC_RX_BLOCK: computed row(" + std::to_string(row)
            +") is larger than matrix("+ std::to_string(matrixOfDataPkts.size()) +")"
        );
        return 10;
    }
    if (column >= FEC_Engine->number_columns) {
        LOG(LoggerVerbosity::ERR, "FEC_RX_BLOCK: computed column(" + std::to_string(column)
            + ") is larger than configured columns(" + std::to_string(FEC_Engine->number_columns) + ")"
        );
        return 20;
    }
    auto [it, inserted] = matrixOfDataPkts[row].emplace(column, FEC_Datagram(rtpHdr, headers, data, length));
    if (inserted == false) {
        LOG(LoggerVerbosity::ERR, "FEC_RX_BLOCK: Trying to insert Data Packet into block matrix at location that is already occupied!! row="
            + std::to_string(row) + " col=" + std::to_string(column)
        );
        return 30;
    }
    if (cell_index == FEC_Engine->block_size - 1) {
        // received last data cell in block
        rx_last_cell = true;
        LOG(LoggerVerbosity::INFO, "FEC_RX_BLOCK: Received last Data cell in block:"
            " rx_last_fec_col=" + util::boolToString_ternary(rx_last_fec_col)
            + " rx_last_fec_row=" + util::boolToString_ternary(rx_last_fec_row)
        );
        if (rx_last_fec_col && rx_last_fec_row) {
            CheckBlock();
        }
    }

    return 0;
}

int FEC_RX_Block::ReceivedFecPacket(UdpStriperPortE port_mode, const std::shared_ptr<PacketHeaderRTP> rtpHdr, const std::shared_ptr<PacketHeaderSMPTE> fecHdr, const PacketHeaders& headers, const std::vector<uint8_t>& data, std::size_t length)
{
    auto cell_index = fecHdr->sequence_base % FEC_Engine->block_size;
    auto row = cell_index / FEC_Engine->number_columns;
    auto column = cell_index % FEC_Engine->number_columns;
    LOG(LoggerVerbosity::INFO, "FEC_RX_BLOCK:ReceivedFecPacket: port_mode="
        + std::string(magic_enum::enum_name(port_mode))
        + " cell=" + std::to_string(cell_index)
        + " row=" + std::to_string(row)
        + " col=" + std::to_string(column)
        + " blk=" + std::to_string(block_num)
    );
    switch (port_mode) {
    case UdpStriperPortE::STRIPER_PORT_FEC_ROW:
    {
        if (row >= FEC_Engine->number_rows) {
            LOG(LoggerVerbosity::ERR, "FEC_RX_BLOCK: computed FEC row(" + std::to_string(row)
                + ") is larger than configured number of rows (" + std::to_string(FEC_Engine->number_rows) + ")"
            );
            return 10;
        }
        auto [it, inserted] = row_fec.emplace(row, FEC_Packet(fecHdr, headers, data, length));
        if (inserted == false) {
            LOG(LoggerVerbosity::ERR, "FEC_RX_BLOCK: Trying to insert FEC Packet into ROW map at location that is already occupied!! row="
                + std::to_string(row) 
            );
            return 20;
        }
        if (row == FEC_Engine->number_rows - 1) {
            // received last data cell in block
            rx_last_fec_row = true;
            if (rx_last_cell && rx_last_fec_col) {
                CheckBlock();
            }
        }
    }
        break;
    case UdpStriperPortE::STRIPER_PORT_FEC_COL:
    {
        if (column >= FEC_Engine->number_columns) {
            LOG(LoggerVerbosity::ERR, "FEC_RX_BLOCK: computed FEC col(" + std::to_string(column)
                + ") is larger than configured number of columns (" + std::to_string(FEC_Engine->number_columns) + ")"
            );
            return 30;
        }
        auto [it, inserted] = col_fec.emplace(column, FEC_Packet(fecHdr, headers, data, length));
        if (inserted == false) {
            LOG(LoggerVerbosity::ERR, "FEC_RX_BLOCK: Trying to insert FEC Packet into COLUMN map at location that is already occupied!! col=" + std::to_string(column)
            );
            return 40;
        }
        if (column == FEC_Engine->number_columns - 1) {
            // received last data cell in block
            rx_last_fec_col = true;
            if (rx_last_cell && rx_last_fec_row) {
                CheckBlock();
            }
        }
    }
    break;
    default:
        LOG(LoggerVerbosity::ERR, "FEC_RX_BLOCK: FEC packets has incorrect port mode=" + std::string(magic_enum::enum_name(port_mode)));
        return 50;
    }
    return 0;
}

void FEC_RX_Block::CheckBlock() {
    LOG(LoggerVerbosity::INFO, "FEC_RX_BLOCK: CheckBlock: blk=" + std::to_string(block_num));
}

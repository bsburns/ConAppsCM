/*------------------------------------------------------------------
 * Simulator Device Output Source File
 *
 * Defines functions and classes for Output device
 * Aggregates all packets from all stripes into a single pipe 
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "DevOutput.h"


DeviceOutputBase::DeviceOutputBase(
    const std::string& name, 
    const OutputConfig& config, 
    StatisticsBase* prtStats)
    : DeviceBase(name, DeviceType::STRIPER),
    cfg(config),
    StatsRxPackets(name+".RX", prtStats)
{
    std::string outFile = cfg.OutputFile;
    if (outFile.empty()) {
        outFile = TestName + "_outpkt.csv";
    }
    LOG(LoggerVerbosity::INFO, std::format("{}: Opening Output File name:", GetDeviceName(), outFile));
    if (log_file.is_open()) log_file.close();
    log_file.open(outFile, std::ios::out | std::ios::trunc);

    // Create Header
    log_file << "time_ns, Pkt_ID, TG_ID, STRIPE_ID, FEC_SEQ, FEC_ROW, FEC_COL, FEC_CELL, FILL_CELL\n";
}

void DeviceOutputBase::ExecuteEvent(std::shared_ptr<EventBase> event) {
    LOG(LoggerVerbosity::INFO, std::format("{}: Exec: rx evt=[{}]", GetDeviceName(), event->ToStringBase()));
    if (event->GetEventType() == EventType::PACKET) {
        if (std::shared_ptr<EventPacket> pktEvt = std::dynamic_pointer_cast<EventPacket>(event)) {
            StatsRxPackets.addValue(pktEvt->packet_length);
            log_file << event->timestamp_ns << ", "
                << pktEvt->GetPacketId() << ", "
                << pktEvt->GetTrafGenId() << ", "
                << event->GetFromDeviceID() << ", " // Stripe ID
                << pktEvt->FEC_SequenceNum << ", "
                << pktEvt->FEC_row  << ", "
                << pktEvt->FEC_col  << ", "
                << pktEvt->FEC_cell  << ", "
                << pktEvt->FillCell
                << "\n";
        } else {
            LOG(LoggerVerbosity::ERROR, std::format("{}: Unable to cast event to EventPacket: evt={}", GetDeviceName(), event->ToStringBase()));
        }
    }
}


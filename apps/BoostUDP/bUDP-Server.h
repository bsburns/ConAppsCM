#pragma once
/*------------------------------------------------------------------
 * bUDP.h
 *
 * Boost based UDP Server header
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */


#include <stdexcept>
#include <climits>
#include <set>
#include <map>
#include <filesystem>
#include <utility>

#include <boost/asio.hpp>

#include "logger.h"
#include "watchdog.h"
#include "threadManager.h"
#include "bStripes.h"
#include "PacketHeader/PacketHeader.h"
#include "bUDP.h"


class UdpServer {
public:
    // Bind to the given port on all available network interfaces
    UdpServer(boost::asio::io_context& io_context, short port_, UdpStriperPortE port_mode_, std::string out_dir_, std::shared_ptr<StripesManager> stripes_mgr_, StriperModeE striper_mode_)
        : socket_(io_context, udp::endpoint(udp::v4(), port_))
        , port(port_)
        , port_mode(port_mode_)
        , stripesMgr(stripes_mgr_)
        , StriperMode(striper_mode_)
        , out_dir(out_dir_)
    {
        start_receive();
    }

    void StopServer() {
        boost::system::error_code ec;
        socket_.cancel(ec);
    }
private:
    std::shared_ptr<StripesManager> stripesMgr;
    StriperModeE StriperMode;
    short port;
	UdpStriperPortE port_mode;
    std::string out_dir; // Directory where receved files will be saved
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::vector<uint8_t> recv_buffer_{ std::vector<uint8_t>(4096, 0) };
    std::set<udp::endpoint> known_clients_; // Active client registry
    std::map<udp::endpoint, udp_connection> KnownClientConnections; // Map of client endpoints to their connection info

    void start_receive() {
        // Wait asynchronously for an incoming packet
        socket_.async_receive_from(
            boost::asio::buffer(recv_buffer_), remote_endpoint_,
            [this](boost::system::error_code ec, std::size_t bytes_transferred) {
                if (ec == boost::asio::error::operation_aborted) {
                    // Graceful exit: operation was canceled, cleanup and return
                    return;
                }

                if (ec) {
                    // Handle other actual network errors
                    return;
                }

                if (bytes_transferred > 0) {
                    process_packet(bytes_transferred);
                }

                // Immediately resume listening for other clients
                start_receive();
            });
    }
    
    void decodeRemoteString(std::string remote_str, uint32_t& srcIP, uint16_t& srcPort) {
        auto pos = remote_str.find(":", 0);
        if (pos != std::string::npos) {
            srcIP = inet_addr(remote_str.substr(0, pos).c_str());
            try {
                unsigned long parsed = std::stoul(remote_str.substr(pos + 1));

                if (parsed > UINT16_MAX) {
                    throw std::out_of_range("Value exceeds 16-bit unsigned range");
                }

                srcPort = static_cast<uint16_t>(parsed);
            }
            catch (const std::invalid_argument& e) {
                std::cout << "Error: Not a valid number.\n";
                srcPort = 0xFFFF;
            }
            catch (const std::out_of_range& e) {
                std::cout << "Error: Out of range.\n";
                srcPort = 0xFFFE;
            }
        }
    }

    void process_packet(std::size_t length) {
        Watchdog& watchdog = Watchdog::GetInstance();
        watchdog.CheckIn(); // Reset watchdog timer on packet receipt
        std::ostringstream oss;
        oss << remote_endpoint_;
        std::string remote_str = oss.str();
        std::string remote_str_log = remote_str + ":" + std::to_string(port);
        std::string message = "";

        // Try to insert a new connection if not present
        auto [it, inserted] = KnownClientConnections.emplace(remote_endpoint_, udp_connection{});
        if (inserted) {
            KnownClientConnections[remote_endpoint_].connection_time = std::chrono::system_clock::now();
            message = std::string(reinterpret_cast<const char*>(recv_buffer_.data()), length);
            LOG(LoggerVerbosity::INFO, "New Client Connected " + remote_str_log);
            uint16_t srcPort = 0;
            uint32_t srcIP = 0;
            decodeRemoteString(remote_str, srcIP, srcPort);
            KnownClientConnections[remote_endpoint_].srcIP = srcIP;
            KnownClientConnections[remote_endpoint_].srcPort = srcPort;

            auto result = message.compare(0, 10, "FILE_MODE:");
            if (result == 0) {
                KnownClientConnections[remote_endpoint_].mode = UdpSendMode::SEND_FILE;
                KnownClientConnections[remote_endpoint_].file_chunks.clear();
                fs::path filePath(message.substr(10)); // Extract file name after "FILE_MODE:"

                KnownClientConnections[remote_endpoint_].file_name = filePath.filename().string();
                LOG(LoggerVerbosity::INFO, remote_str + ": file_name=" + KnownClientConnections[remote_endpoint_].file_name);
                if (StriperMode != StriperModeE::TRANSMITTER) {
                    return; // Don't process the file mode message further
                }
            }
            else {
                result = message.compare(0, 12, "PACKET_MODE:");
                if (result == 0) {
                    KnownClientConnections[remote_endpoint_].mode = UdpSendMode::PACKET;
                } else {
                    KnownClientConnections[remote_endpoint_].mode = UdpSendMode::MESSAGE;
                }
            }
            LOG(LoggerVerbosity::INFO, remote_str_log 
                + " - mode = " +
                std::string(magic_enum::enum_name(KnownClientConnections[remote_endpoint_].mode)));
        } else {
            // Known Connection

        }

        if (KnownClientConnections[remote_endpoint_].mode == UdpSendMode::MESSAGE) {
            message = std::string(reinterpret_cast<const char*> (recv_buffer_.data()), length);
            LOG(LoggerVerbosity::INFO, remote_str_log +
                ":MM: Received Message: " + message);

            // Echo the packet back asynchronously to the sender
            auto response_msg = std::make_shared<std::string>("Echo: " + message);
            socket_.async_send_to(
                boost::asio::buffer(*response_msg), remote_endpoint_,
                [response_msg](boost::system::error_code /*ec*/, std::size_t /*bytes*/) {
                    // Shared pointer capture keeps data alive until transmission completes
                });
        } else if (KnownClientConnections[remote_endpoint_].mode == UdpSendMode::SEND_FILE) {
            // FILE MODE: Expecting file chunks in the format "CHUNK:<chunk_number>:<data>"
            FileTransferHeader fh = FileTransferHeader::deserialize(recv_buffer_);
            LOG(LoggerVerbosity::INFO, remote_str_log +
                ":FM: Received " + std::to_string(length) + " bytes" +
                " chunkNumber=" + std::to_string(fh.chunkNumber) +
                " chunkSize=" + std::to_string(fh.chunkSize)
            );
            if (StriperMode == StriperModeE::TRANSMITTER) {
                LOG(LoggerVerbosity::INFO, "Packetizing received data to send to striper");
                // convert to packet format
                // Create IPv4 and UDP headers for the packet
                PacketHeaders pktHdr;
                auto ipHdr = std::make_shared<PacketHeaderIPv4>();
                ipHdr->Version = 4;
                ipHdr->IHL = 5; // 5 * 4 = 20 bytes
                ipHdr->TOS = 0;
                ipHdr->totalLength = ipHdr->Size() + PacketHeaderUDP().Size() + length;
                ipHdr->TTL = 64;
                ipHdr->Protocol = static_cast<uint8_t>(PacketHeaderType::UDP); // UDP
                ipHdr->srcIP = KnownClientConnections[remote_endpoint_].srcIP;
                ipHdr->dstIP = 0; // Destination IP can be set to 0 for now, or you can set it to a specific value if needed
                pktHdr.AddHeader(ipHdr);

                auto udpHdr = std::make_shared<PacketHeaderUDP>();
                udpHdr->srcPort = KnownClientConnections[remote_endpoint_].srcPort;
                udpHdr->dstPort = port;
                udpHdr->length = udpHdr->Size() + length;
                pktHdr.AddHeader(udpHdr);
                LOG(LoggerVerbosity::INFO, remote_str_log + 
                    " - FM::Striper::Transmitter - Add UDP header: " + udpHdr->to_string());
                stripesMgr->SendPacket(pktHdr, recv_buffer_, length);
            } else if (StriperMode == StriperModeE::RECEIVER) {
				// convert to packet format = Expect recv_buffer_ to start with RTP Header, so we need to add the original IP and UDP headers to the packet
                PacketHeaders pktHdr;

                // Create IPv4 and UDP headers for the packet
                auto ipHdr = std::make_shared<PacketHeaderIPv4>();
                ipHdr->Version = 4;
                ipHdr->IHL = 5; // 5 * 4 = 20 bytes
                ipHdr->TOS = 0;
                ipHdr->totalLength = ipHdr->Size() + PacketHeaderUDP().Size() + length;
                ipHdr->TTL = 64;
                ipHdr->Protocol = static_cast<uint8_t>(PacketHeaderType::UDP); // UDP
                ipHdr->srcIP = KnownClientConnections[remote_endpoint_].srcIP;
                ipHdr->dstIP = 0; // Destination IP can be set to 0 for now, or you can set it to a specific value if needed
                pktHdr.AddHeader(ipHdr);

                auto udpHdr = std::make_shared<PacketHeaderUDP>();
                udpHdr->srcPort = KnownClientConnections[remote_endpoint_].srcPort;
                udpHdr->dstPort = port;
                udpHdr->length = udpHdr->Size() + length;
                pktHdr.AddHeader(udpHdr, -1);
                LOG(LoggerVerbosity::INFO, remote_str_log
                    + " - FM::Striper::Receiver - Add UDP header: " + udpHdr->to_string());

				// Get RTP Header from recv_buffer_ and add it to pktHdr
                auto rtpHdr = std::make_shared<PacketHeaderRTP>();
                rtpHdr->deserialize(recv_buffer_);
                pktHdr.AddHeader(rtpHdr, -1);

                stripesMgr->ReceivePacket(port_mode, pktHdr, recv_buffer_, length);
            } else { // Non Transmitter/Receiver Mode, so save Save file
                if (fh.chunkSize > 0) {
                    auto [cit, inserted] =
                        KnownClientConnections[remote_endpoint_].file_chunks.emplace(
                            fh.chunkNumber,
                            std::vector<uint8_t>(recv_buffer_.begin() + fh.GetHeaderSizeBytes(), recv_buffer_.begin() + length)
                        );
                }
                else {
                    // End of file
                    LOG(LoggerVerbosity::INFO, remote_str_log 
                        + " - FM: Received EOF");

                    // Write the received file chunks to disk

                    std::string output_file = out_dir + "/" + KnownClientConnections[remote_endpoint_].file_name;
                    std::ofstream ofs(output_file, std::ios::binary);
                    if (!ofs) {
                        LOG(LoggerVerbosity::CRITICAL, remote_str_log 
                            + " - FM: Error opening output file: " + output_file);
                        return;
                    }
                    for (const auto& [chunkNum, chunkData] : KnownClientConnections[remote_endpoint_].file_chunks) {
                        ofs.write(reinterpret_cast<const char*>(chunkData.data()), chunkData.size());
                    }
                    ofs.close();
                    LOG(LoggerVerbosity::INFO, remote_str_log 
                        + " - FM: File saved successfully to " + output_file);
                }
            }
        } else if (KnownClientConnections[remote_endpoint_].mode == UdpSendMode::PACKET) {
            LOG(LoggerVerbosity::INFO, remote_str_log 
                + " - PM: Received Packet:"
                + " striper_mode=" + std::string(magic_enum::enum_name(StriperMode))
                + " length=" + std::to_string(length));
            if (StriperMode == StriperModeE::RECEIVER) {
                // convert to packet format
                // Create IPv4 and UDP headers for the packet
                PacketHeaders pktHdr;
                auto ipHdr = std::make_shared<PacketHeaderIPv4>();
                ipHdr->Version = 4;
                ipHdr->IHL = 5; // 5 * 4 = 20 bytes
                ipHdr->TOS = 0;
                ipHdr->totalLength = ipHdr->Size() + PacketHeaderUDP().Size() + length;
                ipHdr->TTL = 64;
                ipHdr->Protocol = static_cast<uint8_t>(PacketHeaderType::UDP); // UDP
                ipHdr->srcIP = KnownClientConnections[remote_endpoint_].srcIP;
                ipHdr->dstIP = 0; // Destination IP can be set to 0 for now, or you can set it to a specific value if needed
                pktHdr.AddHeader(ipHdr);

                auto udpHdr = std::make_shared<PacketHeaderUDP>();
                udpHdr->srcPort = KnownClientConnections[remote_endpoint_].srcPort;
                udpHdr->dstPort = port;
                udpHdr->length = udpHdr->Size() + length;
                pktHdr.AddHeader(udpHdr, -1);
                LOG(LoggerVerbosity::INFO, remote_str_log +
                    " - PM::Striper::Receiver - Add UDP header: " + udpHdr->to_string());
                stripesMgr->ReceivePacket(port_mode, pktHdr, recv_buffer_, length);
            }
        } else {
            std::cout << "Unhandled Mode: "
                << std::string(magic_enum::enum_name(KnownClientConnections[remote_endpoint_].mode))
                << " for endpoint " << remote_str_log << std::endl;
        }
    }
};

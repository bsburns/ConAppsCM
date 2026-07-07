#pragma once
/*------------------------------------------------------------------
 * bUDP-Client.h
 *
 * Boost based UDP Client header
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#define VERSION "0.1"

#include <stdexcept>
#include <climits>
#include <set>
#include <map>
#include <filesystem>
#include <utility>

#include <boost/asio.hpp>

#include "bUDP.h"
#include "logger.h"
#include "watchdog.h"
#include "threadManager.h"
#include "PacketHeader/PacketHeader.h"

using boost::asio::ip::udp;
using namespace my_logger;
namespace fs = std::filesystem;


class UdpClient {
private:
    std::string sport;
    std::string dport;
    UdpSendMode sendMode;
    std::string serverIP;
	udp::socket socket;
	udp::resolver resolver;
    udp::endpoint receiver_endpoint;

public:
    UdpClient(boost::asio::io_context& io_context, std::string serverp_ip_, std::string sport_, std::string dport_, UdpSendMode mode_)
        : resolver(io_context)
		, receiver_endpoint(*resolver.resolve(udp::v4(), serverIP, dport).begin())
        , socket(io_context, udp::v4())
        , serverIP(serverp_ip_)
        , sport(sport_)
        , dport(dport_)
        , sendMode(mode_)
    {
        //boost::asio::io_context io_context;
        // 2. Resolve the remote hostname or IP address and port
        LOG(LoggerVerbosity::INFO, "Connecting to " + serverIP + ":" + sport + "::" + dport);

        // 3. Open the UDP socket
        int sourcePort;
        try {
            sourcePort = std::stoi(sport);
        }
        catch (std::exception& e) {
            std::cerr << "\nException: UdpClient: convert source Port: " << e.what() << std::endl;
            return;
        }

        try {
            //udp::endpoint local_ep(udp::v4(), sourcePort);
            socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), sourcePort));
        }
        catch (std::exception& e) {
            std::cerr << "\nException: UdpClient: bind local endpoint: " << e.what() << std::endl;
            return;
        }

        try
        {
            receiver_endpoint = *resolver.resolve(udp::v4(), serverIP, dport).begin();
            //socket.open(udp::v4());
        }
        catch (const std::exception& e)
        {
            std::cerr << "\nException: UdpClient: Open socket with endpoint: " << e.what() << std::endl;
            return;
        }
    }

    int SendFile(std::string filename) {
        if (!fs::exists(filename)) {
            LOG(LoggerVerbosity::ERR, "Send File does not exist!! file=" + filename);
            return 1;
        }

        LOG(LoggerVerbosity::INFO, "Sending file: " + filename);
        std::string message = "FILE_MODE: " + filename;
        socket.send_to(boost::asio::buffer(message), receiver_endpoint);

        FileTransferHeader fh;
        auto FTHS = fh.GetHeaderSizeBytes();

        std::vector<char> buffer(CHUNK_SIZE + FTHS);
        size_t total_bytes_sent = 0;

        // Open file in binary mode
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            LOG(LoggerVerbosity::ERR, "Cannot open file " + filename);
            return 2;
        }
        while (file) {
            file.read(buffer.data() + FTHS, CHUNK_SIZE - FTHS);
            std::streamsize bytes_read = file.gcount();
            if (bytes_read > 0) {
                fh.chunkSize = static_cast<uint16_t>(bytes_read);
                std::vector<uint8_t> headerData = fh.serialize();
                std::copy(headerData.begin(), headerData.end(), buffer.begin());
                size_t bytes_sent = socket.send_to(boost::asio::buffer(buffer.data(), bytes_read + FTHS), receiver_endpoint);
                total_bytes_sent += bytes_sent;
                LOG(LoggerVerbosity::INFO, "Send file: bytes_sent=" + std::to_string(bytes_sent) +
                    " total_bytes_sent=" + std::to_string(total_bytes_sent) +
                    " bytes_read=" + std::to_string(bytes_read));
                fh.chunkNumber++;
            }
        }
        // Send an empty packet to indicate EOF
        fh.chunkSize = 0;
        socket.send_to(boost::asio::buffer(fh.serialize(), FTHS), receiver_endpoint);

        LOG(LoggerVerbosity::INFO, "File sent successfully. Total bytes: " +
            std::to_string(total_bytes_sent));
        return 0;
    }

    int StartPacketMode() {
        std::string message = "PACKET_MODE:";
        if (socket.is_open()) {
            LOG(LoggerVerbosity::INFO, "Starting Packet Mode: " + message);
        }
        else {
            LOG(LoggerVerbosity::ERR, "Socket is not open. Cannot start Packet Mode.");
            return 1;
        }
        socket.send_to(boost::asio::buffer(message), receiver_endpoint);
        return 0;
    }
    
    int SendPacket(PacketHeaders& headers, std::vector<uint8_t>& data, std::size_t length) {
        LOG(LoggerVerbosity::INFO, "Sending Packet: len=" + std::to_string(length)
            + " hdr_length=" + std::to_string(headers.length));
		auto rc = headers.MakePacket(data, length);
        if (rc != 0) {
            LOG(LoggerVerbosity::ERR, "Failed to make packet from headers. rc=" + std::to_string(rc));
            return rc;
		}
        LOG(LoggerVerbosity::INFO, "Sending Packet2: data.size=" + std::to_string(data.size()));
        
        boost::system::error_code ec;
        int option = 0;
        socklen_t option_len = sizeof(option);
#ifdef _WIN32
        // On Windows, use SOL_SOCKET and SO_ERROR with getsockopt
        ::getsockopt(socket.native_handle(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&option), &option_len);
#else
        ::getsockopt(socket.native_handle(), SOL_SOCKET, SO_ERROR, &option, &option_len);
#endif

        if (!ec && option == 0) {
            // No socket errors detected by the OS layer
        }
        else {
            // Socket has a pending error or failed to query
			LOG(LoggerVerbosity::ERR, "Socket error detected: " + ec.message() + " option="+std::to_string(option));
            return -2;
        }
        if (socket.is_open()) {
            LOG(LoggerVerbosity::INFO, "Socket is open. Ready to send packet.");
        }
        else {
            LOG(LoggerVerbosity::ERR, "Socket is not open. Cannot send packet.");
            return -1;
        }
        try {
            size_t bytes_sent = socket.send_to(boost::asio::buffer(data.data(), length), receiver_endpoint);
            LOG(LoggerVerbosity::INFO, "Sent packet: bytes_sent=" + std::to_string(bytes_sent) +
				" total_length=" + std::to_string(length));
        } catch (std::exception& e) {
            LOG(LoggerVerbosity::ERR, "Exception in SendPacket: " + std::string(e.what()));
            return -100;
		}

        return 0;
    }

    void SendMessage(std::string msg) {
        socket.send_to(boost::asio::buffer(msg), receiver_endpoint);
        std::cout << "\nSent message: " << msg;

        // Prepare a buffer to receive the reply back
        std::array<char, 1024> recv_buf;
        udp::endpoint sender_endpoint;

        // This blocks until data arrives
        size_t len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);

        std::cout << "\nReceived reply: ";
        std::cout.write(recv_buf.data(), len);
        std::cout << std::endl;
	}
};


#pragma once
/*------------------------------------------------------------------
 * Multicast UDP Server
 *
 * Boost based Multicast UDP Server header
 *
 * July 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#define VERSION "0.1"

#include <stdexcept>
#include <map>
#include <vector>
#include <chrono>

#include <boost/asio.hpp>
#include "logger.h"


using boost::asio::ip::udp;
using namespace my_logger;
namespace fs = std::filesystem;

class MulticastUdpServerConnection {
public:
    std::string name;
    uint16_t srcPort = 0;
    uint32_t srcIP = 0;
    std::chrono::system_clock::time_point connection_time; // Track when the connection was established
    uint32_t expected_sequence = 0;

    // Statistics
    StatisticsRTM<uint64_t> StatsRxPackets;

    MulticastUdpServerConnection() {}
    MulticastUdpServerConnection(std::string conn_name)
        : name(conn_name)
        , StatsRxPackets(conn_name + ":RxPkts", nullptr)
    {
    }

    std::string BriefStats() const {
        std::string str = name;
        str += ": RXP=" + std::to_string(StatsRxPackets.count());
        str += ", RXB=" + std::to_string(StatsRxPackets.sum());

        return str;
    }
};


class MulticastUdpServer {
public:
    const MenuItem cli_menu =
    {
    .name = "show",
    .description = "show Multicast UDP Server status commands",
    .subMenus = {
        {
            .name = "connections",
            .description = "Show open connections",
            .subMenus = {},
            .valType = MenuItemValueTypes::NONE,
            .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {
                std::cout << "\nConnections:";
                for (const auto& [key, conn] : KnownClientConnections) {
                    std::cout << "\n\t" << conn.BriefStats();
                }
            },
        },
    },

    .valType = MenuItemValueTypes::SUBMENU,
    .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {
        std::cout << "\nAll connections: ";
        for (const auto& [key, conn] : KnownClientConnections) {
            std::cout << "\n\t" << conn.name;
        }
        },
    };

    // Bind to the given port on all available network interfaces
    MulticastUdpServer(boost::asio::io_context& io_context_,
        const std::string& listen_address_,
        const std::string& multicast_address_,
        unsigned short port_,
        std::function<void(std::vector<uint8_t>&, std::size_t)> recv_message_callback_)
        : socket_(io_context_)
        , listen_address(listen_address_)
        , multicast_address(multicast_address_)
        , port(port_)
        , recv_message_callback(recv_message_callback_)
    {
        // 1. Create endpoints
        boost::system::error_code ec;
        boost::asio::ip::address listen_addr = boost::asio::ip::make_address(listen_address, ec);
        if (ec) {
            throw std::runtime_error("Invalid listen address: " + listen_address + " (" + ec.message() + ")");
        }
        boost::asio::ip::udp::endpoint listen_endpoint(listen_addr, port);

        // 2. Open the socket matching the protocol (IPv4 or IPv6)
        socket_.open(listen_endpoint.protocol());

        // 3. Allow multiple applications to bind to this same address/port
        socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));

        // 4. Bind to the local endpoint
        socket_.bind(listen_endpoint);

        // 5. Join the multicast group
        boost::asio::ip::address mcast_addr = boost::asio::ip::make_address(multicast_address, ec);
        if (ec) {
            throw std::runtime_error("Invalid multicast address: " + multicast_address + " (" + ec.message() + ")");
        }
        socket_.set_option(boost::asio::ip::multicast::join_group(mcast_addr));

        // Start receiving data asynchronously
        start_receive();
    }

    void StopServer() {
        boost::system::error_code ec;
        socket_.cancel(ec);
    }
private:
    std::string listen_address;
    std::string multicast_address;
    short port;
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::vector<uint8_t> recv_buffer_{ std::vector<uint8_t>(4096, 0) };
    std::map<udp::endpoint, MulticastUdpServerConnection> KnownClientConnections; // Map of client endpoints to their connection info
    std::function<void(std::vector<uint8_t>&, std::size_t)> recv_message_callback;

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
        auto it = KnownClientConnections.find(remote_endpoint_);
        if (it == KnownClientConnections.end()) {
            KnownClientConnections[remote_endpoint_] = MulticastUdpServerConnection(remote_str_log);
            KnownClientConnections[remote_endpoint_].connection_time = std::chrono::system_clock::now();
            message = std::string(reinterpret_cast<const char*>(recv_buffer_.data()), length);
            LOG(LoggerVerbosity::INFO, "New Client Connected " + remote_str_log);
            uint16_t srcPort = 0;
            uint32_t srcIP = 0;
            decodeRemoteString(remote_str, srcIP, srcPort);
            KnownClientConnections[remote_endpoint_].srcIP = srcIP;
            KnownClientConnections[remote_endpoint_].srcPort = srcPort;
        }
        else {
            // Known Connection

        }
        KnownClientConnections[remote_endpoint_].StatsRxPackets.addValue(length);
        recv_message_callback(recv_buffer_, length);
    }
};

#pragma once
/*------------------------------------------------------------------
 * bUDP.h
 *
 * Boost based UDP common header
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#define VERSION "0.1"

#include <set>
#include <map>
#include <filesystem>

#include <boost/asio.hpp>

#include "logger.h"
#include "watchdog.h"

using boost::asio::ip::udp;
using namespace my_logger;
namespace fs = std::filesystem;

static const std::size_t CHUNK_SIZE = 1024; // safe UDP payload

enum class UdpSendMode : int {
    NOTSET = 0,
    MESSAGE = 1,
    SEND_FILE = 2
};

class FileTransferHeader {
public:
    uint16_t chunkNumber;
    uint16_t chunkSize;
	FileTransferHeader() : chunkNumber(0), chunkSize(0) {}
    int GetHeaderSizeBytes() const {
        return 4; // 2 bytes for chunkNumber and 2 bytes for chunkSize
	}
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> data(4); // 4 bytes for the header
        data[0] = chunkNumber >> 8;
        data[1] = chunkNumber & 0xFF;
        data[2] = chunkSize >> 8;
        data[3] = chunkSize & 0xFF;
        return data;
    }
    static FileTransferHeader deserialize(const std::vector<uint8_t>& data) {
        if (data.size() < 4) {
            throw std::invalid_argument("Data too short for FileTransferHeader");
        }
        FileTransferHeader header;
        header.chunkNumber = (data[0] << 8) | data[1];
        header.chunkSize = (data[2] << 8) | data[3];
        return header;
	}
};


class udp_connection {
public:
    UdpSendMode mode = UdpSendMode::NOTSET;
    std::map<uint16_t, std::vector<uint8_t>> file_chunks; // For storing file chunks if in SEND_FILE mode
    std::string file_name; // Store the file name being sent by the client
    std::chrono::system_clock::time_point connection_time; // Track when the connection was established

    udp_connection() {}
};

class UdpClient {
private:
    std::string sport;
    std::string dport;
    UdpSendMode sendMode;
    std::string serverIP;

public:
    UdpClient(std::string serverp_ip_, std::string sport_, std::string dport_, UdpSendMode mode_)
        : serverIP(serverp_ip_)
        , sport(sport_)
        , dport(dport_)
        , sendMode(mode_)
    {
    }
    
    int SendFile(std::string filename) {
        if (!fs::exists(filename)) {
            LOG(LoggerVerbosity::ERR, "Send File does not exist!! file=" + filename);
            return 1;
        }
        boost::asio::io_context io_context;
        udp::endpoint receiver_endpoint;

        // 2. Resolve the remote hostname or IP address and port
        LOG(LoggerVerbosity::INFO, "Connecting to " + serverIP + ":" + sport + "::" + dport);
        udp::resolver resolver(io_context);
        receiver_endpoint = *resolver.resolve(udp::v4(), serverIP, dport).begin();

        // 3. Open the UDP socket
        int sourcePort;
        try {
            sourcePort = std::stoi(sport);
        } catch (std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
            return 10;
        }
        udp::socket socket(io_context, udp::endpoint(udp::v4(), sourcePort));
        //socket.open(udp::v4());
        LOG(LoggerVerbosity::INFO, "Socket opened to "
            + serverIP + ":" + sport + "::" + dport
        );

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
};

class UdpServer {
public:
    // Bind to the given port on all available network interfaces
    UdpServer(boost::asio::io_context& io_context, short port, std::string out_dir_)
        : socket_(io_context, udp::endpoint(udp::v4(), port)) 
        , out_dir(out_dir_)
    {
        start_receive();
    }

    void StopServer() {
        boost::system::error_code ec;
        socket_.cancel(ec);
    }
private:
    std::string out_dir; // Directory where receved files will be saved
    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 4096> recv_buffer_;
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

    void process_packet(std::size_t length) {
        Watchdog& watchdog = Watchdog::GetInstance();
        watchdog.CheckIn(); // Reset watchdog timer on packet receipt
        std::string message(recv_buffer_.data(), length);
        std::ostringstream oss;
        oss << remote_endpoint_;
        std::string remote_str = oss.str();

        // Try to insert a new connection if not present
        auto [it, inserted] = KnownClientConnections.emplace(remote_endpoint_, udp_connection{});
        if (inserted) {
            LOG(LoggerVerbosity::INFO, "New Client Connected " + remote_str);
            auto result = message.compare(0, 10, "FILE_MODE:");
            KnownClientConnections[remote_endpoint_].connection_time = std::chrono::system_clock::now();
            if (result == 0) {
                KnownClientConnections[remote_endpoint_].mode = UdpSendMode::SEND_FILE;
                KnownClientConnections[remote_endpoint_].file_chunks.clear();
                fs::path filePath(message.substr(10)); // Extract file name after "FILE_MODE:"

                KnownClientConnections[remote_endpoint_].file_name = filePath.filename().string();
                LOG(LoggerVerbosity::INFO, remote_str + ": file_name=" + KnownClientConnections[remote_endpoint_].file_name);
                return; // Don't process the file mode message further
            }
            else {
                KnownClientConnections[remote_endpoint_].mode = UdpSendMode::MESSAGE;
            }
            LOG(LoggerVerbosity::INFO, remote_str + ": mode = " +
                std::string(magic_enum::enum_name(KnownClientConnections[remote_endpoint_].mode)));
        }

        if (KnownClientConnections[remote_endpoint_].mode == UdpSendMode::MESSAGE) {
            LOG(LoggerVerbosity::INFO, remote_str +
                ":MM: Received Message: " + message);

            // Echo the packet back asynchronously to the sender
            auto response_msg = std::make_shared<std::string>("Echo: " + message);
            socket_.async_send_to(
                boost::asio::buffer(*response_msg), remote_endpoint_,
                [response_msg](boost::system::error_code /*ec*/, std::size_t /*bytes*/) {
                    // Shared pointer capture keeps data alive until transmission completes
                });
        }
        else if (KnownClientConnections[remote_endpoint_].mode == UdpSendMode::SEND_FILE) {
            // FILE MODE: Expecting file chunks in the format "CHUNK:<chunk_number>:<data>"
            std::vector<uint8_t> data(recv_buffer_.begin(), recv_buffer_.begin() + length);
            FileTransferHeader fh = FileTransferHeader::deserialize(data);
            LOG(LoggerVerbosity::INFO, remote_str +
                ":FM: Received " + std::to_string(length) + " bytes" +
                " chunkNumber=" + std::to_string(fh.chunkNumber) +
                " chunkSize=" + std::to_string(fh.chunkSize)
            );
            if (fh.chunkSize > 0) {
                auto [cit, inserted] =
                    KnownClientConnections[remote_endpoint_].file_chunks.emplace(
                        fh.chunkNumber,
                        std::vector<uint8_t>(data.begin() + fh.GetHeaderSizeBytes(), data.end())
                    );
            }
            else {
                // End of file
                LOG(LoggerVerbosity::INFO, remote_str +
                    ":FM: Received EOF");

                // Write the received file chunks to disk

                std::string output_file = out_dir + "/" + KnownClientConnections[remote_endpoint_].file_name;
                std::ofstream ofs(output_file, std::ios::binary);
                if (!ofs) {
                    LOG(LoggerVerbosity::CRITICAL, remote_str +
                        ":FM: Error opening output file: " + output_file);
                    return;
                }
                for (const auto& [chunkNum, chunkData] : KnownClientConnections[remote_endpoint_].file_chunks) {
                    ofs.write(reinterpret_cast<const char*>(chunkData.data()), chunkData.size());
                }
                ofs.close();
                LOG(LoggerVerbosity::INFO, remote_str +
                    ":FM: File saved successfully to " + output_file);
            }
        }
        else {
            std::cout << "Unhandled Mode: "
                << std::string(magic_enum::enum_name(KnownClientConnections[remote_endpoint_].mode))
                << " for endpoint " << remote_endpoint_ << std::endl;
        }
    }
};

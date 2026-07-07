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

#include <stdexcept>
#include <map>
#include <vector>
#include <chrono>

#include <boost/asio.hpp>


using boost::asio::ip::udp;
using namespace my_logger;
namespace fs = std::filesystem;

static const std::size_t CHUNK_SIZE = 1024; // safe UDP payload

enum class UdpSendMode : int {
    NOTSET = 0,
    MESSAGE = 1,
    SEND_FILE = 2,
    PACKET = 3
};

enum class UdpStriperPortE : int {
    NOTSET = 0,
    STRIPER_PORT_DATA = 1,
    STRIPER_PORT_FEC_ROW = 2,
    STRIPER_PORT_FEC_COL = 3,
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
    uint16_t srcPort = 0;
    uint32_t srcIP = 0;
    std::map<uint16_t, std::vector<uint8_t>> file_chunks; // For storing file chunks if in SEND_FILE mode
    std::string file_name; // Store the file name being sent by the client
    std::chrono::system_clock::time_point connection_time; // Track when the connection was established

    udp_connection() {}
};


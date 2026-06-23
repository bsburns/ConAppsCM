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

#pragma once
/*------------------------------------------------------------------
 * Packet Header Header File
 * 
 * Provides a class to manage packet headers, including serialization 
 * and deserialization of header data.  
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <string>
#include <vector>
#include <functional>
#include <typeinfo>  // For typeid
#include <iostream> // For std::cout
#include <iomanip> // For std::setw and std::setfill

#include "utility.h"
enum class PacketHeaderType : int {
    // Define your packet header types here
	NOTSET = 0,
	ICMP = 1,
	IGMP = 2,
	TCP = 6,
    UDP = 17,
	IPv6 = 41,
	GRE = 47,
	ESP = 50,
	AH = 51,
	ICMPv6 = 58,
	OSPF = 89,
	SCTP = 132,
	RTP = 1000,
	SMPTE = 1001,
};

class PacketHeaderBase {
public:
	PacketHeaderType packetType;

	// Constructor
	PacketHeaderBase() { packetType = PacketHeaderType::NOTSET; }

    // Serialization function
    virtual std::vector<uint8_t> serialize() const = 0;

    // Deserialization function
    virtual std::unique_ptr<PacketHeaderBase> deserialize(const std::vector<uint8_t>& data) = 0;
	virtual ~PacketHeaderBase() = default;
};

class PacketHeaderTCP : public PacketHeaderBase {
public:
	uint16_t sourcePort;
	uint16_t destPort;
	uint32_t sequenceNumber;
	uint32_t acknowledgmentNumber;
	uint8_t dataOffset; // 4 bits
	uint8_t flags; // 6 bits
	uint16_t windowSize;
	uint16_t checksum;
	uint16_t urgentPointer;
	PacketHeaderTCP() {
		packetType = PacketHeaderType::TCP;
	}
	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(20); // TCP header is typically 20 bytes
		// Serialize fields into byte vector (big-endian)
		data[0] = sourcePort >> 8;
		data[1] = sourcePort & 0xFF;
		data[2] = destPort >> 8;
		data[3] = destPort & 0xFF;
		data[4] = sequenceNumber >> 24;
		data[5] = (sequenceNumber >> 16) & 0xFF;
		data[6] = (sequenceNumber >> 8) & 0xFF;
		data[7] = sequenceNumber & 0xFF;
		data[8] = acknowledgmentNumber >> 24;
		data[9] = (acknowledgmentNumber >> 16) & 0xFF;
		data[10] = (acknowledgmentNumber >> 8) & 0xFF;
		data[11] = acknowledgmentNumber & 0xFF;
		data[12] = (dataOffset << 4) | (flags >> 2);
		data[13] = ((flags & 0x03) << 6) | (windowSize >> 8);
		data[14] = windowSize & 0xFF;
		data[15] = checksum >> 8;
		data[16] = checksum & 0xFF;
		data[17] = urgentPointer >> 8;
		data[18] = urgentPointer & 0xFF;
		return data;
	}
	
	std::unique_ptr<PacketHeaderBase> deserialize(const std::vector<uint8_t>& data) override {
		if (data.size() < 20) {
			throw std::invalid_argument("Data too short for TCP header");
		}
		auto header = std::make_unique<PacketHeaderTCP>();
		header->sourcePort = (data[0] << 8) | data[1];
		header->destPort = (data[2] << 8) | data[3];
		header->sequenceNumber = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
		header->acknowledgmentNumber = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
		header->dataOffset = data[12] >> 4;
		header->flags = ((data[12] & 0x0F) << 2) | (data[13] >> 6);
		header->windowSize = ((data[13] & 0x3F) << 8) | data[14];
		header->checksum = (data[15] << 8) | data[16];
		header->urgentPointer = (data[17] << 8) | data[18];
		return header;
	}
};

class PacketHeaderUDP : public PacketHeaderBase {
public:
	uint16_t sourcePort;
	uint16_t destPort;
	uint16_t length;
	uint16_t checksum;
	PacketHeaderUDP() {
		packetType = PacketHeaderType::UDP;
	}
	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(8); // UDP header is 8 bytes
		data[0] = sourcePort >> 8;
		data[1] = sourcePort & 0xFF;
		data[2] = destPort >> 8;
		data[3] = destPort & 0xFF;
		data[4] = length >> 8;
		data[5] = length & 0xFF;
		data[6] = checksum >> 8;
		data[7] = checksum & 0xFF;
		return data;
	}
	std::unique_ptr<PacketHeaderBase> deserialize(const std::vector<uint8_t>& data) override {
		if (data.size() < 8) {
			throw std::invalid_argument("Data too short for UDP header");
		}
		auto header = std::make_unique<PacketHeaderUDP>();
		header->sourcePort = (data[0] << 8) | data[1];
		header->destPort = (data[2] << 8) | data[3];
		header->length = (data[4] << 8) | data[5];
		header->checksum = (data[6] << 8) | data[7];
		return header;
	}
};

class PacketHeaderRTP: public PacketHeaderBase{
public:
	uint8_t version = 2;
	bool padded = false;
	bool extension = false;
	uint8_t CSRC_Count;
	bool marker = false;
	uint8_t payload_type;
	uint16_t sequence_number;
	uint32_t timestamp;
	uint32_t sync_src;

	PacketHeaderRTP() {
		packetType = PacketHeaderType::RTP;
	}
	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(12); // RTP header is 12 bytes
		data[0] = (version << 6) | (padded << 5) | (extension << 4) | (CSRC_Count & 0xF);
		data[1] = (marker << 7) | (payload_type & 0x7F);
		data[2] = sequence_number >> 8;
		data[3] = sequence_number & 0xFF;
		data[4] = timestamp >> 24;
		data[5] = (timestamp >> 16) & 0xFF;
		data[6] = (timestamp >> 8) & 0xFF;
		data[7] = timestamp & 0xFF;
		data[8] = sync_src >> 24;
		data[9] = (sync_src >> 16) & 0xFF;
		data[10] = (sync_src >> 8) & 0xFF;
		data[11] = sync_src & 0xFF;
		return data;
	}
	std::unique_ptr<PacketHeaderBase> deserialize(const std::vector<uint8_t>& data) override {
		if (data.size() < 12) {
			throw std::invalid_argument("Data too short for RTP header");
		}
		auto header = std::make_unique<PacketHeaderRTP>();
		header->version = (data[0] >> 6);
		header->padded = (data[0] >> 5) & 0x1;
		header->extension = (data[0] >> 4) & 0x1;
		header->CSRC_Count = data[0] & 0xF;
		header->marker = (data[1] >> 7) & 0x1;
		header->payload_type = data[1] & 0x7F;
		header->sequence_number = data[2] << 8 | data[3];
		header->timestamp = data[4] << 24 | data[5] << 16 | data[6] << 8 | data[7];
		header->sync_src = data[8] << 24 | data[9] << 16 | data[10] << 8 | data[11];
		return header;
	}
};

class PacketHeaderSMPTE : public PacketHeaderBase {
public:
	bool extension = false;
	bool reserved = false;
	bool padding_recovery = false;
	bool extension_recovery = false;
	uint8_t CSRC_recovery;
	bool marker_recovery;
	uint8_t payload_type_recovery;
	uint16_t sequence_base;
	uint32_t timestamp_recovery;
	uint16_t length_recovery;
	uint16_t reserved1 = 0;
	uint16_t offset;
	uint8_t reserved2 = 0;
	uint16_t NA_D_or_L;
	uint8_t reserved3 = 0;

	PacketHeaderSMPTE() { 
		packetType = PacketHeaderType::SMPTE; 
	}

	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(16); // SMPTE header is 16 bytes
		data[0] = (extension << 7) | (reserved << 6) | (padding_recovery << 5) | (extension_recovery << 4) | (CSRC_recovery & 0xF);
		data[1] = (marker_recovery << 7) | (payload_type_recovery & 0x7F);
		data[2] = sequence_base >> 8;
		data[3] = sequence_base & 0xFF;
		data[4] = timestamp_recovery >> 24;
		data[5] = (timestamp_recovery >> 16) & 0xFF;
		data[6] = (timestamp_recovery >> 8) & 0xFF;
		data[7] = timestamp_recovery & 0xFF;
		data[8] = length_recovery >> 8;
		data[9] = length_recovery & 0xFF;
		data[10] = (reserved1 >> 8) & 0xFF;
		data[11] = reserved1 & 0xFF;
		data[12] = offset >> 2;
		data[13] = (offset & 0x3) << 6 | (reserved2 & 0x3F);
		data[14] = NA_D_or_L >> 2;
		data[15] = (NA_D_or_L & 0xFF) >> 6 | (reserved3 & 0x3F);

		return data;
	}
	std::unique_ptr<PacketHeaderBase> deserialize(const std::vector<uint8_t>& data) override {
		if (data.size() < 16) {
			throw std::invalid_argument("Data too short for SMPTE header");
		}
		auto header = std::make_unique<PacketHeaderSMPTE>();
		header->extension = (data[0] >> 7);
		header->reserved = (data[0] >> 6) & 0x1;
		header->padding_recovery = (data[0] >> 5) & 0x1;
		header->extension_recovery = (data[0] >> 4) & 0x1;
		header->CSRC_recovery = data[0] & 0xF;
		header->marker_recovery = data[1] >> 7 & 0x1;
		header->padding_recovery = data[1] & 0x7F;
		header->sequence_base = data[2] << 8 | data[3];
		header->timestamp_recovery = data[4] << 24 | data[5] << 16 | data[6] << 8 | data[7];
		header->length_recovery = data[8] << 8 | data[9];
		header->reserved1 = data[10] << 8 | data[11];
		header->offset = data[12] << 8 | (data[13] >> 6) & 0x3;
		header->reserved2 = data[13] & 0x3F;
		header->NA_D_or_L = data[14] << 2 | (data[15] >> 6) & 0x3;
		header->reserved3 = data[15] & 0x3F;

		return header;
	}



};

class Packet {
public:
	std::vector<PacketHeaderBase> headers;
	std::vector<uint8_t> data;
	Packet() {
		headers.clear();
		data.clear();
	}
};
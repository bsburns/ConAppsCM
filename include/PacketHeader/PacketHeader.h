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
	PacketHeaderType header_type;

	// Constructor
	PacketHeaderBase() { header_type = PacketHeaderType::NOTSET; }

    // Serialization function
    virtual std::vector<uint8_t> serialize() const = 0;

    // Deserialization function
    virtual std::unique_ptr<PacketHeaderBase> deserialize(const std::vector<uint8_t>& data) = 0;

	virtual uint32_t Size() const = 0;

	virtual std::string to_string() const = 0;

	virtual ~PacketHeaderBase() = default;
};

class PacketHeaderTCP : public PacketHeaderBase {
public:
	uint16_t srcPort=0;
	uint16_t dstPort=0;
	uint32_t sequenceNumber=0;
	uint32_t acknowledgmentNumber=0;
	uint8_t dataOffset=0; // 4 bits
	uint8_t flags=0; // 6 bits
	uint16_t windowSize=0;
	uint16_t checksum=0;
	uint16_t urgentPointer=0;
	PacketHeaderTCP() {
		header_type = PacketHeaderType::TCP;
	}
	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(Size()); // TCP header is typically 20 bytes
		// Serialize fields into byte vector (big-endian)
		data[0] = srcPort >> 8;
		data[1] = srcPort & 0xFF;
		data[2] = dstPort >> 8;
		data[3] = dstPort & 0xFF;
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
		if (data.size() < Size()) {
			throw std::invalid_argument("Data too short for TCP header");
		}
		auto header = std::make_unique<PacketHeaderTCP>();
		header->srcPort = (data[0] << 8) | data[1];
		header->dstPort = (data[2] << 8) | data[3];
		header->sequenceNumber = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
		header->acknowledgmentNumber = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
		header->dataOffset = data[12] >> 4;
		header->flags = ((data[12] & 0x0F) << 2) | (data[13] >> 6);
		header->windowSize = ((data[13] & 0x3F) << 8) | data[14];
		header->checksum = (data[15] << 8) | data[16];
		header->urgentPointer = (data[17] << 8) | data[18];
		return header;
	}
	
	uint32_t Size() const override { return 20; }

	std::string to_string() const override {
		std::string str = "TCP={";
		str += "SrcPort=" + std::to_string(srcPort);
		str += ", DstPort=" + std::to_string(dstPort);
		str += ", Seq=" + std::to_string(sequenceNumber);
		str += "}";
		return str;
	}
};

class PacketHeaderUDP : public PacketHeaderBase {
public:
	uint16_t srcPort = 0;
	uint16_t dstPort = 0;
	uint16_t length = 0;
	uint16_t checksum = 0;
	PacketHeaderUDP() {
		header_type = PacketHeaderType::UDP;
	}
	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(Size()); // UDP header is 8 bytes
		data[0] = srcPort >> 8;
		data[1] = srcPort & 0xFF;
		data[2] = dstPort >> 8;
		data[3] = dstPort & 0xFF;
		data[4] = length >> 8;
		data[5] = length & 0xFF;
		data[6] = checksum >> 8;
		data[7] = checksum & 0xFF;
		return data;
	}
	std::unique_ptr<PacketHeaderBase> deserialize(const std::vector<uint8_t>& data) override {
		if (data.size() < Size()) {
			throw std::invalid_argument("Data too short for UDP header");
		}
		auto header = std::make_unique<PacketHeaderUDP>();
		header->srcPort = (data[0] << 8) | data[1];
		header->dstPort = (data[2] << 8) | data[3];
		header->length = (data[4] << 8) | data[5];
		header->checksum = (data[6] << 8) | data[7];
		return header;
	}
	uint32_t Size() const override { return 8; }

	std::string to_string() const override {
		std::string str = "UDP={";
		str += "SrcPort=" + std::to_string(srcPort);
		str += ", DstPort=" + std::to_string(dstPort);
		str += ", len=" + std::to_string(length);
		str += "}";
		return str;
	}
};

class PacketHeaderRTP: public PacketHeaderBase{
public:
	uint8_t version = 2;
	bool padded = false;
	bool extension = false;
	uint8_t CSRC_Count = 0;
	bool marker = false;
	uint8_t payload_type = 0;
	uint16_t sequence_number = 0;
	uint32_t timestamp = 0;
	uint32_t sync_src = 0;

	PacketHeaderRTP() {
		header_type = PacketHeaderType::RTP;
	}
	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(Size()); // RTP header is 12 bytes
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
		if (data.size() < Size()) {
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
	uint32_t Size() const override { return 12; }

	std::string to_string() const override {
		std::string str = "RTP={";
		str += "}";
		return str;
	}

};

class PacketHeaderSMPTE : public PacketHeaderBase {
public:
	bool extension = false;
	bool reserved = false;
	bool padding_recovery = false;
	bool extension_recovery = false;
	uint8_t CSRC_recovery = 0;
	bool marker_recovery = false;
	uint8_t payload_type_recovery = 0;
	uint16_t sequence_base = 0;
	uint32_t timestamp_recovery = 0;
	uint16_t length_recovery = 0;
	uint16_t reserved1 = 0;
	uint16_t offset = 0;
	uint8_t reserved2 = 0;
	uint16_t NA_D_or_L = 0;
	uint8_t reserved3 = 0;

	PacketHeaderSMPTE() { 
		header_type = PacketHeaderType::SMPTE; 
	}

	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(Size()); // SMPTE header is 16 bytes
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
		if (data.size() < Size()) {
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

	uint32_t Size() const override { return 16; }

	std::string to_string() const override {
		std::string str = "SMPTE={";
		str += "}";
		return str;
	}
};

class PacketHeaders {
public:
	uint32_t length = 0;
	std::vector<std::shared_ptr<PacketHeaderBase>> headers;
	PacketHeaders() {
		headers.clear();
	}

	void AddHeader(std::shared_ptr<PacketHeaderBase> hdr, int position = 0) {
		if (position == 0) {
			headers.insert(headers.begin(), hdr);
		} else if (position == -1) {
			headers.emplace_back(hdr);
		} else {
			if (position > 0 && position < headers.size()) {
				headers.insert(headers.begin() + position, hdr);
			} else {
				std::cerr << "Header Insertion location out of range: loc="
					<< position << " MaxPosition=" << headers.size() - 1;
			}
		}
		length += hdr->Size();
	}
};
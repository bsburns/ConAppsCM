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
#include "logger.h"

using namespace my_logger;

enum class PacketHeaderType : uint8_t {
    // Define your packet header types here
	NOTSET = 0,
	IPv4 = 4,
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
	RTP = 100,
	SMPTE = 101,
};

static std::string ip_to_string(uint32_t ip) {
	return std::to_string((ip >> 24) & 0xFF) + "." +
		std::to_string((ip >> 16) & 0xFF) + "." +
		std::to_string((ip >> 8) & 0xFF) + "." +
		std::to_string(ip & 0xFF);
}

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

	virtual void Reset() = 0;

	virtual std::string to_string() const = 0;

	virtual ~PacketHeaderBase() = default;
};

class PacketHeaderIPv4 : public PacketHeaderBase {
public:
	uint8_t Version;
	uint8_t IHL;
	uint8_t TOS;
	uint16_t totalLength;
	uint16_t ID; // 4 bits
	uint8_t flags; // 6 bits
	uint16_t fragmentOffset;
	uint8_t TTL;
	uint8_t Protocol;
	uint16_t headerChecksum;
	uint32_t srcIP;
	uint32_t dstIP;

	PacketHeaderIPv4() {
		header_type = PacketHeaderType::IPv4;
		Reset();
	}

	void Reset() override {
		Version = 0;
		IHL = 0;
		TOS = 0;
		totalLength = 0;
		ID = 0; // 4 bits
		flags = 0; // 6 bits
		fragmentOffset = 0;
		TTL = 0;
		Protocol = 0;
		headerChecksum = 0;
		srcIP = 0;
		dstIP = 0;
	}

	std::vector<uint8_t> serialize() const override {
		std::vector<uint8_t> data(Size()); // TCP header is typically 20 bytes
		// Serialize fields into byte vector (big-endian)
		data[0] = (Version & 0xF) << 4 | (IHL & 0xF);
		data[1] = TOS;
		data[2] = totalLength >> 8;
		data[3] = totalLength & 0xFF;
		data[4] = ID >> 8;
		data[5] = ID & 0xFF;
		data[6] = (flags & 0xF) << 4 | ((fragmentOffset >> 8) & 0xF);
		data[7] = fragmentOffset & 0xFF;
		data[8] = TTL;
		data[9] = Protocol;
		data[10] = (headerChecksum >> 8) & 0xFF;
		data[11] = headerChecksum & 0xFF;
		data[12] = (srcIP >> 24) & 0xFF;
		data[13] = (srcIP >> 16) & 0xFF;
		data[14] = (srcIP >> 8) & 0xFF;
		data[15] = srcIP & 0xFF;
		data[16] = (dstIP >> 24) & 0xFF;
		data[17] = (dstIP >> 16) & 0xFF;
		data[18] = (dstIP >> 8) & 0xFF;
		data[19] = dstIP & 0xFF;
		return data;
	}
	
	std::unique_ptr<PacketHeaderBase> deserialize(const std::vector<uint8_t>& data) override {
		if (data.size() < Size()) {
			throw std::invalid_argument("Data too short for TCP header");
		}
		auto header = std::make_unique<PacketHeaderIPv4>();
		header->Version = (data[0] >> 4) & 0xF;
		header->IHL = data[0] & 0xF;
		header->TOS = data[1];
		header->totalLength = (data[2] << 8) | data[3];
		header->ID = (data[4] << 8) | data[5];
		header->flags = (data[6] >> 4) & 0xF;
		header->fragmentOffset = ((data[6] & 0xF) << 8) | data[7];
		header->TTL = data[8];
		header->Protocol = data[9];
		header->headerChecksum = (data[10] << 8) | data[11];
		header->srcIP = (data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15];
		header->dstIP = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];
		return header;
	}
	
	uint32_t Size() const override { return 20; }

	std::string to_string() const override {
		std::string str = "IPv4={";
		str += "TOS=" + std::to_string(TOS);
		str += ", Length=" + std::to_string(totalLength);
		str += ", TTL=" + std::to_string(TTL);
		str += ", SRC=" + ip_to_string(srcIP);
		str += ", DST=" + ip_to_string(dstIP);
		str += "}";
		return str;
	}
};

class PacketHeaderTCP : public PacketHeaderBase {
public:
	uint16_t srcPort;
	uint16_t dstPort;
	uint32_t sequenceNumber;
	uint32_t acknowledgmentNumber;
	uint8_t dataOffset; // 4 bits
	uint8_t flags; // 6 bits
	uint16_t windowSize;
	uint16_t checksum;
	uint16_t urgentPointer;

	PacketHeaderTCP() {
		header_type = PacketHeaderType::TCP;
		Reset();
	}

	void Reset() override {
		srcPort = 0;
		dstPort = 0;
		sequenceNumber = 0;
		acknowledgmentNumber = 0;
		dataOffset = 0; // 4 bits
		flags = 0; // 6 bits
		windowSize = 0;
		checksum = 0;
		urgentPointer = 0;
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
	uint16_t srcPort;
	uint16_t dstPort;
	uint16_t length;
	uint16_t checksum;

	PacketHeaderUDP() {
		header_type = PacketHeaderType::UDP;
		Reset();
	}

	void Reset() override {
		srcPort = 0;
		dstPort = 0;
		length = 0;
		checksum = 0;
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
	uint8_t version;
	bool padded;
	bool extension;
	uint8_t CSRC_Count;
	bool marker;
	uint8_t payload_type;
	uint16_t sequence_number;
	uint32_t timestamp;
	uint32_t sync_src;

	PacketHeaderRTP() {
		header_type = PacketHeaderType::RTP;
		Reset();
	}

	void Reset() override {
		version = 2;
		padded = false;
		extension = false;
		CSRC_Count = 0;
		marker = false;
		payload_type = 0;
		sequence_number = 0;
		timestamp = 0;
		sync_src = 0;
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
		str += "Seq=" + std::to_string(sequence_number);
		str += ", TS=" + std::to_string(timestamp);
		str += ", PT=" + std::to_string(payload_type);
		str += "}";
		return str;
	}

};

class PacketHeaderSMPTE : public PacketHeaderBase {
public:
	bool extension;
	bool reserved;
	bool padding_recovery;
	bool extension_recovery;
	uint8_t CSRC_recovery;
	bool marker_recovery;
	uint8_t payload_type_recovery;
	uint16_t sequence_base;
	uint32_t timestamp_recovery;
	uint16_t length_recovery;
	uint16_t reserved1;
	uint16_t offset;
	uint8_t reserved2;
	uint16_t NA_D_or_L;
	uint8_t reserved3;

	PacketHeaderSMPTE() { 
		header_type = PacketHeaderType::SMPTE; 
		Reset();
	}

	void Reset() override {
		extension = false;
		reserved = false;
		padding_recovery = false;
		extension_recovery = false;
		CSRC_recovery = 0;
		marker_recovery = false;
		payload_type_recovery = 0;
		sequence_base = 0;
		timestamp_recovery = 0;
		length_recovery = 0;
		reserved1 = 0;
		offset = 0;
		reserved2 = 0;
		NA_D_or_L = 0;
		reserved3 = 0;
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
				LOG(LoggerVerbosity::ERR, "Header Insertion location out of range: loc="
					+ std::to_string(position)
					+ " MaxPosition=" + std::to_string(headers.size() - 1)
				);
			}
		}
		length += hdr->Size();
	}

	int MakePacket(std::vector<uint8_t>& packet, std::size_t& length) {
		for (auto it = headers.rbegin(); it < headers.rend(); ++it) {
			auto hdr_data = (*it)->serialize();
			length += (*it)->Size();
			packet.insert(packet.begin(), hdr_data.begin(), hdr_data.end());
		}
		if (length != packet.size()) {
			LOG(LoggerVerbosity::ERR, "Packet length mismatch: length="
				+ std::to_string(length)
				+ " packet.size()=" + std::to_string(packet.size())
			);
			return -1;
		}
		headers.clear(); // Since headers have been added to packet, delete them
		return 0;
	}


	void Clear() { length = 0; headers.clear(); }

	std::string ToString() const {
		std::string str = "PacketHeaders=[";
		for (auto it = headers.begin(); it != headers.end(); ++it) {
			str += (*it)->to_string();
			if (std::next(it) != headers.end()) {
				str += ", ";
			}
		}
		str += "]";
		return str;
	}
};
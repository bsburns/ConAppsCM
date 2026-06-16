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

class PacketHeader {
public:
    // Packet header fields
    uint32_t version;
    uint32_t packetType;
    uint32_t packetLength;
    // Add other header fields as needed

    // Constructor
    PacketHeader() : version(0), packetType(0), packetLength(0) {}

    // Serialization function
    std::vector<uint8_t> serialize() const {};

    // Deserialization function
    static PacketHeader deserialize(const std::vector<uint8_t>& data) {};
};


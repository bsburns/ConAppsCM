#pragma once
/*------------------------------------------------------------------
 * Utility Header File
 *
 * General purpose utility functions
 * 
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <iostream>
#include <sstream>
#include <fstream>
#include <list>
#include <string>
#include <vector>
#include <ctime>

namespace util {

inline std::string trim(const std::string& s) {
    // Whitespace is one of: space, tab, carriage return,
    // line feed, form feed, or vertical tab.
    const char* whitespace = " \t\n\r\f\v";
    size_t begin = s.find_first_not_of(whitespace);
    if (begin == std::string::npos) {
        return std::string{};
    }
    size_t end = s.find_last_not_of(whitespace);
    return std::string{ s.substr(begin, end - begin + 1) };
}

inline std::vector<std::string> splitBySpace(const std::string &input) 
{
	std::vector<std::string> tokens;
	std::istringstream stream(input);
	std::string word;

	// Extract words separated by whitespace
	while (stream >> word) {
		tokens.push_back(word);
	}

	return tokens;
}

// Save a list of strings to a file (one per line)
inline bool saveListToFile(const std::list<std::string>& data, const std::string& filename) {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << filename << "\n";
        return false;
    }

    for (const auto& item : data) {
        outFile << item << "\n";
        if (!outFile) { // Check for write errors
            std::cerr << "Error: Failed to write to file.\n";
            return false;
        }
    }

    return true;
}

// Load a list of strings from a file (one per line)
inline bool loadListFromFile(std::list<std::string>& data, const std::string& filename) {
    std::ifstream inFile(filename);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open file for reading: " << filename << "\n";
        return false;
    }

    data.clear(); // Ensure list is empty before loading
    std::string line;
    while (std::getline(inFile, line)) {
        data.push_back(line);
    }

    if (inFile.bad()) { // Check for read errors
        std::cerr << "Error: Failed to read from file.\n";
        return false;
    }

    return true;
}

// Function to convert std::tm to formatted string
//std::string tmToString(const std::tm& timeStruct, const std::string& format = "%Y-%m-%d %H:%M:%S") {
//    std::ostringstream oss;
//    oss << std::put_time(&timeStruct, format.c_str()); // C++11 and later
//    return oss.str();
//}

} // end namespace util
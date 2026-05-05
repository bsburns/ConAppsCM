#pragma once
/*------------------------------------------------------------------
 * Simulation Configuration Header File
 *
 * Contains Simulator configuration data structures and functions to load/save configuration from/to file.
 * CLI commands to manipulate configuration at runtime will also be implemented here.
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */

#include <iostream>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "utilities/logger.h"
#include "cli/CliMenu.h"
#include "SimTypes.h"

struct RootSimConfig {
    uint64_t SchedulerUpdateInterval_ns=1000;
    uint64_t StatisticsUpdateInterval_ns=1'000'000;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RootSimConfig, SchedulerUpdateInterval_ns, StatisticsUpdateInterval_ns)

struct DataStreamConfig {
    std::string StreamName = "NA";
    double PacketRate_pps = 0;
	uint32_t PacketSize_bytes = 0;
    uint32_t MinBurstSizePackets = 0;
    uint32_t MaxBurstSizePackets = 0;
	uint32_t NumberPacketsToGenerate = 10; // 0 means infinite
    uint32_t StartTime_ns = 0;

    const std::string ToString() {
        return std::format("DataStreamConfig{{StreamName={}, PacketRate_pps={}, PacketSize_bytes={}, MinBurstSizePackets={}, MaxBurstSizePackets={}, NumberPacketsToGenerate={}, StartTime_ns={}}}", 
			StreamName, PacketRate_pps, PacketSize_bytes, MinBurstSizePackets, MaxBurstSizePackets, NumberPacketsToGenerate, StartTime_ns);
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DataStreamConfig, StreamName, PacketRate_pps, PacketSize_bytes, MinBurstSizePackets, MaxBurstSizePackets, NumberPacketsToGenerate, StartTime_ns)

struct StripeConfig {
    FEC_Mode Mode = FEC_Mode::NONE;     // FEC Mode
    uint32_t Parm_L_cols = 5;           // FEC Parameter L - number of columns in matrix
    uint32_t Parm_D_rows = 100;         // FEC Parameter D - number of rows in matrix
    bool AutoFillEnable = false;        // Enable Auto fill, if packet rate drops below TargetPacketRate_pps
    double AbsMaxPacketRate_pps = 0;    // Absolute maximum packet Rate that a stripe can generate
    double ReportMaxPacketRate_pps = 0; // Once actual Rate exceeds this amount the event will be reported to scheduler
    double ReportMinPacketRate_pps = 0; // Once actual Rate drops below this amount the event will be reported to scheduler
    double TargetPacketRate_pps = 0;    // If actual rate drops below this, then fill packets will be injected
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StripeConfig, 
    Mode,
    Parm_L_cols,
    Parm_D_rows,
    AbsMaxPacketRate_pps, 
    ReportMaxPacketRate_pps, 
    ReportMinPacketRate_pps, 
    TargetPacketRate_pps)
 
struct SchedulerConfig {
    SchedulerMode Mode=SchedulerMode::DYNAMIC_STRIPES;
    int MinNumberStripes=1; // Minimum number of active stripes
    int MaxNumberStripes=16; // Maximum number of active stripes
    uint32_t RevaluationInterval_ns=100; // period between modifing the number of active stripes
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SchedulerConfig, Mode, MinNumberStripes, MaxNumberStripes, RevaluationInterval_ns)

struct OutputConfig {
    std::string OutputFile = "";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(OutputConfig, OutputFile)

struct AllConfig {
    RootSimConfig rootCfg;
    SchedulerConfig schedulerCfg;
    StripeConfig stripeCfg;
    OutputConfig outputCfg;
    std::list<DataStreamConfig> dataStrmCfgList;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AllConfig, rootCfg, schedulerCfg, stripeCfg, outputCfg, dataStrmCfgList)

class SimConfigManager {
private:
	std::mutex mtx; // Mutex to protect access to configuration data
    SimConfigManager() {
        // Initialize with default values
        all_cfg = {
            .rootCfg = {1'000, 100'000'000},
            .schedulerCfg = {SchedulerMode::DYNAMIC_STRIPES, 1, 2, 100},
            .stripeCfg = {FEC_Mode::NONE, 5, 100, false, 41'000, 40'000, 8'000, 8'000},
            .outputCfg ={"default_pkt.csv"},
            .dataStrmCfgList = {{"StrmTest1", 1234.0, 1500, 1, 1, 2, 0},},
        };
    }

    int resoveFilename(std::string& filename, const std::string& mode) {
        if (filename.empty()) {
            std::cerr << "\nNo filename provided for "<< mode << " config.\n";
            return -1;
        }
        if (filename == "default") {
            std::cerr << "\nCannot "<<mode<<" 'default' config. Please provide a valid filename.\n";
            return -2;
        }
        if (filename == "last") {
            filename = "config_last.json";
            std::cout << "\n"<<mode<<" config: " << filename << "\n";
        }
        if (filename == "auto") {
            filename = "config_auto.json";
            std::cout << "\n" << mode << " config: " << filename << "\n";
        }
        if (filename == "none") {
            std::cout << "\nfilename 'none' not valid.\n";
            return -3;
        }
        if (filename.find('.') == std::string::npos) { // No extension applied
            filename += ".json";
            std::cout << "\nNo extension, so adding .json: " << filename << "\n";
        }
        if (filename.find('/') == std::string::npos) { // No path applied, save to current directory
            filename = "./" + filename;
        }
        return 0;
    }

public:
    AllConfig all_cfg;
    const MenuItem cli_menu = {
        .name = "config",
        .description = "Commands to control configuration",
        .subMenus = {
            {
                .name = "save", 
                .description = "Save current configuration to file, arg is filename", 
                .subMenus = {}, 
                .valType = MenuItemValueTypes::STRING,
                .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->SaveConfig(argument); },
            },
            {
                .name = "load", 
                .description = "Load configuration from file, arg is filename", 
                .subMenus = {}, 
                .valType = MenuItemValueTypes::STRING,
                .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->LoadConfig(argument); },
            },
            {
                .name = "show",
                .description = "Show configuration",
                .subMenus = {},
                .valType = MenuItemValueTypes::NONE,
                .executeCommand = [this](MenuItemValueTypes vt, const std::string& argument) {this->ShowConfig(); },
            },
        },
        .valType = MenuItemValueTypes::SUBMENU,
        .executeCommand = [](MenuItemValueTypes vt, const std::string& argument) {},
	};

	SimConfigManager(const SimConfigManager&) = delete; // Delete copy constructor
	SimConfigManager& operator=(const SimConfigManager&) = delete; // Delete copy assignment operator
	SimConfigManager(SimConfigManager&&) = delete; // Delete move constructor
	SimConfigManager& operator=(SimConfigManager&&) = delete; // Delete move assignment operator

    static SimConfigManager& GetInstance() {
        static SimConfigManager instance; // Guaranteed to be destroyed and instantiated on first use
        return instance;
	}

    AllConfig& GetConfig() {
        std::lock_guard<std::mutex> lock(mtx);
        return all_cfg;
	}

    int LoadConfig(std::string filename) {
        std::lock_guard<std::mutex> lock(mtx);

        if (filename == "default") {
            std::cout << "\nLoading default configuration.\n";
            all_cfg = {
                .rootCfg = {1'000, 100'000'000},
                .schedulerCfg = {SchedulerMode::DYNAMIC_STRIPES, 1, 2, 100},
                .stripeCfg = {FEC_Mode::NONE, 5, 100, false, 41'000, 40'000, 8'000, 8'000},
                .outputCfg ={"default_pkt.csv"},
                .dataStrmCfgList = {{"StrmTest1", 1234.0, 1500, 1, 1, 2, 0},},
            };
            return 0;
        }


        auto rc = resoveFilename(filename, "Loading");
		std::cout << "\nresolveFN: rc=" << rc << ", filename=" << filename << "\n";
        if (rc != 0) {
            return rc;
        }
        try {
			std::ifstream ifs(filename);
            if (!ifs.is_open()) {
                std::cerr << "\nCannot open config file: " << filename << "\n";
                return -1;
            }
			// Parse JSON directly into AllConfig struct
            json j;
            ifs >> j;
            all_cfg = j.get<AllConfig>();
			std::cout << "\nConfig loaded successfully from " << filename << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "\nError loading config: " << e.what() << "\n";
            return -1;
        }
        return 0;
    }
    int SaveConfig(std::string filename) {
        std::lock_guard<std::mutex> lock(mtx);
        if (filename.empty()) {
            std::cerr << "\nNo filename provided for saving config.\n";
            return -1;
        }
        if (filename == "default") {
            std::cerr << "\nCannot save to 'default' config. Please provide a valid filename.\n";
            return -1;
		}
        if (filename == "last") {
            filename = "config_last.json";
            std::cout << "\nSaving config to " << filename << "\n";
		}
        if (filename == "auto") {
            filename = "config_auto.json";
            std::cout << "\nSaving config to " << filename << "\n";
        }
        if (filename == "none") {
            std::cout << "\nNot saving config (filename 'none' specified).\n";
            return 0;
		}
        if (filename.find('.') == std::string::npos) { // No extension applied
            filename += ".json";
            std::cout << "\nNo extension, so adding .json: " << filename << "\n";
        }
        if (filename.find('/') == std::string::npos) { // No path applied, save to current directory
            filename = "./" + filename;
        }

         // At this point we should have a valid filename to save to, if not, we will just let the ofstream error out and report it
          
         std::cout << "\nSaving config to " << filename << "\n";
          
		 // Save config to file

        try {
            std::ofstream ofs(filename);
            if (!ofs) {
                std::cerr << "\nCannot open config file for writing: " << filename << "\n";
                return -1;
            }
            json j = all_cfg;
            ofs << j.dump(4); // Pretty print with 4 spaces indent
        } catch (const std::exception& e) {
            std::cerr << "\nError saving config: " << e.what() << "\n";
            return -1;
		}
        return 0;
    }
    void ShowConfig() {
        std::lock_guard<std::mutex> lock(mtx);
        json j = all_cfg;
        std::cout << "\nCurrent Configuration:\n" << j.dump(4) << "\n";
	}
};


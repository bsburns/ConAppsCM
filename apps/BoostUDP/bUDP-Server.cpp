#include "logger.h"

#include <iostream>
#include <string>
#include <set>
#include <memory>
#include <boost/asio.hpp>
#include <map>
#include <filesystem>
#include <cstdlib>
#include "bUDP.h"
#include "commandLineParser.h"

using boost::asio::ip::udp;

std::string OutDir = "";

void show_version() {
    std::cout << "UDP Server (boost) Version: " << VERSION << "." << GIT_HASH << std::endl;
    std::cout << "Build Date: " << __DATE__ << std::endl;
    std::cout << "Build Time: " << __TIME__ << std::endl;
#ifdef _WIN32
    std::cout << "MSVC Version: " << _MSC_FULL_VER << std::endl;
#else
    std::cout << "Compiler: " << __VERSION__ << std::endl;
#endif
    std::cout << "Build Machine: " << BUILD_MACHINE << std::endl;
}

using namespace my_logger;
namespace fs = std::filesystem;


class udp_connection {
public:
	UdpSendMode mode = UdpSendMode::NOTSET;
	std::map<uint16_t, std::vector<uint8_t>> file_chunks; // For storing file chunks if in SEND_FILE mode
	std::string file_name; // Store the file name being sent by the client
	std::chrono::system_clock::time_point connection_time; // Track when the connection was established

    udp_connection() {}
};

class UdpServer {
public:
    // Bind to the given port on all available network interfaces
    UdpServer(boost::asio::io_context& io_context, short port)
        : socket_(io_context, udp::endpoint(udp::v4(), port)) {
        start_receive();
    }

private:
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
                if (!ec && bytes_transferred > 0) {
                    process_packet(bytes_transferred);
                }
                // Immediately resume listening for other clients
                start_receive();
            });
    }

    void process_packet(std::size_t length) {
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
            } else {
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

                std::string output_file = OutDir + "/" + KnownClientConnections[remote_endpoint_].file_name;
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

int main(int argc, char* argv[]) {
    LoggerVerbosity verbosity = LoggerVerbosity::CRITICAL;
    double WatchdogTimeout = 360;
    std::string LogFile;
    std::string ServerPort = "8080";

    CommandLineParser CLP;
    CLP.AddCommand({
        CLP_Command("version", "Show version information", [](const std::string& argument) {
            show_version();
            exit(0);
        }, "", typeid(void)),
        CLP_Command("verbosity,v", "Set logging verbosity level", [&verbosity](const std::string& argument) {
            std::string uarg = argument;
            std::transform(uarg.begin(), uarg.end(), uarg.begin(), ::toupper);
            verbosity = magic_enum::enum_cast<LoggerVerbosity>(uarg).value_or(LoggerVerbosity::NOTSET);
            if (verbosity == LoggerVerbosity::NOTSET) {
                try {
                    int v_int = std::stoi(argument);
                    verbosity = static_cast<LoggerVerbosity>(v_int);
                }
                catch (const std::exception& e) {
                    std::cerr << "Invalid verbosity level: " << argument << ". Setting to NOTSET.\n";
                }
            }
            std::cout << "\nSetting Logger Verbosity to " << std::string(magic_enum::enum_name(verbosity))
                << " (" << static_cast<int>(verbosity) << ")"
                << " argumnet=" << argument
                << "\n";
        }, "CRITICAL", typeid(std::string)),
        CLP_Command("outdir, d", "Specifies output directory", [](const std::string& argument) {
            OutDir = argument;
        }, "c:\\local\\output", typeid(std::string)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "bUDPS.log", typeid(std::string)),
        CLP_Command("server_port, p", "Specifies UDP Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "8080", typeid(std::string)),
        });

    show_version();
    std::cout << "\n*** Setting DEFAULT command line arguments...***\n";
    CLP.SetDefaultValues();
    std::cout << "\n*** Parsing command line arguments...***\n";
    CLP.ProcessArguments(argc, argv);

	// Validate output directory exists
    fs::path dirPath = OutDir;
    if (!(fs::exists(dirPath) && fs::is_directory(dirPath))) {
        std::cout << "Output Directory does not exist: \"" << OutDir << "\"\n";
        std::cout << "Exiting Program due to non-existence of output directory\n";
        exit(300);
    }

    LOG_INST.SetLogFile(LogFile);
    LOG(verbosity, "Starting log\n");

    try {
        boost::asio::io_context io_context;
        short port = static_cast<short>(std::stoi(ServerPort));
        UdpServer server(io_context, port);
        
        std::cout << "UDP Server running on port " << ServerPort << "..." << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}

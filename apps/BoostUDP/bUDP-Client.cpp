/*------------------------------------------------------------------
 * bUDP-Client.cpp
 * 
 * CBoost based UDP Client application to test the bUDP-Server
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include "logger.h"
#include <iostream>
#include <string>
#include <array>
#include <boost/asio.hpp>


#include "commandLineParser.h"
#include "watchdog.h"
#include "PacketHeader.h"
#include "bUDP.h"

std::string show_version(bool use_cout = false) {
    std::stringstream ss;
    ss << "\n\tUDP Client (boost) Version: " << VERSION << "." << GIT_HASH << std::endl;
    ss << "\tBuild Date: " << __DATE__ << std::endl;
    ss << "\tBuild Time: " << __TIME__ << std::endl;
#ifdef _WIN32
    ss << "\tMSVC Version: " << _MSC_FULL_VER << std::endl;
#else
    ss << "\tCompiler: " << __VERSION__ << std::endl;
#endif
    ss << "\tBuild Machine: " << BUILD_MACHINE << std::endl;

    if (use_cout) {
        std::cout << ss.str();
    }
    return ss.str();
}

using namespace my_logger;
using boost::asio::ip::udp;


int main(int argc, char* argv[]) {
    LoggerVerbosity verbosity = LoggerVerbosity::ERR;
	double WatchdogTimeout = 360;
    std::string LogFile;
    std::string SendFile;
    std::string ServerPort = "8080";
    std::string ServerIP = "127.0.0.1";
	bool interactiveMode = false;
	UdpSendMode sendMode = UdpSendMode::MESSAGE;

    CommandLineParser CLP;
    CLP.AddCommand({
        CLP_Command("version", "Show version information", [](const std::string& argument) {
            show_version(true);
            exit(0);
        }, "", typeid(void)),
        CLP_Command("verbosity,v", "Set logging verbosity level: " + LOG_INST.GetLogLevelNames(), [&verbosity](const std::string& argument) {
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
            LOG_INST.verbosity = verbosity;
     
            std::cout << "\nSetting Logger Verbosity to " << std::string(magic_enum::enum_name(verbosity))
                << " (" << static_cast<int>(verbosity) << ")"
				<< " argument=" << argument
                << "\n";
        }, "ERR", typeid(std::string)),
        CLP_Command("sendfile, f", "Specifies file name to send", [&SendFile, &sendMode](const std::string& argument) {
            SendFile = argument;
            sendMode = UdpSendMode::SEND_FILE;
        }, "", typeid(std::string)),
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "bUDPC.log", typeid(std::string)),
        CLP_Command("server_port, p", "Specifies UDP Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "8080", typeid(std::string)),
        CLP_Command("interactive, i", "Allows multiple Messages to be sent from client", [&interactiveMode](const std::string& argument) {
            interactiveMode = true;
        }, "", typeid(void)),
        CLP_Command("server_ip,s", "Specifies IP address of server", [&ServerIP](const std::string& argument) {
            ServerIP = argument;
        }, "127.0.0.1", typeid(std::string)),
        });

    LOG(LoggerVerbosity::DEBUG, show_version());
    LOG(LoggerVerbosity::DEBUG, "*** Setting DEFAULT command line arguments...***");
    CLP.SetDefaultValues();
    LOG(LoggerVerbosity::DEBUG, "\*** Parsing command line arguments...***");
    CLP.ProcessArguments(argc, argv);

    LOG_INST.SetLogFile(LogFile);
    LOG(verbosity, "Starting log\n");

    try {
        // 1. Every Asio program needs an io_context object
        boost::asio::io_context io_context;

        // 2. Resolve the remote hostname or IP address and port
        LOG(LoggerVerbosity::INFO, "Connecting to " + ServerIP + ":" + ServerPort);
        udp::resolver resolver(io_context);
        udp::endpoint receiver_endpoint = *resolver.resolve(udp::v4(), ServerIP, ServerPort).begin();

        // 3. Open the UDP socket
        udp::socket socket(io_context);
        socket.open(udp::v4());
        LOG(LoggerVerbosity::INFO, "Socket opened to " + ServerIP + ":" + ServerPort);

        if (sendMode == UdpSendMode::SEND_FILE) {
            LOG(LoggerVerbosity::INFO, "Sending file: " + SendFile);
            std::string message = "FILE_MODE: " + SendFile;
            socket.send_to(boost::asio::buffer(message), receiver_endpoint);

            FileTransferHeader fh;
			auto FTHS = fh.GetHeaderSizeBytes();

            std::vector<char> buffer(CHUNK_SIZE+ FTHS);
            size_t total_bytes_sent = 0;

            // Open file in binary mode
            std::ifstream file(SendFile, std::ios::binary);
            if (!file) {
                std::cerr << "Error: Cannot open file " << SendFile << "\n";
                return 1;
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

        } else {
            // 4. Send a message to the remote endpoint
            std::string message = "Hello from Synchronous bUDP Client!";
            socket.send_to(boost::asio::buffer(message), receiver_endpoint);
            std::cout << "Message sent cleanly." << std::endl;

            // 5. Prepare a buffer to receive the reply back
            std::array<char, 1024> recv_buf;
            udp::endpoint sender_endpoint;

            // This blocks until data arrives
            size_t len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);

            std::cout << "Received reply: ";
            std::cout.write(recv_buf.data(), len);
            std::cout << std::endl;
            if (interactiveMode) {
                std::string input;
                while (true) {
                    std::cout << "Enter message to send (or 'exit' to quit): ";
                    std::getline(std::cin, input);
                    if (input == "exit") {
                        break;
                    }
                    socket.send_to(boost::asio::buffer(input), receiver_endpoint);
                    std::cout << "Message sent cleanly." << std::endl;

                    std::array<char, 1024> recv_buf;
                    udp::endpoint sender_endpoint;

                    // This blocks until data arrives
                    size_t len = socket.receive_from(boost::asio::buffer(recv_buf), sender_endpoint);

                    std::cout << "Received reply: ";
                    std::cout.write(recv_buf.data(), len);
                    std::cout << std::endl;

                }
            }
            else {
                LOG(LoggerVerbosity::INFO, "Not in interactive mode, exiting after one message.");
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}

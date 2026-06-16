/*------------------------------------------------------------------
 * bUDP-Client.cpp
 * 
 * CBoost based UDP Client application to test the bUDP-Server
 *
 * June 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <iostream>
#include <string>
#include <array>
#include <boost/asio.hpp>

using boost::asio::ip::udp;

#include "commandLineParser.h"
#include "logger.h"
#include "watchdog.h"
#include "PacketHeader.h"

#define VERSION "0.1"

void show_version() {
    std::cout << "UDP Client (boost) Version: " << VERSION << "." << GIT_HASH << std::endl;
    std::cout << "Build Date: " << __DATE__ << std::endl;
    std::cout << "Build Time: " << __TIME__ << std::endl;
#ifdef _WIN32
    std::cout << "MSVC Version: " << _MSC_FULL_VER << std::endl;
#else
    std::cout << "Compiler: " << __VERSION__ << std::endl;
#endif
    std::cout << "Build Machine: " << BUILD_MACHINE << std::endl;
}

int main(int argc, char* argv[]) {
    LoggerVerbosity verbosity = LoggerVerbosity::CRITICAL;
	double WatchdogTimeout = 360;
    std::string LogFile;
    std::string ServerPort = "8080";
    std::string ServerIP = "127.0.0.1";

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
        CLP_Command("logfile, l", "Specifies Log file name", [&LogFile](const std::string& argument) {
            LogFile = argument;
        }, "bUDPC.log", typeid(std::string)),
        CLP_Command("server_port, p", "Specifies UDP Port number of server", [&ServerPort](const std::string& argument) {
            ServerPort = argument;
        }, "8080", typeid(std::string)),
        CLP_Command("server_ip, i", "Specifies IP address of server", [&ServerIP](const std::string& argument) {
            ServerIP = argument;
        }, "127.0.0.1", typeid(std::string)),
        });

    show_version();
    std::cout << "\n** Setting DEFAULT command line arguments...**\n";
    CLP.SetDefaultValues();
    std::cout << "\n** Parsing command line arguments...**\n";
    CLP.ProcessArguments(argc, argv);

    LOG_INST.SetLogFile(LogFile);
    LOG(verbosity, "Starting log");

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

    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}

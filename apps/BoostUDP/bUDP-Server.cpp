#include <iostream>
#include <string>
#include <set>
#include <memory>
#include <boost/asio.hpp>

using boost::asio::ip::udp;

class UdpServer {
public:
    // Bind to the given port on all available network interfaces
    UdpServer(boost::asio::io_context& io_context, short port)
        : socket_(io_context, udp::endpoint(udp::v4(), port)) {
        start_receive();
    }

private:
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
        
        // Register the client endpoint if it's new
        if (known_clients_.insert(remote_endpoint_).second) {
            std::cout << "[New Client Connected] " << remote_endpoint_ << std::endl;
        }

        std::cout << "[Received from " << remote_endpoint_ << "]: " << message << std::endl;

        // Echo the packet back asynchronously to the sender
        auto response_msg = std::make_shared<std::string>("Echo: " + message);
        socket_.async_send_to(
            boost::asio::buffer(*response_msg), remote_endpoint_,
            [response_msg](boost::system::error_code /*ec*/, std::size_t /*bytes*/) {
                // Shared pointer capture keeps data alive until transmission completes
            });
    }

    udp::socket socket_;
    udp::endpoint remote_endpoint_;
    std::array<char, 4096> recv_buffer_;
    std::set<udp::endpoint> known_clients_; // Active client registry
};

int main() {
    try {
        boost::asio::io_context io_context;
        UdpServer server(io_context, 8080);
        
        std::cout << "UDP Server running on port 8080..." << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}

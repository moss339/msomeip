#pragma once

#include "msomeip/types.h"
#include "msomeip/message/message.h"

#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <map>
#include <mutex>

namespace moss {
namespace msomeip {
namespace transport {

class TcpTransport {
public:
    using MessageReceivedCallback = std::function<void(MessagePtr&&, uint32_t connection_id)>;
    using ConnectionCallback = std::function<void(uint32_t connection_id, bool connected)>;

    TcpTransport();
    ~TcpTransport();

    // Non-copyable
    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;

    // Server mode - listen for connections
    bool listen(const std::string& local_address, uint16_t port);

    // Client mode - connect to server
    bool connect(const std::string& remote_address, uint16_t port, uint32_t& connection_id);
    void disconnect(uint32_t connection_id);

    // Start/stop
    void start();
    void stop();

    // Send message by connection ID
    bool send_to(uint32_t connection_id, const Message& message);
    bool send_to(uint32_t connection_id, const std::vector<uint8_t>& data);

    // Send message by endpoint (auto-connect if needed)
    bool send_to_endpoint(const std::string& address, uint16_t port, const Message& message);
    bool send_to_endpoint(const std::string& address, uint16_t port, const std::vector<uint8_t>& data);

    // Get or create connection for endpoint
    uint32_t get_or_create_connection(const std::string& address, uint16_t port);

    // Check if connection exists for endpoint
    bool has_connection(const std::string& address, uint16_t port) const;

    // Set callbacks
    void set_message_received_callback(MessageReceivedCallback callback);
    void set_connection_callback(ConnectionCallback callback);

    uint16_t get_local_port() const { return local_port_; }

private:
    struct Connection {
        int socket_fd = -1;
        std::string remote_address;
        uint16_t remote_port = 0;
        std::thread receive_thread;
        std::atomic<bool> running{false};
        std::vector<uint8_t> receive_buffer;
    };

    void accept_thread();
    void connection_receive_thread(uint32_t connection_id);
    void close_connection(uint32_t connection_id);

    int listen_socket_ = -1;
    std::string local_address_;
    uint16_t local_port_ = 0;

    std::atomic<bool> running_{false};
    std::thread accept_thread_;

    std::atomic<uint32_t> next_connection_id_{1};
    std::unordered_map<uint32_t, std::unique_ptr<Connection>> connections_;
    std::mutex connections_mutex_;

    // Endpoint to connection ID mapping for outgoing connections
    std::map<std::pair<std::string, uint16_t>, uint32_t> endpoint_to_connection_;
    mutable std::mutex endpoint_map_mutex_;

    MessageReceivedCallback message_callback_;
    ConnectionCallback connection_callback_;

    static constexpr size_t MAX_MESSAGE_SIZE = 4096;
    static constexpr size_t MAX_TCP_SIZE = 4095; // SOME/IP TP limit
};

} // namespace transport
}  // namespace msomeip
}  // namespace moss

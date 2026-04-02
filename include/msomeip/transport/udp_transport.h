#pragma once

#include "msomeip/types.h"
#include "msomeip/message/message.h"

#include <functional>
#include <thread>
#include <atomic>
#include <array>

namespace moss {
namespace msomeip {
namespace transport {

class UdpTransport {
public:
    using MessageReceivedCallback = std::function<void(MessagePtr&&, const std::string&, uint16_t)>;

    UdpTransport();
    ~UdpTransport();

    // Non-copyable
    UdpTransport(const UdpTransport&) = delete;
    UdpTransport& operator=(const UdpTransport&) = delete;

    // Initialize the transport
    bool init(const std::string& local_address, uint16_t port);

    // Join multicast group for SD
    bool join_multicast(const std::string& multicast_address, const std::string& interface_address);

    // Start/stop receiving
    void start();
    void stop();

    // Send message
    bool send_to(const Message& message, const std::string& address, uint16_t port);
    bool send_to(const std::vector<uint8_t>& data, const std::string& address, uint16_t port);

    // Set message received callback
    void set_message_received_callback(MessageReceivedCallback callback);

    uint16_t get_local_port() const { return local_port_; }
    std::string get_local_address() const { return local_address_; }

private:
    void receive_thread();

    int socket_fd_ = -1;
    std::string local_address_;
    uint16_t local_port_ = 0;

    std::atomic<bool> running_{false};
    std::thread receive_thread_;

    MessageReceivedCallback message_callback_;

    static constexpr size_t MAX_MESSAGE_SIZE = 65535;
    std::array<uint8_t, MAX_MESSAGE_SIZE> receive_buffer_;
};

} // namespace transport
}  // namespace msomeip
}  // namespace moss

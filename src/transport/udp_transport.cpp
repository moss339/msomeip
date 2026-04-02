#include "msomeip/transport/udp_transport.h"
#include "msomeip/message/message.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

namespace moss {
namespace msomeip {
namespace transport {

UdpTransport::UdpTransport() = default;

UdpTransport::~UdpTransport() {
    stop();
    if (socket_fd_ >= 0) {
        close(socket_fd_);
    }
}

bool UdpTransport::init(const std::string& local_address, uint16_t port) {
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }

    // Allow address reuse
    int reuse = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, local_address.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // Get the actual bound port
    sockaddr_in bound_addr{};
    socklen_t addr_len = sizeof(bound_addr);
    if (getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&bound_addr), &addr_len) == 0) {
        local_port_ = ntohs(bound_addr.sin_port);
    } else {
        local_port_ = port;
    }

    local_address_ = local_address;

    return true;
}

bool UdpTransport::join_multicast(const std::string& multicast_address,
                                   const std::string& interface_address) {
    if (socket_fd_ < 0) return false;

    ip_mreq mreq{};
    if (inet_pton(AF_INET, multicast_address.c_str(), &mreq.imr_multiaddr) != 1) {
        return false;
    }
    if (inet_pton(AF_INET, interface_address.c_str(), &mreq.imr_interface) != 1) {
        mreq.imr_interface.s_addr = INADDR_ANY;
    }

    if (setsockopt(socket_fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        return false;
    }

    return true;
}

void UdpTransport::start() {
    if (running_.exchange(true)) return;

    receive_thread_ = std::thread(&UdpTransport::receive_thread, this);
}

void UdpTransport::stop() {
    running_ = false;

    if (socket_fd_ >= 0) {
        shutdown(socket_fd_, SHUT_RDWR);
    }

    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

bool UdpTransport::send_to(const Message& message, const std::string& address, uint16_t port) {
    auto data = message.serialize();
    return send_to(data, address, port);
}

bool UdpTransport::send_to(const std::vector<uint8_t>& data,
                           const std::string& address,
                           uint16_t port) {
    if (socket_fd_ < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        return false;
    }

    ssize_t sent = sendto(socket_fd_, data.data(), data.size(), 0,
                          reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return sent == static_cast<ssize_t>(data.size());
}

void UdpTransport::set_message_received_callback(MessageReceivedCallback callback) {
    message_callback_ = std::move(callback);
}

void UdpTransport::receive_thread() {
    while (running_) {
        sockaddr_in sender_addr{};
        socklen_t addr_len = sizeof(sender_addr);

        ssize_t received = recvfrom(socket_fd_,
                                    receive_buffer_.data(),
                                    receive_buffer_.size(),
                                    0,
                                    reinterpret_cast<sockaddr*>(&sender_addr),
                                    &addr_len);

        if (received < 0) {
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }

        if (received > 0 && message_callback_) {
            auto msg = Message::deserialize(receive_buffer_.data(), received);
            if (msg) {
                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sender_addr.sin_addr, addr_str, sizeof(addr_str));
                message_callback_(std::move(*msg), addr_str, ntohs(sender_addr.sin_port));
            }
        }
    }
}

} // namespace transport
}  // namespace msomeip
}  // namespace moss

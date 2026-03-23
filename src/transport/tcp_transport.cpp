#include "msomeip/transport/tcp_transport.h"
#include "msomeip/message/message.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

namespace msomeip {
namespace transport {

TcpTransport::TcpTransport() = default;

TcpTransport::~TcpTransport() {
    stop();
    for (auto& [id, conn] : connections_) {
        if (conn && conn->socket_fd >= 0) {
            close(conn->socket_fd);
        }
    }
    if (listen_socket_ >= 0) {
        close(listen_socket_);
    }
}

bool TcpTransport::listen(const std::string& local_address, uint16_t port) {
    listen_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket_ < 0) {
        return false;
    }

    // Allow address reuse
    int reuse = 1;
    if (setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        close(listen_socket_);
        listen_socket_ = -1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, local_address.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listen_socket_);
        listen_socket_ = -1;
        return false;
    }

    if (::listen(listen_socket_, 10) < 0) {
        close(listen_socket_);
        listen_socket_ = -1;
        return false;
    }

    // Get the actual bound port
    sockaddr_in bound_addr{};
    socklen_t addr_len = sizeof(bound_addr);
    if (getsockname(listen_socket_, reinterpret_cast<sockaddr*>(&bound_addr), &addr_len) == 0) {
        local_port_ = ntohs(bound_addr.sin_port);
    } else {
        local_port_ = port;
    }

    local_address_ = local_address;

    return true;
}

bool TcpTransport::connect(const std::string& remote_address, uint16_t port, uint32_t& connection_id) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, remote_address.c_str(), &addr.sin_addr) != 1) {
        close(sock);
        return false;
    }

    if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    connection_id = next_connection_id_++;

    auto conn = std::make_unique<Connection>();
    conn->socket_fd = sock;
    conn->remote_address = remote_address;
    conn->remote_port = port;
    conn->running = true;
    conn->receive_buffer.resize(MAX_MESSAGE_SIZE);

    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_[connection_id] = std::move(conn);
    }

    connections_[connection_id]->receive_thread = std::thread(
        &TcpTransport::connection_receive_thread, this, connection_id);

    if (connection_callback_) {
        connection_callback_(connection_id, true);
    }

    return true;
}

void TcpTransport::disconnect(uint32_t connection_id) {
    close_connection(connection_id);
}

void TcpTransport::start() {
    if (running_.exchange(true)) return;

    if (listen_socket_ >= 0) {
        accept_thread_ = std::thread(&TcpTransport::accept_thread, this);
    }
}

void TcpTransport::stop() {
    running_ = false;

    if (listen_socket_ >= 0) {
        shutdown(listen_socket_, SHUT_RDWR);
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    // Close all connections
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [id, conn] : connections_) {
        if (conn) {
            conn->running = false;
            if (conn->socket_fd >= 0) {
                shutdown(conn->socket_fd, SHUT_RDWR);
            }
            if (conn->receive_thread.joinable()) {
                conn->receive_thread.join();
            }
        }
    }
    connections_.clear();
}

bool TcpTransport::send_to(uint32_t connection_id, const Message& message) {
    auto data = message.serialize();
    return send_to(connection_id, data);
}

bool TcpTransport::send_to(uint32_t connection_id, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(connection_id);
    if (it == connections_.end() || !it->second) {
        return false;
    }

    int sock = it->second->socket_fd;

    // Send length prefix (4 bytes) for TCP framing
    uint32_t length = htonl(static_cast<uint32_t>(data.size()));
    if (send(sock, &length, sizeof(length), MSG_NOSIGNAL) != sizeof(length)) {
        return false;
    }

    ssize_t sent = send(sock, data.data(), data.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(data.size());
}

void TcpTransport::set_message_received_callback(MessageReceivedCallback callback) {
    message_callback_ = std::move(callback);
}

void TcpTransport::set_connection_callback(ConnectionCallback callback) {
    connection_callback_ = std::move(callback);
}

void TcpTransport::accept_thread() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_sock = accept(listen_socket_,
                                  reinterpret_cast<sockaddr*>(&client_addr),
                                  &addr_len);

        if (client_sock < 0) {
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }

        uint32_t connection_id = next_connection_id_++;

        auto conn = std::make_unique<Connection>();
        conn->socket_fd = client_sock;
        conn->remote_address = inet_ntoa(client_addr.sin_addr);
        conn->remote_port = ntohs(client_addr.sin_port);
        conn->running = true;
        conn->receive_buffer.resize(MAX_MESSAGE_SIZE);

        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[connection_id] = std::move(conn);
        }

        connections_[connection_id]->receive_thread = std::thread(
            &TcpTransport::connection_receive_thread, this, connection_id);

        if (connection_callback_) {
            connection_callback_(connection_id, true);
        }
    }
}

void TcpTransport::connection_receive_thread(uint32_t connection_id) {
    Connection* conn = nullptr;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.find(connection_id);
        if (it != connections_.end()) {
            conn = it->second.get();
        }
    }

    if (!conn) return;

    while (conn->running && running_) {
        // Read length prefix
        uint32_t msg_length;
        ssize_t received = recv(conn->socket_fd, &msg_length, sizeof(msg_length), MSG_WAITALL);
        if (received != sizeof(msg_length)) {
            break;
        }
        msg_length = ntohl(msg_length);

        if (msg_length > MAX_MESSAGE_SIZE) {
            break;
        }

        // Read message data
        size_t total_received = 0;
        while (total_received < msg_length) {
            received = recv(conn->socket_fd,
                           conn->receive_buffer.data() + total_received,
                           msg_length - total_received,
                           0);
            if (received <= 0) {
                conn->running = false;
                break;
            }
            total_received += received;
        }

        if (conn->running && total_received == msg_length && message_callback_) {
            auto msg = Message::deserialize(conn->receive_buffer.data(), total_received);
            if (msg) {
                message_callback_(std::move(*msg), connection_id);
            }
        }
    }

    if (connection_callback_) {
        connection_callback_(connection_id, false);
    }

    close_connection(connection_id);
}

void TcpTransport::close_connection(uint32_t connection_id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(connection_id);
    if (it != connections_.end() && it->second) {
        it->second->running = false;
        if (it->second->socket_fd >= 0) {
            close(it->second->socket_fd);
            it->second->socket_fd = -1;
        }
        if (it->second->receive_thread.joinable()) {
            it->second->receive_thread.detach();
        }
        connections_.erase(it);
    }
}

} // namespace transport
} // namespace msomeip
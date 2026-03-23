#pragma once

#include "msomeip/types.h"
#include "msomeip/message/message.h"
#include "msomeip/sd/service_discovery.h"
#include "msomeip/transport/udp_transport.h"
#include "msomeip/transport/tcp_transport.h"

#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <mutex>

namespace msomeip {

// Runtime is the main entry point for the SOME/IP stack
class Runtime : public std::enable_shared_from_this<Runtime> {
public:
    struct Config {
        std::string name;
        std::string address = "127.0.0.1";
        uint16_t udp_port = 0;  // 0 = auto-assign
        uint16_t tcp_port = 0;  // 0 = auto-assign
        bool enable_sd = true;
        sd::Config sd_config;
    };

    // Singleton access
    static std::shared_ptr<Runtime> get();
    static std::shared_ptr<Runtime> create(const Config& config);
    static void set_default(const Config& config);

    ~Runtime();

    // Non-copyable
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    // Initialize the runtime
    bool init();

    // Start/stop the runtime
    void start();
    void stop();

    // Create an application
    std::shared_ptr<class Application> create_application(const std::string& name);

    // Send message (internal use)
    bool send_message(const Message& message, const std::string& address, uint16_t port, bool reliable);

    // Register message handler for a service
    void register_service_handler(ServiceId service, InstanceId instance, MessageHandler handler);
    void unregister_service_handler(ServiceId service, InstanceId instance);

    // Service Discovery access
    sd::ServiceDiscovery& get_service_discovery() { return *service_discovery_; }

    // Get local endpoints
    std::string get_local_address() const { return config_.address; }
    uint16_t get_local_udp_port() const { return udp_transport_->get_local_port(); }
    uint16_t get_local_tcp_port() const { return tcp_transport_->get_local_port(); }

    // Get client ID
    ClientId get_client_id() const { return client_id_; }

private:
    explicit Runtime(Config config);

    void on_udp_message_received(MessagePtr&& message, const std::string& address, uint16_t port);
    void on_tcp_message_received(MessagePtr&& message, uint32_t connection_id);
    void on_tcp_connection(uint32_t connection_id, bool connected);

    Config config_;
    ClientId client_id_;

    std::unique_ptr<transport::UdpTransport> udp_transport_;
    std::unique_ptr<transport::TcpTransport> tcp_transport_;
    std::unique_ptr<sd::ServiceDiscovery> service_discovery_;

    // Message handlers
    std::unordered_map<ServiceIdTuple, MessageHandler, ServiceIdTupleHash> service_handlers_;
    mutable std::mutex handlers_mutex_;

    // Session counter
    std::atomic<SessionId> session_counter_{1};

    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};

    static std::weak_ptr<Runtime> default_runtime_;
    static std::mutex runtime_mutex_;
};

} // namespace msomeip

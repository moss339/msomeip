#include "msomeip/runtime.h"
#include "msomeip/application.h"

#include <iostream>
#include <random>

namespace moss {
namespace msomeip {

// Static members
std::weak_ptr<Runtime> Runtime::default_runtime_;
std::mutex Runtime::runtime_mutex_;

std::shared_ptr<Runtime> Runtime::get() {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    auto runtime = default_runtime_.lock();
    if (!runtime) {
        runtime = create(Config{});
        default_runtime_ = runtime;
    }
    return runtime;
}

std::shared_ptr<Runtime> Runtime::create(const Config& config) {
    auto runtime = std::shared_ptr<Runtime>(new Runtime(config));
    if (!runtime->init()) {
        return nullptr;
    }
    return runtime;
}

void Runtime::set_default(const Config& config) {
    std::lock_guard<std::mutex> lock(runtime_mutex_);
    auto runtime = create(config);
    if (runtime) {
        default_runtime_ = runtime;
    }
}

Runtime::Runtime(Config config) : config_(std::move(config)) {
    // Generate a unique client ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dis(0x0001, 0x7FFF);
    client_id_ = dis(gen);
}

Runtime::~Runtime() {
    stop();
}

bool Runtime::init() {
    if (initialized_.exchange(true)) {
        return true;
    }

    // Initialize UDP transport
    udp_transport_ = std::make_unique<transport::UdpTransport>();
    if (!udp_transport_->init(config_.address, config_.udp_port)) {
        std::cerr << "Failed to initialize UDP transport" << std::endl;
        return false;
    }

    // Initialize TCP transport
    tcp_transport_ = std::make_unique<transport::TcpTransport>();
    if (!tcp_transport_->listen(config_.address, config_.tcp_port)) {
        std::cerr << "Failed to initialize TCP transport" << std::endl;
        return false;
    }

    // Set up message callbacks
    udp_transport_->set_message_received_callback(
        [this](MessagePtr&& msg, const std::string& addr, uint16_t port) {
            on_udp_message_received(std::move(msg), addr, port);
        });

    tcp_transport_->set_message_received_callback(
        [this](MessagePtr&& msg, uint32_t conn_id) {
            on_tcp_message_received(std::move(msg), conn_id);
        });

    tcp_transport_->set_connection_callback(
        [this](uint32_t conn_id, bool connected) {
            on_tcp_connection(conn_id, connected);
        });

    // Initialize Service Discovery
    if (config_.enable_sd) {
        service_discovery_ = std::make_unique<sd::ServiceDiscovery>(config_.sd_config);
        service_discovery_->init(config_.address,
                                  udp_transport_->get_local_port(),
                                  tcp_transport_->get_local_port());

        // Join multicast group
        udp_transport_->join_multicast(config_.sd_config.multicast_address, config_.address);
    }

    return true;
}

void Runtime::start() {
    if (running_.exchange(true)) {
        return;
    }

    udp_transport_->start();
    tcp_transport_->start();

    if (service_discovery_) {
        service_discovery_->start();
    }
}

void Runtime::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (service_discovery_) {
        service_discovery_->stop();
    }

    udp_transport_->stop();
    tcp_transport_->stop();
}

std::shared_ptr<Application> Runtime::create_application(const std::string& name) {
    auto app = std::make_shared<Application>(shared_from_this(), name);
    return app;
}

bool Runtime::send_message(const Message& message, const std::string& address,
                           uint16_t port, bool reliable) {
    if (reliable) {
        // For reliable transport, we need an existing connection or create one
        // This is simplified - in real implementation we'd need connection management
        return false;
    } else {
        return udp_transport_->send_to(message, address, port);
    }
}

void Runtime::register_service_handler(ServiceId service, InstanceId instance, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    service_handlers_[{service, instance}] = std::move(handler);
}

void Runtime::unregister_service_handler(ServiceId service, InstanceId instance) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    service_handlers_.erase({service, instance});
}

void Runtime::on_udp_message_received(MessagePtr&& message, const std::string& address, uint16_t port) {
    // Handle SD messages
    if (message->get_service_id() == SOMEIP_SD_SERVICE_ID &&
        message->get_method_id() == SOMEIP_SD_METHOD_ID) {
        if (service_discovery_) {
            service_discovery_->process_sd_message(*message);
        }
        return;
    }

    // Route to service handler
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = service_handlers_.find({message->get_service_id(), message->get_instance_id()});
    if (it != service_handlers_.end() && it->second) {
        it->second(std::move(message));
    }
}

void Runtime::on_tcp_message_received(MessagePtr&& message, uint32_t connection_id) {
    (void)connection_id;

    // Route to service handler
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    auto it = service_handlers_.find({message->get_service_id(), message->get_instance_id()});
    if (it != service_handlers_.end() && it->second) {
        it->second(std::move(message));
    }
}

void Runtime::on_tcp_connection(uint32_t connection_id, bool connected) {
    (void)connection_id;
    (void)connected;
    // Handle connection state changes if needed
}

}  // namespace msomeip
}  // namespace moss

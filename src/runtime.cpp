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

    // Initialize TP reassembler
    tp_reassembler_ = std::make_unique<tp::TpReassembler>(std::chrono::milliseconds(5000));

    // Initialize ShmAgent if configured
    if (!config_.shm_name.empty()) {
        shm_agent_ = std::make_unique<ShmAgent>();
        shm_agent_->set_client_id(client_id_);
        if (!shm_agent_->connect(config_.shm_name)) {
            std::cerr << "Warning: Failed to connect to shared memory routing: " << config_.shm_name << std::endl;
            // Non-fatal: continue without shared memory routing
        }
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
    const auto& payload = message.get_payload();
    size_t payload_size = payload ? payload->size() : 0;

    // Determine max segment size based on transport
    size_t max_segment_size = reliable ? tp::TCP_SEGMENT_SIZE : tp::UDP_SEGMENT_SIZE;

    // Check if TP segmentation is needed
    if (tp::TpSegmenter::needs_segmentation(payload_size, max_segment_size)) {
        // Segment the message
        auto segments = tp::TpSegmenter::segment(message, max_segment_size);
        if (segments.empty()) {
            return false;
        }

        // Send all segments
        bool all_sent = true;
        for (const auto& segment : segments) {
            bool sent;
            if (reliable) {
                sent = tcp_transport_->send_to_endpoint(address, port, *segment);
            } else {
                sent = udp_transport_->send_to(*segment, address, port);
            }
            if (!sent) {
                all_sent = false;
                break;
            }
        }
        return all_sent;
    }

    // No segmentation needed, send normally
    if (reliable) {
        return tcp_transport_->send_to_endpoint(address, port, message);
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
    // Check if this is a TP message that needs reassembly
    if (message->is_tp_message()) {
        uint32_t request_id = message->get_request_id();
        if (tp_reassembler_->add_segment(std::move(message))) {
            // Reassembly complete
            auto reassembled = tp_reassembler_->reassemble(request_id);
            if (!reassembled) {
                return;
            }
            message = std::move(reassembled);
        } else {
            return;  // Waiting for more segments
        }
    }

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

    // Check if this is a TP message that needs reassembly
    if (message->is_tp_message()) {
        uint32_t request_id = message->get_request_id();
        if (tp_reassembler_->add_segment(std::move(message))) {
            // Reassembly complete
            auto reassembled = tp_reassembler_->reassemble(request_id);
            if (!reassembled) {
                return;
            }
            message = std::move(reassembled);
        } else {
            return;  // Waiting for more segments
        }
    }

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

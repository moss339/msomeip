#pragma once

#include "msomeip/types.h"
#include "msomeip/message/message.h"

#include <unordered_map>
#include <mutex>
#include <future>

namespace msomeip {

class Runtime;

// Pending request tracking
struct PendingRequest {
    std::promise<MessagePtr> promise;
    std::chrono::steady_clock::time_point timestamp;
    std::chrono::milliseconds timeout;
};

// Application represents a client or server application
class Application : public std::enable_shared_from_this<Application> {
public:
    Application(std::shared_ptr<Runtime> runtime, std::string name);
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Initialize
    void init();

    // Start/stop
    void start();
    void stop();

    // Get name
    const std::string& get_name() const { return name_; }

    // Get runtime
    std::shared_ptr<Runtime> get_runtime() const { return runtime_; }

    // === Client-side API ===

    // Send a request and wait for response
    std::future<MessagePtr> send_request(
        ServiceId service,
        InstanceId instance,
        MethodId method,
        const PayloadData& payload,
        bool reliable = false,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Send a request without waiting for response (fire-and-forget)
    bool send_request_no_return(
        ServiceId service,
        InstanceId instance,
        MethodId method,
        const PayloadData& payload,
        bool reliable = false);

    // Subscribe to an event
    bool subscribe_event(
        ServiceId service,
        InstanceId instance,
        EventgroupId eventgroup,
        EventId event,
        MajorVersion major_version);

    void unsubscribe_event(
        ServiceId service,
        InstanceId instance,
        EventgroupId eventgroup);

    // Set event handler
    void set_event_handler(EventId event, MessageHandler handler);

    // === Server-side API ===

    // Offer a service
    void offer_service(const ServiceConfig& config);
    void stop_offer_service(ServiceId service, InstanceId instance);

    // Register method handler
    void register_method_handler(MethodId method, MessageHandler handler);
    void unregister_method_handler(MethodId method);

    // Send a response
    void send_response(MessagePtr& request, const PayloadData& payload);
    void send_error_response(MessagePtr& request, ReturnCode code);

    // Offer an event
    void offer_event(const EventConfig& config);
    void stop_offer_event(ServiceId service, InstanceId instance, EventId event);

    // Notify event (send to all subscribers)
    void notify_event(EventId event, const PayloadData& payload);

    // === Field API ===

    // Field is a special case of event with getter/setter
    void offer_field(const EventConfig& config,
                     std::function<PayloadData()> getter,
                     std::function<bool(const PayloadData&)> setter);

    // Set field value (triggers notification)
    void set_field_value(EventId field, const PayloadData& value);

    // === Service Discovery callbacks ===
    void on_service_available(ServiceId service, InstanceId instance, bool available);
    void set_availability_handler(AvailabilityHandler handler);

    // === Message handling ===
    void handle_message(MessagePtr&& message);

private:
    void cleanup_expired_requests();

    std::shared_ptr<Runtime> runtime_;
    std::string name_;

    SessionId session_id_ = 1;
    mutable std::mutex session_mutex_;

    // Pending requests
    std::unordered_map<RequestId, std::shared_ptr<PendingRequest>> pending_requests_;
    mutable std::mutex requests_mutex_;

    // Method handlers (for servers)
    std::unordered_map<MethodId, MessageHandler> method_handlers_;
    mutable std::mutex methods_mutex_;

    // Event handlers (for clients)
    std::unordered_map<EventId, MessageHandler> event_handlers_;
    mutable std::mutex events_mutex_;

    // Availability handler
    AvailabilityHandler availability_handler_;

    // Field management
    struct FieldInfo {
        PayloadData value;
        std::function<PayloadData()> getter;
        std::function<bool(const PayloadData&)> setter;
        std::vector<Endpoint> subscribers;
    };
    std::unordered_map<EventId, FieldInfo> fields_;
    mutable std::mutex fields_mutex_;

    // Event configuration
    std::unordered_map<EventId, EventConfig> event_configs_;
    mutable std::mutex event_configs_mutex_;

    std::atomic<bool> running_{false};
};

} // namespace msomeip

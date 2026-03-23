#pragma once

#include "msomeip/types.h"
#include "msomeip/application.h"

#include <functional>
#include <map>

namespace msomeip {

// Base class for service skeletons (servers)
class ServiceSkeleton : public std::enable_shared_from_this<ServiceSkeleton> {
public:
    ServiceSkeleton(std::shared_ptr<Application> app,
                    ServiceId service_id,
                    InstanceId instance_id);
    virtual ~ServiceSkeleton();

    // Initialize - register handlers
    virtual void init();

    // Offer/stop the service
    void offer();
    void stop_offer();

    // Event notification
    void notify_event(EventId event, const PayloadData& payload);

    // Field helpers
    void notify_field(FieldId field, const PayloadData& payload);

protected:
    // Register a method handler
    void register_method(MethodId method,
                         std::function<PayloadData(const PayloadData&)> handler);

    // Register a getter for a field
    void register_field_getter(FieldId field,
                                std::function<PayloadData()> getter);

    // Register a setter for a field
    void register_field_setter(FieldId field,
                                std::function<bool(const PayloadData&)> setter);

    // Register an event
    void register_event(EventId event, EventgroupId eventgroup, bool is_reliable = false);

    // Register a field (getter + setter + notifier)
    void register_field(FieldId field,
                        EventgroupId eventgroup,
                        std::function<PayloadData()> getter,
                        std::function<bool(const PayloadData&)> setter,
                        bool is_reliable = false);

    std::shared_ptr<Application> get_app() const { return app_; }

    ServiceId get_service_id() const { return service_id_; }
    InstanceId get_instance_id() const { return instance_id_; }

private:
    void on_message_received(MessagePtr&& message);
    void handle_method_call(MessagePtr&& message);
    void handle_field_get(MessagePtr&& message);
    void handle_field_set(MessagePtr&& message);

    std::shared_ptr<Application> app_;
    ServiceId service_id_;
    InstanceId instance_id_;

    std::map<MethodId, std::function<PayloadData(const PayloadData&)>> method_handlers_;
    std::map<FieldId, std::function<PayloadData()>> field_getters_;
    std::map<FieldId, std::function<bool(const PayloadData&)>> field_setters_;
    std::map<FieldId, EventConfig> fields_;
    std::map<EventId, EventConfig> events_;

    mutable std::mutex mutex_;
};

// Base class for service proxies (clients)
class ServiceProxy : public std::enable_shared_from_this<ServiceProxy> {
public:
    ServiceProxy(std::shared_ptr<Application> app,
                 ServiceId service_id,
                 InstanceId instance_id);
    virtual ~ServiceProxy();

    // Initialize - setup handlers
    virtual void init();

    // Wait for service availability
    bool is_available() const { return available_; }

    // Set availability handler
    void set_availability_handler(AvailabilityHandler handler);

    // Call a method synchronously
    std::optional<PayloadData> call_method(MethodId method,
                                           const PayloadData& payload,
                                           std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Call a method asynchronously
    std::future<std::optional<PayloadData>> call_method_async(
        MethodId method,
        const PayloadData& payload,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Call a method without return
    bool call_method_no_return(MethodId method, const PayloadData& payload);

    // Subscribe to an event
    bool subscribe_event(EventId event,
                         EventgroupId eventgroup,
                         MessageHandler handler);

    void unsubscribe_event(EventId event, EventgroupId eventgroup);

    // Get a field value
    std::optional<PayloadData> get_field(FieldId field,
                                         std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Set a field value
    bool set_field(FieldId field, const PayloadData& payload);

protected:
    void on_service_available(ServiceId service, InstanceId instance, bool available);
    void on_event_received(MessagePtr&& message);

    std::shared_ptr<Application> get_app() const { return app_; }

    ServiceId get_service_id() const { return service_id_; }
    InstanceId get_instance_id() const { return instance_id_; }

private:
    std::shared_ptr<Application> app_;
    ServiceId service_id_;
    InstanceId instance_id_;
    std::atomic<bool> available_{false};

    std::map<EventId, std::pair<EventgroupId, MessageHandler>> event_handlers_;
    mutable std::mutex events_mutex_;

    AvailabilityHandler availability_handler_;
};

} // namespace msomeip

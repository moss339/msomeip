#include "msomeip/service_base.h"

namespace msomeip {

// ==================== ServiceSkeleton ====================

ServiceSkeleton::ServiceSkeleton(std::shared_ptr<Application> app,
                                  ServiceId service_id,
                                  InstanceId instance_id)
    : app_(std::move(app))
    , service_id_(service_id)
    , instance_id_(instance_id) {}

ServiceSkeleton::~ServiceSkeleton() {
    stop_offer();
}

void ServiceSkeleton::init() {
    // Register message handler for this service
    if (app_) {
        app_->register_method_handler(0,
            [this](MessagePtr&& msg) { on_message_received(std::move(msg)); });
    }
}

void ServiceSkeleton::offer() {
    if (!app_) return;

    // Build service configuration
    ServiceConfig config;
    config.service_id = service_id_;
    config.instance_id = instance_id_;
    config.major_version = 1;
    config.minor_version = 0;
    config.unreliable = true;
    config.reliable = false;

    app_->offer_service(config);

    // Offer all registered events
    for (const auto& [event, cfg] : events_) {
        app_->offer_event(cfg);
    }

    // Offer all registered fields
    for (const auto& [field, cfg] : fields_) {
        app_->offer_event(cfg);
    }
}

void ServiceSkeleton::stop_offer() {
    if (!app_) return;

    app_->stop_offer_service(service_id_, instance_id_);

    for (const auto& [event, cfg] : events_) {
        app_->stop_offer_event(service_id_, instance_id_, event);
    }

    for (const auto& [field, cfg] : fields_) {
        app_->stop_offer_event(service_id_, instance_id_, field);
    }
}

void ServiceSkeleton::notify_event(EventId event, const PayloadData& payload) {
    if (!app_) return;
    app_->notify_event(event, payload);
}

void ServiceSkeleton::notify_field(FieldId field, const PayloadData& payload) {
    notify_event(field, payload);
}

void ServiceSkeleton::register_method(MethodId method,
                                      std::function<PayloadData(const PayloadData&)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    method_handlers_[method] = std::move(handler);
}

void ServiceSkeleton::register_field_getter(FieldId field,
                                            std::function<PayloadData()> getter) {
    std::lock_guard<std::mutex> lock(mutex_);
    field_getters_[field] = std::move(getter);
}

void ServiceSkeleton::register_field_setter(FieldId field,
                                            std::function<bool(const PayloadData&)> setter) {
    std::lock_guard<std::mutex> lock(mutex_);
    field_setters_[field] = std::move(setter);
}

void ServiceSkeleton::register_event(EventId event, EventgroupId eventgroup, bool is_reliable) {
    EventConfig config;
    config.service_id = service_id_;
    config.instance_id = instance_id_;
    config.event_id = event;
    config.eventgroup_id = eventgroup;
    config.is_reliable = is_reliable;

    std::lock_guard<std::mutex> lock(mutex_);
    events_[event] = config;
}

void ServiceSkeleton::register_field(FieldId field,
                                      EventgroupId eventgroup,
                                      std::function<PayloadData()> getter,
                                      std::function<bool(const PayloadData&)> setter,
                                      bool is_reliable) {
    register_field_getter(field, std::move(getter));
    register_field_setter(field, std::move(setter));

    EventConfig config;
    config.service_id = service_id_;
    config.instance_id = instance_id_;
    config.event_id = field;
    config.eventgroup_id = eventgroup;
    config.is_field = true;
    config.is_reliable = is_reliable;

    std::lock_guard<std::mutex> lock(mutex_);
    fields_[field] = config;
}

void ServiceSkeleton::on_message_received(MessagePtr&& message) {
    if (!message) return;

    if (message->is_request()) {
        handle_method_call(std::move(message));
    } else if (message->is_event()) {
        // Field getter/setter are encoded in special method IDs
        MethodId method = message->get_method_id();
        if ((method & 0x7F00) == 0x0100) {
            handle_field_get(std::move(message));
        } else if ((method & 0x7F00) == 0x0200) {
            handle_field_set(std::move(message));
        }
    }
}

void ServiceSkeleton::handle_method_call(MessagePtr&& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = method_handlers_.find(message->get_method_id());
    if (it != method_handlers_.end()) {
        PayloadData payload;
        if (message->get_payload()) {
            payload = message->get_payload()->get_data();
        }

        auto result = it->second(payload);

        if (app_) {
            app_->send_response(message, result);
        }
    } else {
        if (app_) {
            app_->send_error_response(message, ReturnCode::E_UNKNOWN_METHOD);
        }
    }
}

void ServiceSkeleton::handle_field_get(MessagePtr&& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    FieldId field = message->get_method_id() & 0x00FF;
    auto it = field_getters_.find(field);
    if (it != field_getters_.end()) {
        auto value = it->second();
        if (app_) {
            app_->send_response(message, value);
        }
    } else {
        if (app_) {
            app_->send_error_response(message, ReturnCode::E_UNKNOWN_METHOD);
        }
    }
}

void ServiceSkeleton::handle_field_set(MessagePtr&& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    FieldId field = message->get_method_id() & 0x00FF;
    auto it = field_setters_.find(field);
    if (it != field_setters_.end()) {
        PayloadData payload;
        if (message->get_payload()) {
            payload = message->get_payload()->get_data();
        }

        bool success = it->second(payload);
        if (success) {
            // Notify field change
            if (app_) {
                app_->send_response(message, {});
            }
            notify_field(field, payload);
        } else {
            if (app_) {
                app_->send_error_response(message, ReturnCode::E_NOT_OK);
            }
        }
    } else {
        if (app_) {
            app_->send_error_response(message, ReturnCode::E_UNKNOWN_METHOD);
        }
    }
}

// ==================== ServiceProxy ====================

ServiceProxy::ServiceProxy(std::shared_ptr<Application> app,
                            ServiceId service_id,
                            InstanceId instance_id)
    : app_(std::move(app))
    , service_id_(service_id)
    , instance_id_(instance_id) {}

ServiceProxy::~ServiceProxy() = default;

void ServiceProxy::init() {
    if (!app_) return;

    // Setup availability handler
    app_->set_availability_handler(
        [this](ServiceId service, InstanceId instance, bool available) {
            on_service_available(service, instance, available);
        });

    // Find the service
    app_->init();
}

void ServiceProxy::set_availability_handler(AvailabilityHandler handler) {
    availability_handler_ = std::move(handler);
}

std::optional<PayloadData> ServiceProxy::call_method(MethodId method,
                                                      const PayloadData& payload,
                                                      std::chrono::milliseconds timeout) {
    if (!app_ || !available_) {
        return std::nullopt;
    }

    auto future = app_->send_request(service_id_, instance_id_, method,
                                     payload, false, timeout);

    if (future.wait_for(timeout) == std::future_status::timeout) {
        return std::nullopt;
    }

    auto response = future.get();
    if (response && response->get_payload()) {
        return response->get_payload()->get_data();
    }

    return std::nullopt;
}

std::future<std::optional<PayloadData>> ServiceProxy::call_method_async(
    MethodId method,
    const PayloadData& payload,
    std::chrono::milliseconds timeout) {

    return std::async(std::launch::async, [this, method, payload, timeout]() {
        return call_method(method, payload, timeout);
    });
}

bool ServiceProxy::call_method_no_return(MethodId method, const PayloadData& payload) {
    if (!app_ || !available_) {
        return false;
    }

    return app_->send_request_no_return(service_id_, instance_id_, method, payload, false);
}

bool ServiceProxy::subscribe_event(EventId event,
                                   EventgroupId eventgroup,
                                   MessageHandler handler) {
    if (!app_) return false;

    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        event_handlers_[event] = {eventgroup, std::move(handler)};
    }

    app_->set_event_handler(event,
        [this](MessagePtr&& msg) { on_event_received(std::move(msg)); });

    return app_->subscribe_event(service_id_, instance_id_, eventgroup, event, 1);
}

void ServiceProxy::unsubscribe_event(EventId event, EventgroupId eventgroup) {
    if (!app_) return;

    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        event_handlers_.erase(event);
    }

    app_->unsubscribe_event(service_id_, instance_id_, eventgroup);
}

std::optional<PayloadData> ServiceProxy::get_field(FieldId field,
                                                    std::chrono::milliseconds timeout) {
    // Field getter uses special method ID
    MethodId getter_id = 0x0100 | (field & 0x00FF);
    return call_method(getter_id, {}, timeout);
}

bool ServiceProxy::set_field(FieldId field, const PayloadData& payload) {
    // Field setter uses special method ID
    MethodId setter_id = 0x0200 | (field & 0x00FF);
    auto result = call_method(setter_id, payload);
    return result.has_value();
}

void ServiceProxy::on_service_available(ServiceId service, InstanceId instance, bool available) {
    if (service == service_id_ && instance == instance_id_) {
        available_ = available;
        if (availability_handler_) {
            availability_handler_(service, instance, available);
        }
    }
}

void ServiceProxy::on_event_received(MessagePtr&& message) {
    if (!message) return;

    std::lock_guard<std::mutex> lock(events_mutex_);
    auto it = event_handlers_.find(message->get_method_id());
    if (it != event_handlers_.end() && it->second.second) {
        it->second.second(std::move(message));
    }
}

} // namespace msomeip
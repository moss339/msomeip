#include "msomeip/application.h"
#include "msomeip/runtime.h"

#include <iostream>

namespace moss {
namespace msomeip {

Application::Application(std::shared_ptr<Runtime> runtime, std::string name)
    : runtime_(std::move(runtime)), name_(std::move(name)) {}

Application::~Application() {
    stop();
}

void Application::init() {
    // Register with runtime
    if (runtime_) {
        runtime_->register_service_handler(0, 0,
            [this](MessagePtr&& msg) { handle_message(std::move(msg)); });
    }
}

void Application::start() {
    if (running_.exchange(true)) return;
}

void Application::stop() {
    if (!running_.exchange(false)) return;

    // Complete all pending requests with error
    std::lock_guard<std::mutex> lock(requests_mutex_);
    for (auto& [id, req] : pending_requests_) {
        auto msg = Message::create_error_response(*req->promise.get_future().get(),
                                                   ReturnCode::E_NOT_OK);
    }
    pending_requests_.clear();
}

std::future<MessagePtr> Application::send_request(
    ServiceId service,
    InstanceId instance,
    MethodId method,
    const PayloadData& payload,
    bool reliable,
    std::chrono::milliseconds timeout) {

    std::lock_guard<std::mutex> lock(session_mutex_);

    auto msg = Message::create_request(service, instance, method,
                                       runtime_->get_client_id(), reliable);
    msg->set_session_id(session_id_++);
    msg->set_payload(payload);

    auto pending = std::make_shared<PendingRequest>();
    pending->timestamp = std::chrono::steady_clock::now();
    pending->timeout = timeout;

    auto future = pending->promise.get_future();

    {
        std::lock_guard<std::mutex> req_lock(requests_mutex_);
        pending_requests_[msg->get_request_id()] = pending;
    }

    // Get service endpoints from SD
    auto endpoints = runtime_->get_service_discovery().get_service_endpoints(service, instance);

    if (endpoints.empty()) {
        pending->promise.set_value(Message::create_error_response(*msg, ReturnCode::E_NOT_REACHABLE));
        return future;
    }

    // Send via first available endpoint
    for (const auto& ep : endpoints) {
        if (ep.protocol == IpProtocol::UDP && !reliable) {
            if (runtime_->send_message(*msg, ep.address, ep.port, false)) {
                return future;
            }
        }
    }

    pending->promise.set_value(Message::create_error_response(*msg, ReturnCode::E_NOT_REACHABLE));
    return future;
}

bool Application::send_request_no_return(
    ServiceId service,
    InstanceId instance,
    MethodId method,
    const PayloadData& payload,
    bool reliable) {

    std::lock_guard<std::mutex> lock(session_mutex_);

    auto msg = Message::create_request(service, instance, method,
                                       runtime_->get_client_id(), reliable);
    msg->set_session_id(session_id_++);
    msg->set_message_type(MessageType::REQUEST_NO_RETURN);
    msg->set_payload(payload);

    auto endpoints = runtime_->get_service_discovery().get_service_endpoints(service, instance);

    for (const auto& ep : endpoints) {
        if (ep.protocol == IpProtocol::UDP && !reliable) {
            if (runtime_->send_message(*msg, ep.address, ep.port, false)) {
                return true;
            }
        }
    }

    return false;
}

bool Application::subscribe_event(
    ServiceId service,
    InstanceId instance,
    EventgroupId eventgroup,
    EventId event,
    MajorVersion major_version) {

    std::lock_guard<std::mutex> lock(event_configs_mutex_);
    event_configs_[event] = {service, instance, event, eventgroup, false, false};

    return runtime_->get_service_discovery().subscribe_eventgroup(
        service, instance, eventgroup, major_version);
}

void Application::unsubscribe_event(
    ServiceId service,
    InstanceId instance,
    EventgroupId eventgroup) {

    runtime_->get_service_discovery().unsubscribe_eventgroup(service, instance, eventgroup);
}

void Application::set_event_handler(EventId event, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    event_handlers_[event] = std::move(handler);
}

void Application::offer_service(const ServiceConfig& config) {
    runtime_->get_service_discovery().offer_service(config);
}

void Application::stop_offer_service(ServiceId service, InstanceId instance) {
    runtime_->get_service_discovery().stop_offer_service(service, instance);
}

void Application::register_method_handler(MethodId method, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    method_handlers_[method] = std::move(handler);
}

void Application::unregister_method_handler(MethodId method) {
    std::lock_guard<std::mutex> lock(methods_mutex_);
    method_handlers_.erase(method);
}

void Application::send_response(MessagePtr& request, const PayloadData& payload) {
    auto response = Message::create_response(*request);
    response->set_payload(payload);

    // Send back to requester
    auto endpoints = runtime_->get_service_discovery().get_service_endpoints(
        request->get_service_id(), request->get_instance_id());

    for (const auto& ep : endpoints) {
        if (ep.protocol == IpProtocol::UDP) {
            runtime_->send_message(*response, ep.address, ep.port, false);
            break;
        }
    }
}

void Application::send_error_response(MessagePtr& request, ReturnCode code) {
    auto response = Message::create_error_response(*request, code);

    auto endpoints = runtime_->get_service_discovery().get_service_endpoints(
        request->get_service_id(), request->get_instance_id());

    for (const auto& ep : endpoints) {
        if (ep.protocol == IpProtocol::UDP) {
            runtime_->send_message(*response, ep.address, ep.port, false);
            break;
        }
    }
}

void Application::offer_event(const EventConfig& config) {
    std::lock_guard<std::mutex> lock(event_configs_mutex_);
    event_configs_[config.event_id] = config;

    runtime_->get_service_discovery().offer_eventgroup(
        config.service_id, config.instance_id, config.eventgroup_id, {});
}

void Application::stop_offer_event(ServiceId service, InstanceId instance, EventId event) {
    std::lock_guard<std::mutex> lock(event_configs_mutex_);
    auto it = event_configs_.find(event);
    if (it != event_configs_.end()) {
        runtime_->get_service_discovery().stop_offer_eventgroup(
            service, instance, it->second.eventgroup_id);
        event_configs_.erase(it);
    }
}

void Application::notify_event(EventId event, const PayloadData& payload) {
    std::lock_guard<std::mutex> lock(event_configs_mutex_);
    auto it = event_configs_.find(event);
    if (it == event_configs_.end()) return;

    auto msg = Message::create_notification(
        it->second.service_id, it->second.instance_id, event);
    msg->set_payload(payload);

    // In real implementation, we'd send to all subscribers
    // For now, just broadcast
}

void Application::offer_field(const EventConfig& config,
                               std::function<PayloadData()> getter,
                               std::function<bool(const PayloadData&)> setter) {
    std::lock_guard<std::mutex> lock(fields_mutex_);

    FieldInfo info;
    info.getter = std::move(getter);
    info.setter = std::move(setter);
    fields_[config.event_id] = std::move(info);

    // Also offer as event
    offer_event(config);
}

void Application::set_field_value(EventId field, const PayloadData& value) {
    {
        std::lock_guard<std::mutex> lock(fields_mutex_);
        auto it = fields_.find(field);
        if (it != fields_.end()) {
            it->second.value = value;
        }
    }

    // Notify subscribers
    notify_event(field, value);
}

void Application::on_service_available(ServiceId service, InstanceId instance, bool available) {
    if (availability_handler_) {
        availability_handler_(service, instance, available);
    }
}

void Application::set_availability_handler(AvailabilityHandler handler) {
    availability_handler_ = std::move(handler);
}

void Application::handle_message(MessagePtr&& message) {
    if (message->is_response_type()) {
        // Handle response
        std::lock_guard<std::mutex> lock(requests_mutex_);
        auto it = pending_requests_.find(message->get_request_id());
        if (it != pending_requests_.end()) {
            it->second->promise.set_value(message);
            pending_requests_.erase(it);
        }
    } else if (message->is_event()) {
        // Handle event
        std::lock_guard<std::mutex> lock(events_mutex_);
        auto it = event_handlers_.find(message->get_method_id());
        if (it != event_handlers_.end() && it->second) {
            it->second(std::move(message));
        }
    } else if (message->is_request_type()) {
        // Handle method call
        std::lock_guard<std::mutex> lock(methods_mutex_);
        auto it = method_handlers_.find(message->get_method_id());
        if (it != method_handlers_.end() && it->second) {
            it->second(std::move(message));
        } else {
            // Method not found
            send_error_response(message, ReturnCode::E_UNKNOWN_METHOD);
        }
    }

    cleanup_expired_requests();
}

void Application::cleanup_expired_requests() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(requests_mutex_);

    for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second->timestamp);
        if (elapsed > it->second->timeout) {
            auto msg = Message::create_error_response(
                *Message::create_request(0, 0, 0, 0), ReturnCode::E_TIMEOUT);
            it->second->promise.set_value(msg);
            it = pending_requests_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace msomeip
}  // namespace moss

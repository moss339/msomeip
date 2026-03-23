#include "msomeip/sd/service_discovery.h"
#include "msomeip/message/message.h"

#include <cstring>
#include <algorithm>
#include <random>
#include <sstream>

namespace msomeip {
namespace sd {

ServiceDiscovery::ServiceDiscovery(Config config) : config_(std::move(config)) {}

ServiceDiscovery::~ServiceDiscovery() {
    stop();
}

void ServiceDiscovery::init(const std::string& local_address,
                            uint16_t udp_port,
                            std::optional<uint16_t> tcp_port) {
    local_address_ = local_address;
    local_udp_port_ = udp_port;
    local_tcp_port_ = tcp_port;
}

void ServiceDiscovery::start() {
    if (running_.exchange(true)) return;

    worker_thread_ = std::thread(&ServiceDiscovery::worker_thread, this);
}

void ServiceDiscovery::stop() {
    running_ = false;
    cv_.notify_all();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void ServiceDiscovery::offer_service(const ServiceConfig& config) {
    std::lock_guard<std::mutex> lock(services_mutex_);

    ServiceInfo info;
    info.service_id = config.service_id;
    info.instance_id = config.instance_id;
    info.major_version = config.major_version;
    info.minor_version = config.minor_version;
    info.ttl = static_cast<uint32_t>(config.ttl.count());
    info.is_offered = true;
    info.is_local = true;
    info.last_update = std::chrono::steady_clock::now();

    if (config.unreliable) {
        info.endpoints.push_back({local_address_, config.unreliable_port, IpProtocol::UDP});
    }
    if (config.reliable && local_tcp_port_) {
        info.endpoints.push_back({local_address_, config.reliable_port, IpProtocol::TCP});
    }

    local_services_[{config.service_id, config.instance_id}] = std::move(info);

    // Send initial offer with random delay
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(config_.initial_delay_min.count(),
                                        config_.initial_delay_max.count());

    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
    send_offer_service(local_services_[{config.service_id, config.instance_id}]);
}

void ServiceDiscovery::stop_offer_service(ServiceId service, InstanceId instance) {
    std::lock_guard<std::mutex> lock(services_mutex_);

    auto it = local_services_.find({service, instance});
    if (it != local_services_.end()) {
        it->second.is_offered = false;
        it->second.ttl = 0; // Stop offer
        send_offer_service(it->second);
        local_services_.erase(it);
    }
}

void ServiceDiscovery::find_service(ServiceId service, InstanceId instance) {
    std::lock_guard<std::mutex> lock(services_mutex_);

    auto key = std::make_pair(service, instance);
    find_requests_[key] = std::chrono::steady_clock::now();

    // Send find service immediately
    send_find_service(service, instance);
}

void ServiceDiscovery::stop_find_service(ServiceId service, InstanceId instance) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    find_requests_.erase({service, instance});
}

bool ServiceDiscovery::subscribe_eventgroup(ServiceId service,
                                            InstanceId instance,
                                            EventgroupId eventgroup,
                                            MajorVersion major_version) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    PendingSubscription sub;
    sub.service_id = service;
    sub.instance_id = instance;
    sub.eventgroup_id = eventgroup;
    sub.timestamp = std::chrono::steady_clock::now();
    sub.retry_count = 0;

    pending_subscriptions_.push_back(std::move(sub));
    return true;
}

void ServiceDiscovery::unsubscribe_eventgroup(ServiceId service,
                                               InstanceId instance,
                                               EventgroupId eventgroup) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    pending_subscriptions_.erase(
        std::remove_if(pending_subscriptions_.begin(), pending_subscriptions_.end(),
            [&](const PendingSubscription& sub) {
                return sub.service_id == service &&
                       sub.instance_id == instance &&
                       sub.eventgroup_id == eventgroup;
            }),
        pending_subscriptions_.end());
}

void ServiceDiscovery::offer_eventgroup(ServiceId service,
                                         InstanceId instance,
                                         EventgroupId eventgroup,
                                         const std::vector<Endpoint>& endpoints) {
    std::lock_guard<std::mutex> lock(services_mutex_);

    EventgroupInfo info;
    info.service_id = service;
    info.instance_id = instance;
    info.eventgroup_id = eventgroup;
    info.endpoints = endpoints;

    eventgroups_[{service, instance, eventgroup}] = std::move(info);
}

void ServiceDiscovery::stop_offer_eventgroup(ServiceId service,
                                              InstanceId instance,
                                              EventgroupId eventgroup) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    eventgroups_.erase({service, instance, eventgroup});
}

void ServiceDiscovery::set_service_available_handler(AvailabilityHandler handler) {
    availability_handler_ = std::move(handler);
}

void ServiceDiscovery::set_subscription_handler(SubscriptionHandler handler) {
    subscription_handler_ = std::move(handler);
}

std::vector<ServiceInfo> ServiceDiscovery::get_discovered_services() const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    std::vector<ServiceInfo> result;
    for (const auto& [key, info] : discovered_services_) {
        result.push_back(info);
    }
    return result;
}

std::optional<ServiceInfo> ServiceDiscovery::get_service_info(ServiceId service,
                                                              InstanceId instance) const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = discovered_services_.find({service, instance});
    if (it != discovered_services_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<Endpoint> ServiceDiscovery::get_service_endpoints(ServiceId service,
                                                               InstanceId instance) const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = discovered_services_.find({service, instance});
    if (it != discovered_services_.end()) {
        return it->second.endpoints;
    }
    return {};
}

void ServiceDiscovery::process_sd_message(const Message& message) {
    const auto& payload = message.get_payload();
    if (!payload || payload->size() < 12) return;

    const uint8_t* data = payload->data();
    size_t offset = 0;

    // Parse SD header
    uint8_t flags = data[offset++];
    offset += 3; // Reserved

    uint32_t entries_length = (static_cast<uint32_t>(data[offset]) << 24) |
                              (static_cast<uint32_t>(data[offset + 1]) << 16) |
                              (static_cast<uint32_t>(data[offset + 2]) << 8) |
                              data[offset + 3];
    offset += 4;

    // Parse entries
    std::vector<ServiceEntry> entries;
    size_t entries_end = offset + entries_length;
    while (offset + ServiceEntry::ENTRY_SIZE <= entries_end &&
           offset + ServiceEntry::ENTRY_SIZE <= payload->size()) {
        auto entry = ServiceEntry::deserialize(data + offset, entries_end - offset);
        if (entry) {
            entries.push_back(*entry);
        }
        offset += ServiceEntry::ENTRY_SIZE;
    }

    // Parse options
    offset = entries_end;
    if (offset + 4 > payload->size()) return;

    uint32_t options_length = (static_cast<uint32_t>(data[offset]) << 24) |
                              (static_cast<uint32_t>(data[offset + 1]) << 16) |
                              (static_cast<uint32_t>(data[offset + 2]) << 8) |
                              data[offset + 3];
    offset += 4;

    std::vector<std::unique_ptr<SdOption>> options;
    size_t options_end = offset + options_length;
    while (offset < options_end && offset < payload->size()) {
        auto option = SdOption::deserialize(data + offset, options_end - offset);
        if (option) {
            offset += option->get_size();
            options.push_back(std::move(option));
        } else {
            break;
        }
    }

    // Process entries
    for (const auto& entry : entries) {
        switch (entry.get_type()) {
            case SdEntryType::FIND_SERVICE:
                handle_find_service(entry);
                break;
            case SdEntryType::OFFER_SERVICE:
                handle_offer_service(entry, options);
                break;
            case SdEntryType::SUBSCRIBE_EVENTGROUP:
                handle_subscribe_eventgroup(entry, options);
                break;
            case SdEntryType::SUBSCRIBE_EVENTGROUP_ACK:
                handle_subscribe_eventgroup_ack(entry);
                break;
        }
    }
}

void ServiceDiscovery::handle_find_service(const ServiceEntry& entry) {
    std::lock_guard<std::mutex> lock(services_mutex_);

    for (const auto& [key, info] : local_services_) {
        if (info.is_offered &&
            info.service_id == entry.get_service_id() &&
            (entry.get_instance_id() == 0xFFFF || info.instance_id == entry.get_instance_id())) {
            send_offer_service(info);
        }
    }
}

void ServiceDiscovery::handle_offer_service(const ServiceEntry& entry,
                                            const std::vector<std::unique_ptr<SdOption>>& options) {
    ServiceInfo info;
    info.service_id = entry.get_service_id();
    info.instance_id = entry.get_instance_id();
    info.major_version = entry.get_major_version();
    info.minor_version = entry.get_minor_version();
    info.ttl = entry.get_ttl();
    info.last_update = std::chrono::steady_clock::now();
    info.is_offered = entry.get_ttl() > 0;

    // Extract endpoints from options
    for (const auto& opt : options) {
        if (auto* endpoint = dynamic_cast<IPv4EndpointOption*>(opt.get())) {
            info.endpoints.push_back({
                endpoint->get_address(),
                endpoint->get_port(),
                endpoint->get_protocol()
            });
        }
    }

    update_service_info(info);

    if (availability_handler_ && entry.get_ttl() > 0) {
        availability_handler_(info.service_id, info.instance_id, true);
    }
}

void ServiceDiscovery::handle_subscribe_eventgroup(const ServiceEntry& entry,
                                                   const std::vector<std::unique_ptr<SdOption>>& options) {
    std::lock_guard<std::mutex> lock(services_mutex_);

    auto key = std::make_pair(entry.get_service_id(), entry.get_instance_id());
    auto it = local_services_.find(key);
    if (it == local_services_.end()) return;

    SubscriptionInfo sub_info;
    sub_info.service_id = entry.get_service_id();
    sub_info.instance_id = entry.get_instance_id();
    sub_info.eventgroup_id = entry.get_eventgroup_id();
    sub_info.major_version = entry.get_major_version();
    sub_info.ttl = entry.get_ttl();

    for (const auto& opt : options) {
        if (auto* endpoint = dynamic_cast<IPv4EndpointOption*>(opt.get())) {
            sub_info.endpoints.push_back({
                endpoint->get_address(),
                endpoint->get_port(),
                endpoint->get_protocol()
            });
        }
    }

    bool accepted = true;
    if (subscription_handler_) {
        accepted = subscription_handler_(sub_info);
    }

    if (accepted) {
        send_subscribe_eventgroup_ack(it->second, entry.get_eventgroup_id());
    } else {
        send_subscribe_eventgroup_nack(it->second, entry.get_eventgroup_id());
    }
}

void ServiceDiscovery::handle_subscribe_eventgroup_ack(const ServiceEntry& entry) {
    // Remove from pending subscriptions
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    pending_subscriptions_.erase(
        std::remove_if(pending_subscriptions_.begin(), pending_subscriptions_.end(),
            [&](const PendingSubscription& sub) {
                return sub.service_id == entry.get_service_id() &&
                       sub.instance_id == entry.get_instance_id() &&
                       sub.eventgroup_id == entry.get_eventgroup_id();
            }),
        pending_subscriptions_.end());
}

void ServiceDiscovery::send_find_service(ServiceId service, InstanceId instance) {
    // This would send a multicast message
    // Implementation depends on transport layer
}

void ServiceDiscovery::send_offer_service(const ServiceInfo& info) {
    // This would send a multicast message
    // Implementation depends on transport layer
}

void ServiceDiscovery::send_subscribe_eventgroup(const PendingSubscription& sub) {
    // Implementation depends on transport layer
}

void ServiceDiscovery::send_subscribe_eventgroup_ack(const ServiceInfo& service,
                                                      EventgroupId eventgroup) {
    // Implementation depends on transport layer
}

void ServiceDiscovery::send_subscribe_eventgroup_nack(const ServiceInfo& service,
                                                       EventgroupId eventgroup) {
    // Implementation depends on transport layer
}

void ServiceDiscovery::check_ttl_expiration() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(services_mutex_);

    for (auto it = discovered_services_.begin(); it != discovered_services_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_update).count();
        if (elapsed > static_cast<int64_t>(it->second.ttl)) {
            if (availability_handler_) {
                availability_handler_(it->second.service_id, it->second.instance_id, false);
            }
            it = discovered_services_.erase(it);
        } else {
            ++it;
        }
    }
}

void ServiceDiscovery::send_periodic_offers() {
    std::lock_guard<std::mutex> lock(services_mutex_);

    for (const auto& [key, info] : local_services_) {
        if (info.is_offered) {
            send_offer_service(info);
        }
    }
}

void ServiceDiscovery::retry_pending_subscriptions() {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    for (const auto& sub : pending_subscriptions_) {
        if (sub.retry_count < config_.repetition_max) {
            send_subscribe_eventgroup(sub);
        }
    }
}

bool ServiceDiscovery::is_local_service(ServiceId service, InstanceId instance) const {
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = local_services_.find({service, instance});
    return it != local_services_.end() && it->second.is_offered;
}

void ServiceDiscovery::update_service_info(const ServiceInfo& info) {
    std::lock_guard<std::mutex> lock(services_mutex_);
    discovered_services_[{info.service_id, info.instance_id}] = info;
}

void ServiceDiscovery::worker_thread() {
    auto next_offer_time = std::chrono::steady_clock::now() + config_.cyclic_offer_delay;

    while (running_) {
        std::unique_lock<std::mutex> lock(cv_mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(100));

        if (!running_) break;

        // Check TTL expiration
        check_ttl_expiration();

        // Send periodic offers
        auto now = std::chrono::steady_clock::now();
        if (now >= next_offer_time) {
            send_periodic_offers();
            next_offer_time = now + config_.cyclic_offer_delay;
        }

        // Retry pending subscriptions
        retry_pending_subscriptions();
    }
}

} // namespace sd
} // namespace msomeip

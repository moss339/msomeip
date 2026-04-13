#pragma once

#include "msomeip/types.h"
#include "msomeip/sd/service_entry.h"
#include "msomeip/message/message.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <map>

namespace moss {
namespace msomeip {
namespace sd {

class UDPTransportBase;

// Service Discovery message format
// +----------------------------+
// | SOME/IP Header (16 bytes)  |
// +----------------------------+
// | Flags (8 bits)             |
// +----------------------------+
// | Reserved (24 bits)         |
// +----------------------------+
// | Length of Entries Array    |
// +----------------------------+
// | Entries Array              |
// +----------------------------+
// | Length of Options Array    |
// +----------------------------+
// | Options Array              |
// +----------------------------+

struct ServiceInfo {
    ServiceId service_id;
    InstanceId instance_id;
    MajorVersion major_version;
    MinorVersion minor_version;
    uint32_t ttl;
    std::chrono::steady_clock::time_point last_update;
    std::vector<Endpoint> endpoints;
    bool is_offered = false;
    bool is_local = false;
};

struct EventgroupInfo {
    ServiceId service_id;
    InstanceId instance_id;
    EventgroupId eventgroup_id;
    MajorVersion major_version;
    std::vector<Endpoint> endpoints;
    std::vector<Endpoint> subscribers;
};

struct PendingSubscription {
    ServiceId service_id;
    InstanceId instance_id;
    EventgroupId eventgroup_id;
    std::chrono::steady_clock::time_point timestamp;
    uint8_t retry_count = 0;
};

struct Config {
    std::string multicast_address = "224.244.224.245";
    uint16_t port = SOMEIP_SD_PORT;
    std::chrono::milliseconds initial_delay_min = std::chrono::milliseconds(50);
    std::chrono::milliseconds initial_delay_max = std::chrono::milliseconds(150);
    std::chrono::milliseconds repetition_base_delay = std::chrono::milliseconds(30);
    uint8_t repetition_max = 3;
    std::chrono::milliseconds cyclic_offer_delay = std::chrono::milliseconds(2000);
    std::chrono::milliseconds offer_cancellation_delay = std::chrono::milliseconds(5000);
    std::chrono::milliseconds ttl_factor_offers = std::chrono::milliseconds(10);
    std::chrono::milliseconds ttl_factor_subscriptions = std::chrono::milliseconds(10);
    std::chrono::milliseconds find_cancellation_delay = std::chrono::milliseconds(5000);
};

class ServiceDiscovery {
public:
    explicit ServiceDiscovery(Config config);
    ~ServiceDiscovery();

    // Initialize with local endpoints
    void init(const std::string& local_address,
              uint16_t udp_port,
              std::optional<uint16_t> tcp_port = std::nullopt);

    // Service lifecycle
    void offer_service(const ServiceConfig& config);
    void stop_offer_service(ServiceId service, InstanceId instance);

    // Service discovery
    void find_service(ServiceId service, InstanceId instance = 0xFFFF);
    void stop_find_service(ServiceId service, InstanceId instance);

    // Event subscription
    bool subscribe_eventgroup(ServiceId service,
                              InstanceId instance,
                              EventgroupId eventgroup,
                              MajorVersion major_version);
    void unsubscribe_eventgroup(ServiceId service,
                                InstanceId instance,
                                EventgroupId eventgroup);

    // Eventgroup offering (for servers)
    void offer_eventgroup(ServiceId service,
                          InstanceId instance,
                          EventgroupId eventgroup,
                          const std::vector<Endpoint>& endpoints);
    void stop_offer_eventgroup(ServiceId service,
                               InstanceId instance,
                               EventgroupId eventgroup);

    // Callbacks
    void set_service_available_handler(AvailabilityHandler handler);
    void set_subscription_handler(SubscriptionHandler handler);

    // Callback for sending messages (set by Application)
    using SendMessageCallback = std::function<void(MessagePtr msg, const std::string& address, uint16_t port)>;
    void set_send_message_callback(SendMessageCallback callback) { send_message_callback_ = std::move(callback); }

    // Get discovered services
    std::vector<ServiceInfo> get_discovered_services() const;
    std::optional<ServiceInfo> get_service_info(ServiceId service,
                                                 InstanceId instance) const;

    // Get endpoints for a service
    std::vector<Endpoint> get_service_endpoints(ServiceId service,
                                                  InstanceId instance) const;

    // Get subscribers for an eventgroup
    std::vector<Endpoint> get_subscribers(ServiceId service,
                                           InstanceId instance,
                                           EventgroupId eventgroup) const;

    // Process incoming SD message
    void process_sd_message(const Message& message);

    // Build SD message
    MessagePtr build_sd_message();

    // Start/Stop
    void start();
    void stop();

private:
    void worker_thread();
    void handle_find_service(const ServiceEntry& entry);
    void handle_offer_service(const ServiceEntry& entry,
                              const std::vector<std::unique_ptr<SdOption>>& options);
    void handle_subscribe_eventgroup(const ServiceEntry& entry,
                                     const std::vector<std::unique_ptr<SdOption>>& options);
    void handle_subscribe_eventgroup_ack(const ServiceEntry& entry);

    void send_find_service(ServiceId service, InstanceId instance);
    void send_offer_service(const ServiceInfo& info);
    void send_subscribe_eventgroup(const PendingSubscription& sub);
    void send_subscribe_eventgroup_ack(const ServiceInfo& service,
                                       EventgroupId eventgroup);
    void send_subscribe_eventgroup_nack(const ServiceInfo& service,
                                        EventgroupId eventgroup);

    void check_ttl_expiration();
    void send_periodic_offers();
    void retry_pending_subscriptions();

    bool is_local_service(ServiceId service, InstanceId instance) const;
    void update_service_info(const ServiceInfo& info);

    // Configuration
    Config config_;

    // Local endpoints
    std::string local_address_;
    uint16_t local_udp_port_ = 0;
    std::optional<uint16_t> local_tcp_port_;

    // Service registry
    mutable std::mutex services_mutex_;
    std::map<std::pair<ServiceId, InstanceId>, ServiceInfo> local_services_;
    std::map<std::pair<ServiceId, InstanceId>, ServiceInfo> discovered_services_;
    std::map<std::tuple<ServiceId, InstanceId, EventgroupId>, EventgroupInfo> eventgroups_;

    // Pending subscriptions
    mutable std::mutex subscriptions_mutex_;
    std::vector<PendingSubscription> pending_subscriptions_;

    // Services being searched
    std::map<std::pair<ServiceId, InstanceId>, std::chrono::steady_clock::time_point> find_requests_;

    // Callbacks
    AvailabilityHandler availability_handler_;
    SubscriptionHandler subscription_handler_;
    SendMessageCallback send_message_callback_;

    // Threading
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    mutable std::mutex cv_mutex_;
    std::condition_variable cv_;

    // Session ID counter
    std::atomic<SessionId> session_counter_{1};
};

} // namespace sd
}  // namespace msomeip
}  // namespace moss

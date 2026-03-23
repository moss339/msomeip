#pragma once

#include "msomeip/types.h"
#include <shm/shm_types.h>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <queue>
#include <condition_variable>

namespace msomeip {

using AvailabilityHandler = std::function<void(ServiceId, InstanceId, bool)>;
using SubscriptionHandler = std::function<bool(const SubscriptionInfo&)>;

enum class RouteCommandType : uint8_t {
    REGISTER_SERVICE = 0x01,
    UNREGISTER_SERVICE = 0x02,
    FIND_SERVICE = 0x03,
    SUBSCRIBE = 0x04,
    UNSUBSCRIBE = 0x05,
    LOOKUP_SERVICE = 0x06,
    LOOKUP_SUBSCRIBERS = 0x07,
    HEARTBEAT = 0x08,
};

enum class RouteResponseType : uint8_t {
    SERVICE_AVAILABLE = 0x10,
    SERVICE_UNAVAILABLE = 0x11,
    SUBSCRIPTION_ACK = 0x12,
    SUBSCRIPTION_NACK = 0x13,
    LOOKUP_RESULT = 0x14,
    ERROR = 0x1F,
};

class ShmAgent {
public:
    ShmAgent();
    ~ShmAgent();

    ShmAgent(const ShmAgent&) = delete;
    ShmAgent& operator=(const ShmAgent&) = delete;

    bool connect(const std::string& shm_name = "mrouting_shm");
    void disconnect();
    bool is_connected() const { return connected_.load(); }

    bool send_register_service(const ServiceConfig& config);
    bool send_unregister_service(ServiceId service, InstanceId instance);
    bool send_find_service(ServiceId service, InstanceId instance = 0xFFFF);
    bool send_subscribe(ServiceId service, InstanceId instance,
                        EventgroupId eventgroup, MajorVersion major_version);
    bool send_unsubscribe(ServiceId service, InstanceId instance,
                          EventgroupId eventgroup);
    bool send_heartbeat(uint32_t client_id);

    std::vector<Endpoint> lookup_service_endpoints(ServiceId service, InstanceId instance);
    std::vector<Endpoint> lookup_subscribers(ServiceId service, InstanceId instance,
                                             EventgroupId eventgroup);

    void set_service_available_handler(AvailabilityHandler handler);
    void set_subscription_ack_handler(SubscriptionHandler handler);
    int get_notify_fd() const;
    void set_client_id(uint32_t client_id) { client_id_ = client_id; }
    uint32_t get_client_id() const { return client_id_; }

private:
    void worker_thread();
    void process_responses();
    bool send_command(RouteCommandType type, const std::vector<uint8_t>& payload);

    struct ResponseHeader {
        uint32_t sequence;
        RouteResponseType type;
        uint32_t payload_length;
    };
    std::optional<ResponseHeader> deserialize_header(const uint8_t* data, size_t length);

    shm_handle_t shm_handle_ = nullptr;
    void* shm_data_ = nullptr;
    size_t shm_data_size_ = 0;
    int notify_fd_ = -1;

    std::atomic<uint32_t> sequence_{1};
    uint32_t client_id_ = 0;
    std::atomic<bool> connected_{false};

    std::thread worker_thread_;
    std::atomic<bool> running_{false};

    std::queue<std::vector<uint8_t>> response_queue_;
    std::mutex response_mutex_;
    std::condition_variable response_cv_;

    AvailabilityHandler availability_handler_;
    SubscriptionHandler subscription_handler_;

    std::map<uint32_t, RouteCommandType> pending_requests_;
    std::mutex pending_mutex_;
};

} // namespace msomeip

#include "msomeip/shm_agent.h"
#include <shm/shm_api.h>
#include <cstring>
#include <algorithm>
#include <arpa/inet.h>

namespace msomeip {

ShmAgent::ShmAgent()
    : shm_data_(nullptr)
    , shm_data_size_(0)
    , notify_fd_(-1)
    , client_id_(0) {
}

ShmAgent::~ShmAgent() {
    disconnect();
}

bool ShmAgent::connect(const std::string& shm_name) {
    if (connected_.load()) {
        return true;
    }

    shm_handle_t handle = nullptr;
    shm_error_t err = shm_client_connect(shm_name.c_str(),
                                        static_cast<shm_permission_t>(SHM_PERM_READ | SHM_PERM_WRITE),
                                        &handle);
    if (err != SHM_OK || handle == nullptr) {
        return false;
    }

    shm_handle_ = handle;
    shm_data_ = shm_get_data_ptr(handle);
    shm_data_size_ = shm_get_data_size(handle);
    notify_fd_ = shm_get_notify_fd(handle);

    connected_ = true;
    running_ = true;

    worker_thread_ = std::thread(&ShmAgent::worker_thread, this);

    return true;
}

void ShmAgent::disconnect() {
    if (!connected_.load()) {
        return;
    }

    running_ = false;

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    if (shm_handle_ != nullptr) {
        shm_close(&shm_handle_);
        shm_handle_ = nullptr;
        shm_data_ = nullptr;
        shm_data_size_ = 0;
        notify_fd_ = -1;
    }

    connected_ = false;
}

bool ShmAgent::send_command(RouteCommandType type, const std::vector<uint8_t>& payload) {
    if (!connected_.load() || shm_data_ == nullptr) {
        return false;
    }

    if (shm_lock(shm_handle_, 1000) != SHM_OK) {
        return false;
    }

    struct CommandQueue {
        uint32_t count;
        uint32_t read_index;
        uint32_t write_index;
        uint32_t capacity;
    };

    auto* header = static_cast<const uint8_t*>(shm_data_);
    uint32_t cmd_queue_offset;
    uint32_t cmd_queue_size;
    std::memcpy(&cmd_queue_offset, header + 16, 4);
    std::memcpy(&cmd_queue_size, header + 20, 4);

    auto* queue = reinterpret_cast<CommandQueue*>(static_cast<uint8_t*>(shm_data_) + cmd_queue_offset);

    if (queue->count >= queue->capacity) {
        shm_unlock(shm_handle_);
        return false;
    }

    uint32_t seq = sequence_.fetch_add(1);

    uint8_t* queue_data = static_cast<uint8_t*>(shm_data_) + cmd_queue_offset + sizeof(CommandQueue);
    size_t offset = queue->write_index * (4 + 2 + 1 + 4 + 256);

    uint32_t seq_n = htonl(seq);
    std::memcpy(queue_data + offset, &seq_n, 4);
    offset += 4;

    uint16_t client_n = htons(static_cast<uint16_t>(client_id_));
    std::memcpy(queue_data + offset, &client_n, 2);
    offset += 2;

    queue_data[offset] = static_cast<uint8_t>(type);
    offset += 1;

    uint32_t payload_len_n = htonl(static_cast<uint32_t>(payload.size()));
    std::memcpy(queue_data + offset, &payload_len_n, 4);
    offset += 4;

    if (!payload.empty()) {
        std::memcpy(queue_data + offset, payload.data(), std::min(payload.size(), size_t(256)));
    }

    queue->write_index = (queue->write_index + 1) % queue->capacity;
    queue->count++;

    shm_unlock(shm_handle_);
    shm_notify(shm_handle_);

    return true;
}

bool ShmAgent::send_register_service(const ServiceConfig& config) {
    std::vector<uint8_t> payload;

    uint16_t svc_id = htons(config.service_id);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&svc_id),
                  reinterpret_cast<const uint8_t*>(&svc_id) + sizeof(svc_id));

    uint16_t inst_id = htons(config.instance_id);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&inst_id),
                  reinterpret_cast<const uint8_t*>(&inst_id) + sizeof(inst_id));

    payload.push_back(config.major_version);

    uint32_t minor = htonl(config.minor_version);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&minor),
                  reinterpret_cast<const uint8_t*>(&minor) + sizeof(minor));

    uint32_t ttl = htonl(static_cast<uint32_t>(config.ttl.count()));
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&ttl),
                  reinterpret_cast<const uint8_t*>(&ttl) + sizeof(ttl));

    uint32_t ep_count = htonl(0);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&ep_count),
                  reinterpret_cast<const uint8_t*>(&ep_count) + sizeof(ep_count));

    return send_command(RouteCommandType::REGISTER_SERVICE, payload);
}

bool ShmAgent::send_unregister_service(ServiceId service, InstanceId instance) {
    std::vector<uint8_t> payload;

    uint16_t svc_id = htons(service);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&svc_id),
                  reinterpret_cast<const uint8_t*>(&svc_id) + sizeof(svc_id));

    uint16_t inst_id = htons(instance);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&inst_id),
                  reinterpret_cast<const uint8_t*>(&inst_id) + sizeof(inst_id));

    return send_command(RouteCommandType::UNREGISTER_SERVICE, payload);
}

bool ShmAgent::send_find_service(ServiceId service, InstanceId instance) {
    std::vector<uint8_t> payload;

    uint16_t svc_id = htons(service);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&svc_id),
                  reinterpret_cast<const uint8_t*>(&svc_id) + sizeof(svc_id));

    uint16_t inst_id = htons(instance);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&inst_id),
                  reinterpret_cast<const uint8_t*>(&inst_id) + sizeof(inst_id));

    return send_command(RouteCommandType::FIND_SERVICE, payload);
}

bool ShmAgent::send_subscribe(ServiceId service, InstanceId instance,
                              EventgroupId eventgroup, MajorVersion major_version) {
    std::vector<uint8_t> payload;

    uint16_t svc_id = htons(service);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&svc_id),
                  reinterpret_cast<const uint8_t*>(&svc_id) + sizeof(svc_id));

    uint16_t inst_id = htons(instance);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&inst_id),
                  reinterpret_cast<const uint8_t*>(&inst_id) + sizeof(inst_id));

    uint16_t eg_id = htons(eventgroup);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&eg_id),
                  reinterpret_cast<const uint8_t*>(&eg_id) + sizeof(eg_id));

    payload.push_back(major_version);

    uint32_t ep_count = htonl(0);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&ep_count),
                  reinterpret_cast<const uint8_t*>(&ep_count) + sizeof(ep_count));

    return send_command(RouteCommandType::SUBSCRIBE, payload);
}

bool ShmAgent::send_unsubscribe(ServiceId service, InstanceId instance, EventgroupId eventgroup) {
    std::vector<uint8_t> payload;

    uint16_t svc_id = htons(service);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&svc_id),
                  reinterpret_cast<const uint8_t*>(&svc_id) + sizeof(svc_id));

    uint16_t inst_id = htons(instance);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&inst_id),
                  reinterpret_cast<const uint8_t*>(&inst_id) + sizeof(inst_id));

    uint16_t eg_id = htons(eventgroup);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&eg_id),
                  reinterpret_cast<const uint8_t*>(&eg_id) + sizeof(eg_id));

    return send_command(RouteCommandType::UNSUBSCRIBE, payload);
}

bool ShmAgent::send_heartbeat(uint32_t client_id) {
    std::vector<uint8_t> payload;

    uint32_t client = htonl(client_id);
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&client),
                  reinterpret_cast<const uint8_t*>(&client) + sizeof(client));

    uint32_t reserved = 0;
    payload.insert(payload.end(),
                  reinterpret_cast<const uint8_t*>(&reserved),
                  reinterpret_cast<const uint8_t*>(&reserved) + sizeof(reserved));

    return send_command(RouteCommandType::HEARTBEAT, payload);
}

std::vector<Endpoint> ShmAgent::lookup_service_endpoints(ServiceId service, InstanceId instance) {
    std::vector<Endpoint> result;

    if (!connected_.load() || shm_data_ == nullptr) {
        return result;
    }

    if (shm_lock(shm_handle_, 1000) != SHM_OK) {
        return result;
    }

    auto* header = static_cast<const uint8_t*>(shm_data_);

    uint32_t sr_offset;
    uint32_t sr_size;
    std::memcpy(&sr_offset, header + 28, 4);
    std::memcpy(&sr_size, header + 32, 4);

    struct ServiceRegistry {
        uint32_t count;
    };

    auto* sr = reinterpret_cast<const ServiceRegistry*>(static_cast<const uint8_t*>(shm_data_) + sr_offset);
    const uint8_t* sr_data = static_cast<const uint8_t*>(shm_data_) + sr_offset + sizeof(ServiceRegistry);

    size_t offset = 0;
    size_t max_offset = sr_size - sizeof(ServiceRegistry);

    for (uint32_t i = 0; i < sr->count && offset < max_offset; ++i) {
        if (offset + 16 > max_offset) break;

        ServiceId svc_id;
        InstanceId inst_id;
        uint32_t ep_count;

        std::memcpy(&svc_id, sr_data + offset, 2);
        svc_id = ntohs(svc_id);
        offset += 2;

        std::memcpy(&inst_id, sr_data + offset, 2);
        inst_id = ntohs(inst_id);
        offset += 2;

        offset += 1; // major_version
        offset += 4; // minor_version
        offset += 4; // ttl
        offset += 1; // is_local

        std::memcpy(&ep_count, sr_data + offset, 4);
        ep_count = ntohl(ep_count);
        offset += 4;

        if (svc_id == service && inst_id == instance) {
            for (uint32_t j = 0; j < ep_count && offset < max_offset; ++j) {
                if (offset + 1 > max_offset) break;

                uint8_t addr_len = sr_data[offset];
                offset += 1;

                if (offset + addr_len + 3 > max_offset) break;

                Endpoint ep;
                ep.address.assign(reinterpret_cast<const char*>(sr_data + offset), addr_len);
                offset += addr_len;

                std::memcpy(&ep.port, sr_data + offset, 2);
                ep.port = ntohs(ep.port);
                offset += 2;

                ep.protocol = static_cast<IpProtocol>(sr_data[offset]);
                offset += 1;

                result.push_back(ep);
            }
            break;
        } else {
            for (uint32_t j = 0; j < ep_count && offset < max_offset; ++j) {
                if (offset + 1 > max_offset) break;
                uint8_t addr_len = sr_data[offset];
                offset += 1 + addr_len + 3;
            }
        }
    }

    shm_unlock(shm_handle_);
    return result;
}

std::vector<Endpoint> ShmAgent::lookup_subscribers(ServiceId, InstanceId, EventgroupId) {
    return {};
}

void ShmAgent::worker_thread() {
    while (running_.load()) {
        if (shm_wait(shm_handle_, 100) == SHM_OK) {
            shm_consume_notify(shm_handle_);
            process_responses();
        }
    }
}

void ShmAgent::process_responses() {
    if (!connected_.load() || shm_data_ == nullptr) {
        return;
    }

    if (shm_lock(shm_handle_, 100) != SHM_OK) {
        return;
    }

    struct ResponseQueue {
        uint32_t count;
        uint32_t read_index;
        uint32_t write_index;
        uint32_t capacity;
    };

    auto* header = static_cast<const uint8_t*>(shm_data_);
    uint32_t rsp_queue_offset;
    uint32_t rsp_queue_size;
    std::memcpy(&rsp_queue_offset, header + 24, 4);
    std::memcpy(&rsp_queue_size, header + 28, 4);

    auto* queue = reinterpret_cast<ResponseQueue*>(static_cast<uint8_t*>(shm_data_) + rsp_queue_offset);

    while (queue->count > 0) {
        uint8_t* queue_data = static_cast<uint8_t*>(shm_data_) + rsp_queue_offset + sizeof(ResponseQueue);
        size_t offset = queue->read_index * (4 + 1 + 4 + 256);

        uint8_t response[285];
        std::memcpy(response, queue_data + offset, sizeof(response));

        uint32_t seq;
        std::memcpy(&seq, response, 4);
        seq = ntohl(seq);

        RouteResponseType type = static_cast<RouteResponseType>(response[4]);
        uint32_t payload_len;
        std::memcpy(&payload_len, response + 5, 4);
        payload_len = ntohl(payload_len);

        std::vector<uint8_t> payload(response + 9, response + 9 + payload_len);

        queue->read_index = (queue->read_index + 1) % queue->capacity;
        queue->count--;

        switch (type) {
            case RouteResponseType::SERVICE_AVAILABLE: {
                if (payload.size() >= 4) {
                    ServiceId svc_id;
                    InstanceId inst_id;
                    std::memcpy(&svc_id, payload.data(), 2);
                    svc_id = ntohs(svc_id);
                    std::memcpy(&inst_id, payload.data() + 2, 2);
                    inst_id = ntohs(inst_id);

                    if (availability_handler_) {
                        availability_handler_(svc_id, inst_id, true);
                    }
                }
                break;
            }
            case RouteResponseType::SERVICE_UNAVAILABLE: {
                if (payload.size() >= 4) {
                    ServiceId svc_id;
                    InstanceId inst_id;
                    std::memcpy(&svc_id, payload.data(), 2);
                    svc_id = ntohs(svc_id);
                    std::memcpy(&inst_id, payload.data() + 2, 2);
                    inst_id = ntohs(inst_id);

                    if (availability_handler_) {
                        availability_handler_(svc_id, inst_id, false);
                    }
                }
                break;
            }
            case RouteResponseType::SUBSCRIPTION_ACK:
            case RouteResponseType::SUBSCRIPTION_NACK: {
                if (subscription_handler_ && payload.size() >= 7) {
                    SubscriptionInfo info;
                    std::memcpy(&info.service_id, payload.data(), 2);
                    info.service_id = ntohs(info.service_id);
                    std::memcpy(&info.instance_id, payload.data() + 2, 2);
                    info.instance_id = ntohs(info.instance_id);
                    std::memcpy(&info.eventgroup_id, payload.data() + 4, 2);
                    info.eventgroup_id = ntohs(info.eventgroup_id);

                    subscription_handler_(info);
                }
                break;
            }
            default:
                break;
        }
    }

    shm_unlock(shm_handle_);
}

void ShmAgent::set_service_available_handler(AvailabilityHandler handler) {
    availability_handler_ = handler;
}

void ShmAgent::set_subscription_ack_handler(SubscriptionHandler handler) {
    subscription_handler_ = handler;
}

int ShmAgent::get_notify_fd() const {
    return notify_fd_;
}

std::optional<ShmAgent::ResponseHeader> ShmAgent::deserialize_header(
    const uint8_t* data, size_t length) {
    if (length < 9) {
        return std::nullopt;
    }

    ResponseHeader header;
    std::memcpy(&header.sequence, data, 4);
    header.sequence = ntohl(header.sequence);

    header.type = static_cast<RouteResponseType>(data[4]);

    std::memcpy(&header.payload_length, data + 5, 4);
    header.payload_length = ntohl(header.payload_length);

    return header;
}

} // namespace msomeip

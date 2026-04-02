#pragma once

#include <cstdint>
#include <string>
#include <array>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include <variant>
#include <memory>
#include <atomic>

namespace moss {
namespace msomeip {

// SOME/IP protocol constants
constexpr uint16_t SOMEIP_PROTOCOL_VERSION = 0x01;
constexpr uint16_t SOMEIP_INTERFACE_VERSION = 0x01;
constexpr uint16_t SOMEIP_SD_PORT = 30490;
constexpr uint16_t SOMEIP_SD_METHOD_ID = 0x8100;
constexpr uint16_t SOMEIP_SD_SERVICE_ID = 0xFFFF;
constexpr uint16_t SOMEIP_SD_INSTANCE_ID = 0x0000;
constexpr uint32_t SOMEIP_MAGIC_COOKIE_CLIENT = 0xDBEEFDBE;
constexpr uint32_t SOMEIP_MAGIC_COOKIE_SERVER = 0xDBEEFD42;

// Message types
enum class MessageType : uint8_t {
    REQUEST = 0x00,
    REQUEST_NO_RETURN = 0x01,
    NOTIFICATION = 0x02,
    RESPONSE = 0x80,
    ERROR = 0x81,
    TP_REQUEST = 0x20,
    TP_REQUEST_NO_RETURN = 0x21,
    TP_NOTIFICATION = 0x22,
    TP_RESPONSE = 0xA0,
    TP_ERROR = 0xA1
};

// Return codes
enum class ReturnCode : uint8_t {
    E_OK = 0x00,
    E_NOT_OK = 0x01,
    E_UNKNOWN_SERVICE = 0x02,
    E_UNKNOWN_METHOD = 0x03,
    E_NOT_READY = 0x04,
    E_NOT_REACHABLE = 0x05,
    E_TIMEOUT = 0x06,
    E_WRONG_PROTOCOL_VERSION = 0x07,
    E_WRONG_INTERFACE_VERSION = 0x08,
    E_MALFORMED_MESSAGE = 0x09,
    E_WRONG_MESSAGE_TYPE = 0x0A
};

// Service Discovery message types
enum class SdMessageType : uint8_t {
    FIND_SERVICE = 0x00,
    OFFER_SERVICE = 0x01,
    STOP_OFFER = 0x02,
    SUBSCRIBE_EVENTGROUP = 0x06,
    STOP_SUBSCRIBE_EVENTGROUP = 0x07,
    SUBSCRIBE_EVENTGROUP_ACK = 0x08,
    SUBSCRIBE_EVENTGROUP_NACK = 0x09
};

// Entry types for Service Discovery
enum class SdEntryType : uint8_t {
    FIND_SERVICE = 0x00,
    OFFER_SERVICE = 0x01,
    SUBSCRIBE_EVENTGROUP = 0x06,
    SUBSCRIBE_EVENTGROUP_ACK = 0x07
};

// IP protocol types
enum class IpProtocol : uint8_t {
    UDP = 0x06,
    TCP = 0x11
};

// Service identifiers
using ServiceId = uint16_t;
using InstanceId = uint16_t;
using MethodId = uint16_t;
using EventId = uint16_t;
using FieldId = uint16_t;
using SessionId = uint16_t;
using ClientId = uint16_t;
using RequestId = uint32_t;
using MessageId = uint32_t;
using MajorVersion = uint8_t;
using MinorVersion = uint32_t;
using EventgroupId = uint16_t;

// Unique service identifier
struct ServiceIdTuple {
    ServiceId service;
    InstanceId instance;

    bool operator==(const ServiceIdTuple& other) const {
        return service == other.service && instance == other.instance;
    }

    bool operator<(const ServiceIdTuple& other) const {
        if (service != other.service) return service < other.service;
        return instance < other.instance;
    }
};

struct ServiceIdTupleHash {
    size_t operator()(const ServiceIdTuple& id) const {
        return (static_cast<size_t>(id.service) << 16) | id.instance;
    }
};

// Endpoint address
struct Endpoint {
    std::string address;
    uint16_t port;
    IpProtocol protocol;

    bool operator==(const Endpoint& other) const {
        return address == other.address &&
               port == other.port &&
               protocol == other.protocol;
    }
};

// Service configuration
struct ServiceConfig {
    ServiceId service_id;
    InstanceId instance_id;
    MajorVersion major_version;
    MinorVersion minor_version;
    std::chrono::milliseconds ttl{0xFFFFFF}; // Default "infinite"
    bool reliable;
    bool unreliable;
    uint16_t reliable_port{0};
    uint16_t unreliable_port{0};
};

// Subscription information
struct SubscriptionInfo {
    ServiceId service_id;
    InstanceId instance_id;
    EventgroupId eventgroup_id;
    MajorVersion major_version;
    uint32_t ttl;
    std::vector<Endpoint> endpoints;
};

// Event configuration
struct EventConfig {
    ServiceId service_id;
    InstanceId instance_id;
    EventId event_id;
    EventgroupId eventgroup_id;
    bool is_field;
    bool is_reliable;
};

// Method configuration
struct MethodConfig {
    ServiceId service_id;
    InstanceId instance_id;
    MethodId method_id;
    bool is_reliable;
    std::chrono::milliseconds timeout{5000};
};

// Payload data
using PayloadData = std::vector<uint8_t>;

// Forward declarations
class Message;
class Payload;
class Runtime;
class Application;
class ServiceBase;
class ServiceProxy;
class ServiceSkeleton;

using MessagePtr = std::shared_ptr<Message>;
using PayloadPtr = std::shared_ptr<Payload>;

// Callback types
using MessageHandler = std::function<void(MessagePtr&&)>;
using AvailabilityHandler = std::function<void(ServiceId, InstanceId, bool)>;
using SubscriptionHandler = std::function<bool(const SubscriptionInfo&)>;
using ErrorHandler = std::function<void(ReturnCode)>;
using TimerCallback = std::function<void()>;

}  // namespace msomeip
}  // namespace moss

#include "msomeip/sd/service_entry.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <arpa/inet.h>

namespace moss {
namespace msomeip {
namespace sd {

// ServiceEntry factory methods
ServiceEntry ServiceEntry::create_find_service(
    ServiceId service,
    InstanceId instance) {
    ServiceEntry entry;
    entry.type_ = SdEntryType::FIND_SERVICE;
    entry.service_id_ = service;
    entry.instance_id_ = instance;
    return entry;
}

ServiceEntry ServiceEntry::create_offer_service(
    ServiceId service,
    InstanceId instance,
    MajorVersion major_version,
    MinorVersion minor_version,
    uint32_t ttl) {
    ServiceEntry entry;
    entry.type_ = SdEntryType::OFFER_SERVICE;
    entry.service_id_ = service;
    entry.instance_id_ = instance;
    entry.major_version_ = major_version;
    entry.minor_version_ = minor_version;
    entry.ttl_ = ttl;
    return entry;
}

ServiceEntry ServiceEntry::create_subscribe_eventgroup(
    ServiceId service,
    InstanceId instance,
    EventgroupId eventgroup,
    MajorVersion major_version,
    uint32_t ttl) {
    ServiceEntry entry;
    entry.type_ = SdEntryType::SUBSCRIBE_EVENTGROUP;
    entry.service_id_ = service;
    entry.instance_id_ = instance;
    entry.minor_version_ = (static_cast<uint32_t>(major_version) << 24) | eventgroup;
    entry.major_version_ = major_version;
    entry.ttl_ = ttl;
    return entry;
}

ServiceEntry ServiceEntry::create_subscribe_eventgroup_ack(
    ServiceId service,
    InstanceId instance,
    EventgroupId eventgroup,
    MajorVersion major_version,
    uint32_t ttl) {
    ServiceEntry entry;
    entry.type_ = SdEntryType::SUBSCRIBE_EVENTGROUP_ACK;
    entry.service_id_ = service;
    entry.instance_id_ = instance;
    entry.minor_version_ = (static_cast<uint32_t>(major_version) << 24) | eventgroup;
    entry.major_version_ = major_version;
    entry.ttl_ = ttl;
    return entry;
}

ServiceEntry ServiceEntry::create_subscribe_eventgroup_nack(
    ServiceId service,
    InstanceId instance,
    EventgroupId eventgroup) {
    ServiceEntry entry;
    entry.type_ = SdEntryType::SUBSCRIBE_EVENTGROUP_ACK;
    entry.service_id_ = service;
    entry.instance_id_ = instance;
    entry.minor_version_ = eventgroup;
    entry.ttl_ = 0; // TTL = 0 means NACK
    return entry;
}

std::vector<uint8_t> ServiceEntry::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(ENTRY_SIZE);

    // Type
    result.push_back(static_cast<uint8_t>(type_));

    // Options indices
    result.push_back(index_first_option_);
    result.push_back(index_second_option_);

    // Number of options
    uint8_t num_options = (num_first_options_ & 0x0F) | ((num_second_options_ & 0x0F) << 4);
    result.push_back(num_options);

    // Service ID
    result.push_back(static_cast<uint8_t>(service_id_ >> 8));
    result.push_back(static_cast<uint8_t>(service_id_ & 0xFF));

    // Instance ID
    result.push_back(static_cast<uint8_t>(instance_id_ >> 8));
    result.push_back(static_cast<uint8_t>(instance_id_ & 0xFF));

    // Major version
    result.push_back(major_version_);

    // TTL
    result.push_back(static_cast<uint8_t>(ttl_ >> 16));
    result.push_back(static_cast<uint8_t>(ttl_ >> 8));
    result.push_back(static_cast<uint8_t>(ttl_ & 0xFF));

    // Minor version (or reserved for subscribe eventgroup)
    result.push_back(static_cast<uint8_t>(minor_version_ >> 24));
    result.push_back(static_cast<uint8_t>(minor_version_ >> 16));
    result.push_back(static_cast<uint8_t>(minor_version_ >> 8));
    result.push_back(static_cast<uint8_t>(minor_version_ & 0xFF));

    return result;
}

std::optional<ServiceEntry> ServiceEntry::deserialize(const uint8_t* data, size_t length) {
    if (length < ENTRY_SIZE) {
        return std::nullopt;
    }

    ServiceEntry entry;

    entry.type_ = static_cast<SdEntryType>(data[0]);
    entry.index_first_option_ = data[1];
    entry.index_second_option_ = data[2];
    entry.num_first_options_ = data[3] & 0x0F;
    entry.num_second_options_ = (data[3] >> 4) & 0x0F;

    entry.service_id_ = (static_cast<uint16_t>(data[4]) << 8) | data[5];
    entry.instance_id_ = (static_cast<uint16_t>(data[6]) << 8) | data[7];

    entry.major_version_ = data[8];

    entry.ttl_ = (static_cast<uint32_t>(data[9]) << 16) |
                 (static_cast<uint32_t>(data[10]) << 8) |
                 data[11];

    entry.minor_version_ = (static_cast<uint32_t>(data[12]) << 24) |
                           (static_cast<uint32_t>(data[13]) << 16) |
                           (static_cast<uint32_t>(data[14]) << 8) |
                           data[15];

    return entry;
}

EventgroupId ServiceEntry::get_eventgroup_id() const {
    return static_cast<EventgroupId>(minor_version_ & 0xFFFF);
}

void ServiceEntry::set_eventgroup_id(EventgroupId id) {
    minor_version_ = (minor_version_ & 0xFFFF0000) | id;
}

bool ServiceEntry::is_eventgroup_entry() const {
    return type_ == SdEntryType::SUBSCRIBE_EVENTGROUP ||
           type_ == SdEntryType::SUBSCRIBE_EVENTGROUP_ACK;
}

bool ServiceEntry::is_service_entry() const {
    return type_ == SdEntryType::FIND_SERVICE ||
           type_ == SdEntryType::OFFER_SERVICE;
}

// IPv4EndpointOption implementation
IPv4EndpointOption::IPv4EndpointOption(const std::string& address,
                                        uint16_t port,
                                        IpProtocol protocol)
    : port_(port), protocol_(protocol) {
    struct in_addr addr;
    if (inet_pton(AF_INET, address.c_str(), &addr) == 1) {
        std::memcpy(address_.data(), &addr, 4);
    }
}

std::vector<uint8_t> IPv4EndpointOption::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(OPTION_SIZE);

    // Length (after length field)
    result.push_back(0x00);
    result.push_back(0x09); // 9 bytes after length

    // Type
    result.push_back(static_cast<uint8_t>(SdOptionType::IPv4_ENDPOINT));

    // Reserved
    result.push_back(0x00);

    // IP address
    result.insert(result.end(), address_.begin(), address_.end());

    // Reserved
    result.push_back(0x00);

    // Protocol
    result.push_back(static_cast<uint8_t>(protocol_));

    // Port
    result.push_back(static_cast<uint8_t>(port_ >> 8));
    result.push_back(static_cast<uint8_t>(port_ & 0xFF));

    return result;
}

std::string IPv4EndpointOption::get_address() const {
    char buf[INET_ADDRSTRLEN];
    struct in_addr addr;
    std::memcpy(&addr, address_.data(), 4);
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
}

void IPv4EndpointOption::set_address(const std::string& address) {
    struct in_addr addr;
    if (inet_pton(AF_INET, address.c_str(), &addr) == 1) {
        std::memcpy(address_.data(), &addr, 4);
    }
}

// IPv4MulticastOption implementation
IPv4MulticastOption::IPv4MulticastOption(const std::string& address, uint16_t port)
    : port_(port) {
    struct in_addr addr;
    if (inet_pton(AF_INET, address.c_str(), &addr) == 1) {
        std::memcpy(address_.data(), &addr, 4);
    }
}

std::vector<uint8_t> IPv4MulticastOption::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(OPTION_SIZE);

    // Length
    result.push_back(0x00);
    result.push_back(0x09);

    // Type
    result.push_back(static_cast<uint8_t>(SdOptionType::IPv4_MULTICAST));

    // Reserved
    result.push_back(0x00);

    // IP address
    result.insert(result.end(), address_.begin(), address_.end());

    // Reserved
    result.push_back(0x00);

    // Reserved
    result.push_back(0x00);

    // Port
    result.push_back(static_cast<uint8_t>(port_ >> 8));
    result.push_back(static_cast<uint8_t>(port_ & 0xFF));

    return result;
}

std::string IPv4MulticastOption::get_address() const {
    char buf[INET_ADDRSTRLEN];
    struct in_addr addr;
    std::memcpy(&addr, address_.data(), 4);
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return std::string(buf);
}

void IPv4MulticastOption::set_address(const std::string& address) {
    struct in_addr addr;
    if (inet_pton(AF_INET, address.c_str(), &addr) == 1) {
        std::memcpy(address_.data(), &addr, 4);
    }
}

std::unique_ptr<SdOption> SdOption::deserialize(const uint8_t* data, size_t length) {
    if (length < 3) return nullptr;

    auto type = static_cast<SdOptionType>(data[2]);

    switch (type) {
        case SdOptionType::IPv4_ENDPOINT: {
            if (length < IPv4EndpointOption::OPTION_SIZE) return nullptr;
            auto opt = std::make_unique<IPv4EndpointOption>();
            char addr_str[INET_ADDRSTRLEN];
            struct in_addr addr;
            std::memcpy(&addr, data + 4, 4);
            inet_ntop(AF_INET, &addr, addr_str, sizeof(addr_str));
            opt->set_address(addr_str);
            opt->set_protocol(static_cast<IpProtocol>(data[9]));
            opt->set_port((static_cast<uint16_t>(data[10]) << 8) | data[11]);
            return opt;
        }
        case SdOptionType::IPv4_MULTICAST: {
            if (length < IPv4MulticastOption::OPTION_SIZE) return nullptr;
            auto opt = std::make_unique<IPv4MulticastOption>();
            char addr_str[INET_ADDRSTRLEN];
            struct in_addr addr;
            std::memcpy(&addr, data + 4, 4);
            inet_ntop(AF_INET, &addr, addr_str, sizeof(addr_str));
            opt->set_address(addr_str);
            opt->set_port((static_cast<uint16_t>(data[10]) << 8) | data[11]);
            return opt;
        }
        default:
            return nullptr;
    }
}

} // namespace sd
}  // namespace msomeip
}  // namespace moss

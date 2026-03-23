#pragma once

#include "msomeip/types.h"

namespace msomeip {
namespace sd {

// SOME/IP SD Entry header format
// +----------------------------+
// | Type (8 bits)              |
// +----------------------------+
// | Index 1st options (8 bits) |
// +----------------------------+
// | Index 2nd options (8 bits) |
// +----------------------------+
// | # of opt 1 (4 bits)        |
// | # of opt 2 (4 bits)        |
// +----------------------------+
// | Service ID (16 bits)       |
// +----------------------------+
// | Instance ID (16 bits)      |
// +----------------------------+
// | Major Version (8 bits)     |
// +----------------------------+
// | TTL (24 bits)              |
// +----------------------------+
// | Minor Version (32 bits)    |
// +----------------------------+

class ServiceEntry {
public:
    static constexpr size_t ENTRY_SIZE = 16;

    ServiceEntry() = default;

    // Factory methods
    static ServiceEntry create_find_service(
        ServiceId service,
        InstanceId instance = 0xFFFF);

    static ServiceEntry create_offer_service(
        ServiceId service,
        InstanceId instance,
        MajorVersion major_version,
        MinorVersion minor_version,
        uint32_t ttl);

    static ServiceEntry create_subscribe_eventgroup(
        ServiceId service,
        InstanceId instance,
        EventgroupId eventgroup,
        MajorVersion major_version,
        uint32_t ttl);

    static ServiceEntry create_subscribe_eventgroup_ack(
        ServiceId service,
        InstanceId instance,
        EventgroupId eventgroup,
        MajorVersion major_version,
        uint32_t ttl);

    static ServiceEntry create_subscribe_eventgroup_nack(
        ServiceId service,
        InstanceId instance,
        EventgroupId eventgroup);

    // Serialization
    std::vector<uint8_t> serialize() const;
    static std::optional<ServiceEntry> deserialize(const uint8_t* data, size_t length);

    // Getters/Setters
    SdEntryType get_type() const { return type_; }
    void set_type(SdEntryType type) { type_ = type; }

    ServiceId get_service_id() const { return service_id_; }
    void set_service_id(ServiceId id) { service_id_ = id; }

    InstanceId get_instance_id() const { return instance_id_; }
    void set_instance_id(InstanceId id) { instance_id_ = id; }

    MajorVersion get_major_version() const { return major_version_; }
    void set_major_version(MajorVersion version) { major_version_ = version; }

    MinorVersion get_minor_version() const { return minor_version_; }
    void set_minor_version(MinorVersion version) { minor_version_ = version; }

    uint32_t get_ttl() const { return ttl_; }
    void set_ttl(uint32_t ttl) { ttl_ = ttl; }

    EventgroupId get_eventgroup_id() const;
    void set_eventgroup_id(EventgroupId id);

    // Counter
    uint8_t get_counter() const { return counter_; }
    void set_counter(uint8_t counter) { counter_ = counter; }

    // Options indices
    uint8_t get_index_first_option() const { return index_first_option_; }
    void set_index_first_option(uint8_t idx) { index_first_option_ = idx; }

    uint8_t get_index_second_option() const { return index_second_option_; }
    void set_index_second_option(uint8_t idx) { index_second_option_ = idx; }

    uint8_t get_num_first_options() const { return num_first_options_; }
    void set_num_first_options(uint8_t num) { num_first_options_ = num; }

    uint8_t get_num_second_options() const { return num_second_options_; }
    void set_num_second_options(uint8_t num) { num_second_options_ = num; }

    // Utility
    bool is_eventgroup_entry() const;
    bool is_service_entry() const;

private:
    SdEntryType type_ = SdEntryType::FIND_SERVICE;
    uint8_t index_first_option_ = 0;
    uint8_t index_second_option_ = 0;
    uint8_t num_first_options_ = 0;
    uint8_t num_second_options_ = 0;
    ServiceId service_id_ = 0;
    InstanceId instance_id_ = 0;
    MajorVersion major_version_ = 0;
    uint32_t ttl_ = 0;
    MinorVersion minor_version_ = 0;
    uint8_t counter_ = 0; // For subscribe eventgroup entries
};

// SD Option types
enum class SdOptionType : uint8_t {
    CONFIGURATION = 0x01,
    LOAD_BALANCING = 0x02,
    IPv4_ENDPOINT = 0x04,
    IPv6_ENDPOINT = 0x06,
    IPv4_MULTICAST = 0x14,
    IPv6_MULTICAST = 0x16
};

// SD Option base class
class SdOption {
public:
    virtual ~SdOption() = default;

    virtual SdOptionType get_type() const = 0;
    virtual std::vector<uint8_t> serialize() const = 0;
    virtual size_t get_size() const = 0;

    static std::unique_ptr<SdOption> deserialize(const uint8_t* data, size_t length);
};

// IPv4 Endpoint Option
class IPv4EndpointOption : public SdOption {
public:
    static constexpr size_t OPTION_SIZE = 12;

    IPv4EndpointOption() = default;
    IPv4EndpointOption(const std::string& address, uint16_t port, IpProtocol protocol);

    SdOptionType get_type() const override { return SdOptionType::IPv4_ENDPOINT; }
    std::vector<uint8_t> serialize() const override;
    size_t get_size() const override { return OPTION_SIZE; }

    std::string get_address() const;
    void set_address(const std::string& address);

    uint16_t get_port() const { return port_; }
    void set_port(uint16_t port) { port_ = port; }

    IpProtocol get_protocol() const { return protocol_; }
    void set_protocol(IpProtocol protocol) { protocol_ = protocol; }

private:
    std::array<uint8_t, 4> address_ = {0, 0, 0, 0};
    uint16_t port_ = 0;
    IpProtocol protocol_ = IpProtocol::UDP;
};

// IPv4 Multicast Option
class IPv4MulticastOption : public SdOption {
public:
    static constexpr size_t OPTION_SIZE = 12;

    IPv4MulticastOption() = default;
    IPv4MulticastOption(const std::string& address, uint16_t port);

    SdOptionType get_type() const override { return SdOptionType::IPv4_MULTICAST; }
    std::vector<uint8_t> serialize() const override;
    size_t get_size() const override { return OPTION_SIZE; }

    std::string get_address() const;
    void set_address(const std::string& address);

    uint16_t get_port() const { return port_; }
    void set_port(uint16_t port) { port_ = port; }

private:
    std::array<uint8_t, 4> address_ = {0, 0, 0, 0};
    uint16_t port_ = 0;
    uint8_t reserved_ = 0;
};

} // namespace sd
} // namespace msomeip

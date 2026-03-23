#include <gtest/gtest.h>
#include "msomeip/sd/service_discovery.h"
#include "msomeip/sd/service_entry.h"

using namespace msomeip;
using namespace mmsomeip::sd;

class ServiceDiscoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

class ServiceEntryTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

// ServiceEntry Tests
TEST_F(ServiceEntryTest, CreateFindService) {
    auto entry = ServiceEntry::create_find_service(0x1234, 0x0001);

    EXPECT_EQ(entry.get_type(), SdEntryType::FIND_SERVICE);
    EXPECT_EQ(entry.get_service_id(), 0x1234);
    EXPECT_EQ(entry.get_instance_id(), 0x0001);
}

TEST_F(ServiceEntryTest, CreateOfferService) {
    auto entry = ServiceEntry::create_offer_service(
        0x1234, 0x0001, 1, 0, 0xFFFFFF);

    EXPECT_EQ(entry.get_type(), SdEntryType::OFFER_SERVICE);
    EXPECT_EQ(entry.get_service_id(), 0x1234);
    EXPECT_EQ(entry.get_instance_id(), 0x0001);
    EXPECT_EQ(entry.get_major_version(), 1);
    EXPECT_EQ(entry.get_minor_version(), 0);
    EXPECT_EQ(entry.get_ttl(), 0xFFFFFF);
}

TEST_F(ServiceEntryTest, CreateSubscribeEventgroup) {
    auto entry = ServiceEntry::create_subscribe_eventgroup(
        0x1234, 0x0001, 0x0001, 1, 0xFFFFFF);

    EXPECT_EQ(entry.get_type(), SdEntryType::SUBSCRIBE_EVENTGROUP);
    EXPECT_EQ(entry.get_service_id(), 0x1234);
    EXPECT_EQ(entry.get_instance_id(), 0x0001);
    EXPECT_EQ(entry.get_eventgroup_id(), 0x0001);
    EXPECT_EQ(entry.get_major_version(), 1);
    EXPECT_EQ(entry.get_ttl(), 0xFFFFFF);
}

TEST_F(ServiceEntryTest, SerializeDeserialize) {
    auto original = ServiceEntry::create_offer_service(
        0x1234, 0x5678, 1, 5, 0xFFFFFF);

    auto serialized = original.serialize();
    EXPECT_EQ(serialized.size(), ServiceEntry::ENTRY_SIZE);

    auto deserialized = ServiceEntry::deserialize(serialized.data(), serialized.size());
    ASSERT_TRUE(deserialized.has_value());

    EXPECT_EQ(deserialized->get_type(), original.get_type());
    EXPECT_EQ(deserialized->get_service_id(), original.get_service_id());
    EXPECT_EQ(deserialized->get_instance_id(), original.get_instance_id());
    EXPECT_EQ(deserialized->get_major_version(), original.get_major_version());
    EXPECT_EQ(deserialized->get_minor_version(), original.get_minor_version());
    EXPECT_EQ(deserialized->get_ttl(), original.get_ttl());
}

TEST_F(ServiceEntryTest, DeserializeTooShort) {
    uint8_t data[10] = {};
    auto result = ServiceEntry::deserialize(data, 10);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ServiceEntryTest, IsServiceEntry) {
    auto find_entry = ServiceEntry::create_find_service(0x1234);
    EXPECT_TRUE(find_entry.is_service_entry());
    EXPECT_FALSE(find_entry.is_eventgroup_entry());

    auto offer_entry = ServiceEntry::create_offer_service(0x1234, 0x0001, 1, 0, 0xFFFFFF);
    EXPECT_TRUE(offer_entry.is_service_entry());
    EXPECT_FALSE(offer_entry.is_eventgroup_entry());
}

TEST_F(ServiceEntryTest, IsEventgroupEntry) {
    auto sub_entry = ServiceEntry::create_subscribe_eventgroup(
        0x1234, 0x0001, 0x0001, 1, 0xFFFFFF);
    EXPECT_TRUE(sub_entry.is_eventgroup_entry());
    EXPECT_FALSE(sub_entry.is_service_entry());

    auto ack_entry = ServiceEntry::create_subscribe_eventgroup_ack(
        0x1234, 0x0001, 0x0001, 1, 0xFFFFFF);
    EXPECT_TRUE(ack_entry.is_eventgroup_entry());
    EXPECT_FALSE(ack_entry.is_service_entry());
}

// IPv4EndpointOption Tests
TEST_F(ServiceDiscoveryTest, IPv4EndpointOptionSerialize) {
    IPv4EndpointOption option("192.168.1.1", 30509, IpProtocol::UDP);

    EXPECT_EQ(option.get_type(), SdOptionType::IPv4_ENDPOINT);
    EXPECT_EQ(option.get_address(), "192.168.1.1");
    EXPECT_EQ(option.get_port(), 30509);
    EXPECT_EQ(option.get_protocol(), IpProtocol::UDP);

    auto serialized = option.serialize();
    EXPECT_EQ(serialized.size(), IPv4EndpointOption::OPTION_SIZE);
}

TEST_F(ServiceDiscoveryTest, IPv4EndpointOptionTcp) {
    IPv4EndpointOption option("10.0.0.1", 30490, IpProtocol::TCP);

    EXPECT_EQ(option.get_protocol(), IpProtocol::TCP);
}

TEST_F(ServiceDiscoveryTest, IPv4MulticastOption) {
    IPv4MulticastOption option("224.0.0.1", 30490);

    EXPECT_EQ(option.get_type(), SdOptionType::IPv4_MULTICAST);
    EXPECT_EQ(option.get_address(), "224.0.0.1");
    EXPECT_EQ(option.get_port(), 30490);

    auto serialized = option.serialize();
    EXPECT_EQ(serialized.size(), IPv4MulticastOption::OPTION_SIZE);
}

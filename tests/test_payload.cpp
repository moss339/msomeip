#include <gtest/gtest.h>
#include "msomeip/message/payload.h"

using namespace msomeip;

class PayloadTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
};

TEST_F(PayloadTest, DefaultConstructor) {
    Payload payload;
    EXPECT_EQ(payload.size(), 0);
    EXPECT_TRUE(payload.empty());
    EXPECT_EQ(payload.get_position(), 0);
}

TEST_F(PayloadTest, ConstructorWithData) {
    PayloadData data = {0x01, 0x02, 0x03, 0x04};
    Payload payload(data);

    EXPECT_EQ(payload.size(), 4);
    EXPECT_FALSE(payload.empty());
}

TEST_F(PayloadTest, ConstructorWithPointer) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    Payload payload(data, sizeof(data));

    EXPECT_EQ(payload.size(), 4);
}

TEST_F(PayloadTest, AppendUint8) {
    Payload payload;
    payload.append(static_cast<uint8_t>(0xAB));

    EXPECT_EQ(payload.size(), 1);

    uint8_t value;
    EXPECT_TRUE(payload.get_uint8(value));
    EXPECT_EQ(value, 0xAB);
}

TEST_F(PayloadTest, AppendUint16BigEndian) {
    Payload payload;
    payload.append(static_cast<uint16_t>(0x1234), true);

    EXPECT_EQ(payload.size(), 2);

    uint16_t value;
    EXPECT_TRUE(payload.get_uint16(value, true));
    EXPECT_EQ(value, 0x1234);
}

TEST_F(PayloadTest, AppendUint16LittleEndian) {
    Payload payload;
    payload.append(static_cast<uint16_t>(0x1234), false);

    uint16_t value;
    EXPECT_TRUE(payload.get_uint16(value, false));
    EXPECT_EQ(value, 0x1234);
}

TEST_F(PayloadTest, AppendUint32BigEndian) {
    Payload payload;
    payload.append(static_cast<uint32_t>(0x12345678), true);

    EXPECT_EQ(payload.size(), 4);

    uint32_t value;
    EXPECT_TRUE(payload.get_uint32(value, true));
    EXPECT_EQ(value, 0x12345678);
}

TEST_F(PayloadTest, AppendUint64BigEndian) {
    Payload payload;
    payload.append(static_cast<uint64_t>(0x1234567890ABCDEF), true);

    EXPECT_EQ(payload.size(), 8);

    uint64_t value;
    EXPECT_TRUE(payload.get_uint64(value, true));
    EXPECT_EQ(value, 0x1234567890ABCDEF);
}

TEST_F(PayloadTest, AppendBytes) {
    Payload payload;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    payload.append(data, sizeof(data));

    EXPECT_EQ(payload.size(), 4);

    std::vector<uint8_t> result;
    payload.set_position(0);
    EXPECT_TRUE(payload.get_bytes(result, 4));
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 0x01);
    EXPECT_EQ(result[3], 0x04);
}

TEST_F(PayloadTest, AppendVector) {
    Payload payload;
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    payload.append(data);

    EXPECT_EQ(payload.size(), 4);
}

TEST_F(PayloadTest, GetUint8OutOfBounds) {
    Payload payload;
    uint8_t value;
    EXPECT_FALSE(payload.get_uint8(value));
}

TEST_F(PayloadTest, GetUint16OutOfBounds) {
    Payload payload;
    payload.append(static_cast<uint8_t>(0x01));

    uint16_t value;
    EXPECT_FALSE(payload.get_uint16(value));
}

TEST_F(PayloadTest, GetUint32OutOfBounds) {
    Payload payload;
    payload.append(static_cast<uint8_t>(0x01));

    uint32_t value;
    EXPECT_FALSE(payload.get_uint32(value));
}

TEST_F(PayloadTest, GetUint64OutOfBounds) {
    Payload payload;
    payload.append(static_cast<uint8_t>(0x01));

    uint64_t value;
    EXPECT_FALSE(payload.get_uint64(value));
}

TEST_F(PayloadTest, SetPosition) {
    Payload payload;
    payload.append(static_cast<uint16_t>(0x1234));
    payload.append(static_cast<uint16_t>(0x5678));

    // Note: append doesn't change position_, only read operations do
    EXPECT_EQ(payload.get_position(), 0);

    // Simulate reading
    uint16_t value;
    EXPECT_TRUE(payload.get_uint16(value));
    EXPECT_EQ(payload.get_position(), 2);

    payload.set_position(0);
    EXPECT_EQ(payload.get_position(), 0);

    EXPECT_TRUE(payload.get_uint16(value));
    EXPECT_EQ(value, 0x1234);
}

TEST_F(PayloadTest, SetPositionOutOfBounds) {
    Payload payload;
    payload.append(static_cast<uint16_t>(0x1234));

    // Move position to 2 by reading
    uint16_t value;
    EXPECT_TRUE(payload.get_uint16(value));
    EXPECT_EQ(payload.get_position(), 2);

    // Try to set out of bounds - should not change
    payload.set_position(100);
    EXPECT_EQ(payload.get_position(), 2); // Should not change
}

TEST_F(PayloadTest, Clear) {
    Payload payload;
    payload.append(static_cast<uint16_t>(0x1234));
    payload.set_position(1);

    EXPECT_EQ(payload.size(), 2);
    EXPECT_EQ(payload.get_position(), 1);

    payload.clear();

    EXPECT_EQ(payload.size(), 0);
    EXPECT_EQ(payload.get_position(), 0);
    EXPECT_TRUE(payload.empty());
}

TEST_F(PayloadTest, Copy) {
    Payload payload;
    payload.append(static_cast<uint16_t>(0x1234));
    payload.append(static_cast<uint16_t>(0x5678));

    auto copy = payload.copy();
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->size(), 4);
    EXPECT_EQ(copy->get_position(), 0);
}

TEST_F(PayloadTest, GetString) {
    Payload payload;
    payload.append(static_cast<uint32_t>(5)); // Length
    payload.append(reinterpret_cast<const uint8_t*>("hello"), 5);

    std::string value;
    EXPECT_TRUE(payload.get_string(value));
    EXPECT_EQ(value, "hello");
}

TEST_F(PayloadTest, GetStringTooShort) {
    Payload payload;
    payload.append(static_cast<uint32_t>(10)); // Length says 10, but only 3 bytes
    payload.append(reinterpret_cast<const uint8_t*>("abc"), 3);

    std::string value;
    EXPECT_FALSE(payload.get_string(value));
}

TEST_F(PayloadTest, MultipleReads) {
    Payload payload;
    payload.append(static_cast<uint8_t>(0x01));
    payload.append(static_cast<uint8_t>(0x02));
    payload.append(static_cast<uint8_t>(0x03));
    payload.append(static_cast<uint8_t>(0x04));

    uint8_t v1, v2, v3, v4;
    EXPECT_TRUE(payload.get_uint8(v1));
    EXPECT_TRUE(payload.get_uint8(v2));
    EXPECT_TRUE(payload.get_uint8(v3));
    EXPECT_TRUE(payload.get_uint8(v4));

    EXPECT_EQ(v1, 0x01);
    EXPECT_EQ(v2, 0x02);
    EXPECT_EQ(v3, 0x03);
    EXPECT_EQ(v4, 0x04);

    uint8_t v5;
    EXPECT_FALSE(payload.get_uint8(v5)); // End of data
}

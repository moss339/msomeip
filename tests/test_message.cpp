#include <gtest/gtest.h>
#include "msomeip/message/message.h"
#include "msomeip/message/payload.h"

using namespace msomeip;

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }
};

TEST_F(MessageTest, CreateRequest) {
    auto msg = Message::create_request(0x1234, 0x0001, 0x0001, 0x0001, false);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->get_service_id(), 0x1234);
    EXPECT_EQ(msg->get_instance_id(), 0x0001);
    EXPECT_EQ(msg->get_method_id(), 0x0001);
    EXPECT_EQ(msg->get_client_id(), 0x0001);
    EXPECT_EQ(msg->get_message_type(), MessageType::REQUEST);
    EXPECT_TRUE(msg->is_request());
    EXPECT_FALSE(msg->is_event());
}

TEST_F(MessageTest, CreateNotification) {
    auto msg = Message::create_notification(0x1234, 0x0001, 0x8001);
    ASSERT_NE(msg, nullptr);
    EXPECT_EQ(msg->get_service_id(), 0x1234);
    EXPECT_EQ(msg->get_instance_id(), 0x0001);
    EXPECT_EQ(msg->get_method_id(), 0x8001);
    EXPECT_EQ(msg->get_message_type(), MessageType::NOTIFICATION);
    EXPECT_FALSE(msg->is_request());
    EXPECT_TRUE(msg->is_event());
}

TEST_F(MessageTest, CreateResponse) {
    auto request = Message::create_request(0x1234, 0x0001, 0x0001, 0x0001, false);
    auto response = Message::create_response(*request);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->get_service_id(), 0x1234);
    EXPECT_EQ(response->get_method_id(), 0x0001);
    EXPECT_EQ(response->get_client_id(), 0x0001);
    EXPECT_EQ(response->get_message_type(), MessageType::RESPONSE);
    EXPECT_TRUE(response->is_response_type());
}

TEST_F(MessageTest, CreateErrorResponse) {
    auto request = Message::create_request(0x1234, 0x0001, 0x0001, 0x0001, false);
    auto response = Message::create_error_response(*request, ReturnCode::E_UNKNOWN_SERVICE);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->get_message_type(), MessageType::ERROR);
    EXPECT_EQ(response->get_return_code(), ReturnCode::E_UNKNOWN_SERVICE);
}

TEST_F(MessageTest, SerializeDeserialize) {
    auto original = Message::create_request(0x1234, 0x0001, 0x0001, 0x0001, false);
    original->set_session_id(0x1234);
    original->set_interface_version(0x01);

    PayloadData payload_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    original->set_payload(payload_data);

    auto serialized = original->serialize();
    EXPECT_EQ(serialized.size(), Message::HEADER_SIZE + payload_data.size());

    auto deserialized = Message::deserialize(serialized.data(), serialized.size());
    ASSERT_TRUE(deserialized.has_value());

    auto& msg = deserialized.value();
    EXPECT_EQ(msg->get_service_id(), original->get_service_id());
    EXPECT_EQ(msg->get_method_id(), original->get_method_id());
    EXPECT_EQ(msg->get_client_id(), original->get_client_id());
    EXPECT_EQ(msg->get_session_id(), original->get_session_id());
    EXPECT_EQ(msg->get_interface_version(), original->get_interface_version());
    EXPECT_EQ(msg->get_message_type(), original->get_message_type());
    EXPECT_EQ(msg->get_return_code(), original->get_return_code());
    ASSERT_NE(msg->get_payload(), nullptr);
    EXPECT_EQ(msg->get_payload()->size(), payload_data.size());
}

TEST_F(MessageTest, DeserializeTooShort) {
    uint8_t data[10] = {};
    auto result = Message::deserialize(data, 10);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageTest, DeserializeInvalidProtocolVersion) {
    uint8_t data[Message::HEADER_SIZE] = {};
    data[0] = 0x12; // Service ID
    data[1] = 0x34;
    data[2] = 0x00; // Method ID
    data[3] = 0x01;
    data[4] = 0x00; // Length = 0
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x00;
    data[8] = 0x00; // Client ID
    data[9] = 0x01;
    data[10] = 0x00; // Session ID
    data[11] = 0x01;
    data[12] = 0xFF; // Invalid protocol version
    data[13] = 0x01;
    data[14] = 0x00;
    data[15] = 0x00;

    auto result = Message::deserialize(data, sizeof(data));
    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageTest, SetGetPayload) {
    auto msg = Message::create_request(0x1234, 0x0001, 0x0001, 0x0001, false);

    PayloadData data = {0x11, 0x22, 0x33, 0x44};
    msg->set_payload(data);

    ASSERT_NE(msg->get_payload(), nullptr);
    EXPECT_EQ(msg->get_payload()->size(), 4);
    EXPECT_EQ(msg->get_length(), 4);
}

TEST_F(MessageTest, MessageIdOperations) {
    auto msg = Message::create_request(0x1234, 0x0001, 0x5678, 0x0001, false);

    EXPECT_EQ(msg->get_message_id(), 0x12345678);

    msg->set_message_id(0xABCD1234);
    EXPECT_EQ(msg->get_service_id(), 0xABCD);
    EXPECT_EQ(msg->get_method_id(), 0x1234);
}

TEST_F(MessageTest, RequestIdOperations) {
    auto msg = Message::create_request(0x1234, 0x0001, 0x0001, 0x0001, false);
    msg->set_session_id(0x5678);

    EXPECT_EQ(msg->get_request_id(), 0x00015678);

    msg->set_request_id(0x1234ABCD);
    EXPECT_EQ(msg->get_client_id(), 0x1234);
    EXPECT_EQ(msg->get_session_id(), 0xABCD);
}

TEST_F(MessageTest, IsValid) {
    auto msg = Message::create_request(0x1234, 0x0001, 0x0001, 0x0001, false);
    EXPECT_TRUE(msg->is_valid());

    msg->set_protocol_version(0xFF);
    EXPECT_FALSE(msg->is_valid());
}

TEST_F(MessageTest, ToString) {
    auto msg = Message::create_request(0x1234, 0x0001, 0x0001, 0x0001, false);
    msg->set_session_id(0x1234);

    std::string str = msg->to_string();
    EXPECT_NE(str.find("SID=1234"), std::string::npos);
    EXPECT_NE(str.find("IID=1"), std::string::npos);
    EXPECT_NE(str.find("SSID=1234"), std::string::npos);
}

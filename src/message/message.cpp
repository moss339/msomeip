#include "msomeip/message/message.h"
#include <cstring>
#include <sstream>
#include <iomanip>

namespace moss {
namespace msomeip {

Message::Message() = default;

MessagePtr Message::create_request(
    ServiceId service,
    InstanceId instance,
    MethodId method,
    ClientId client,
    bool reliable) {

    auto msg = std::make_shared<Message>();
    msg->set_service_id(service);
    msg->set_instance_id(instance);
    msg->set_method_id(method);
    msg->set_client_id(client);
    msg->set_message_type(MessageType::REQUEST);
    msg->set_reliable(reliable);
    msg->set_payload(std::make_shared<Payload>());
    return msg;
}

MessagePtr Message::create_notification(
    ServiceId service,
    InstanceId instance,
    EventId event) {

    auto msg = std::make_shared<Message>();
    msg->set_service_id(service);
    msg->set_instance_id(instance);
    msg->set_method_id(event | 0x8000); // Event bit
    msg->set_message_type(MessageType::NOTIFICATION);
    msg->set_payload(std::make_shared<Payload>());
    return msg;
}

MessagePtr Message::create_response(const Message& request) {
    auto msg = std::make_shared<Message>();
    msg->set_service_id(request.get_service_id());
    msg->set_instance_id(request.get_instance_id());
    msg->set_method_id(request.get_method_id());
    msg->set_client_id(request.get_client_id());
    msg->set_session_id(request.get_session_id());
    msg->set_message_type(MessageType::RESPONSE);
    msg->set_reliable(request.is_reliable());
    msg->set_payload(std::make_shared<Payload>());
    return msg;
}

MessagePtr Message::create_error_response(
    const Message& request,
    ReturnCode code) {

    auto msg = create_response(request);
    msg->set_message_type(MessageType::ERROR);
    msg->set_return_code(code);
    return msg;
}

std::optional<MessagePtr> Message::deserialize(const uint8_t* data, size_t length) {
    if (length < HEADER_SIZE) {
        return std::nullopt;
    }

    auto msg = std::make_shared<Message>();

    // Message ID (Service ID + Method ID)
    msg->service_id_ = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    msg->method_id_ = (static_cast<uint16_t>(data[2]) << 8) | data[3];

    // Length (after header)
    uint32_t payload_length = (static_cast<uint32_t>(data[4]) << 24) |
                              (static_cast<uint32_t>(data[5]) << 16) |
                              (static_cast<uint32_t>(data[6]) << 8) |
                              data[7];

    // Request ID (Client ID + Session ID)
    msg->client_id_ = (static_cast<uint16_t>(data[8]) << 8) | data[9];
    msg->session_id_ = (static_cast<uint16_t>(data[10]) << 8) | data[11];

    // Protocol and Interface versions
    msg->protocol_version_ = data[12];
    msg->interface_version_ = data[13];

    // Message type and Return code
    msg->message_type_ = static_cast<MessageType>(data[14]);
    msg->return_code_ = static_cast<ReturnCode>(data[15]);

    // Validate header
    if (msg->protocol_version_ != SOMEIP_PROTOCOL_VERSION) {
        return std::nullopt;
    }

    // Check if total length matches
    if (length < HEADER_SIZE + payload_length) {
        return std::nullopt;
    }

    // Extract payload
    if (payload_length > 0) {
        PayloadData payload_data(data + HEADER_SIZE, data + HEADER_SIZE + payload_length);
        msg->payload_ = std::make_shared<Payload>(std::move(payload_data));
    } else {
        msg->payload_ = std::make_shared<Payload>();
    }

    return msg;
}

MessageId Message::get_message_id() const {
    return (static_cast<MessageId>(service_id_) << 16) | method_id_;
}

void Message::set_message_id(MessageId id) {
    service_id_ = static_cast<ServiceId>(id >> 16);
    method_id_ = static_cast<MethodId>(id & 0xFFFF);
}

uint32_t Message::get_length() const {
    return payload_ ? static_cast<uint32_t>(payload_->size()) : 0;
}

RequestId Message::get_request_id() const {
    return (static_cast<RequestId>(client_id_) << 16) | session_id_;
}

void Message::set_request_id(RequestId id) {
    client_id_ = static_cast<ClientId>(id >> 16);
    session_id_ = static_cast<SessionId>(id & 0xFFFF);
}

void Message::set_payload(const PayloadData& data) {
    payload_ = std::make_shared<Payload>(data);
}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(HEADER_SIZE + get_length());

    // Message ID (Service ID + Method ID)
    result.push_back(static_cast<uint8_t>(service_id_ >> 8));
    result.push_back(static_cast<uint8_t>(service_id_ & 0xFF));
    result.push_back(static_cast<uint8_t>(method_id_ >> 8));
    result.push_back(static_cast<uint8_t>(method_id_ & 0xFF));

    // Length (payload length only)
    uint32_t payload_len = get_length();
    result.push_back(static_cast<uint8_t>(payload_len >> 24));
    result.push_back(static_cast<uint8_t>(payload_len >> 16));
    result.push_back(static_cast<uint8_t>(payload_len >> 8));
    result.push_back(static_cast<uint8_t>(payload_len & 0xFF));

    // Request ID (Client ID + Session ID)
    result.push_back(static_cast<uint8_t>(client_id_ >> 8));
    result.push_back(static_cast<uint8_t>(client_id_ & 0xFF));
    result.push_back(static_cast<uint8_t>(session_id_ >> 8));
    result.push_back(static_cast<uint8_t>(session_id_ & 0xFF));

    // Protocol and Interface versions
    result.push_back(protocol_version_);
    result.push_back(interface_version_);

    // Message type and Return code
    result.push_back(static_cast<uint8_t>(message_type_));
    result.push_back(static_cast<uint8_t>(return_code_));

    // Payload
    if (payload_) {
        const auto& data = payload_->get_data();
        result.insert(result.end(), data.begin(), data.end());
    }

    return result;
}

size_t Message::get_size() const {
    return HEADER_SIZE + get_length();
}

bool Message::is_valid() const {
    return protocol_version_ == SOMEIP_PROTOCOL_VERSION &&
           (message_type_ <= MessageType::ERROR ||
            message_type_ >= MessageType::TP_REQUEST);
}

std::string Message::to_string() const {
    std::stringstream ss;
    ss << "Message["
       << "SID=" << std::hex << service_id_
       << ",IID=" << instance_id_
       << ",MID=" << method_id_
       << ",CID=" << client_id_
       << ",SSID=" << session_id_
       << ",Type=" << static_cast<int>(message_type_)
       << ",RC=" << static_cast<int>(return_code_)
       << ",Len=" << std::dec << get_length()
       << "]";
    return ss.str();
}

}  // namespace msomeip
}  // namespace moss

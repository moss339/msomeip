#pragma once

#include "msomeip/types.h"
#include "msomeip/message/payload.h"

namespace moss {
namespace msomeip {

// SOME/IP message header structure
// +----------------------------+
// | Message ID (32 bits)       |
// |   - Service ID (16 bits)   |
// |   - Method ID (16 bits)    |
// +----------------------------+
// | Length (32 bits)           |
// +----------------------------+
// | Request ID (32 bits)       |
// |   - Client ID (16 bits)    |
// |   - Session ID (16 bits)   |
// +----------------------------+
// | Protocol Version (8 bits)  |
// +----------------------------+
// | Interface Version (8 bits) |
// +----------------------------+
// | Message Type (8 bits)      |
// +----------------------------+
// | Return Code (8 bits)       |
// +----------------------------+
// | Payload (variable)         |
// +----------------------------+

class Message {
public:
    // Static header size (without payload)
    static constexpr size_t HEADER_SIZE = 16;

    // Factory methods
    static MessagePtr create_request(
        ServiceId service,
        InstanceId instance,
        MethodId method,
        ClientId client,
        bool reliable = false);

    static MessagePtr create_notification(
        ServiceId service,
        InstanceId instance,
        EventId event);

    static MessagePtr create_response(const Message& request);

    static MessagePtr create_error_response(
        const Message& request,
        ReturnCode code);

    static std::optional<MessagePtr> deserialize(const uint8_t* data, size_t length);

    // Constructor
    Message();
    ~Message() = default;

    // Non-copyable but movable
    Message(const Message&) = delete;
    Message& operator=(const Message&) = delete;
    Message(Message&&) = default;
    Message& operator=(Message&&) = default;

    // Header fields
    MessageId get_message_id() const;
    void set_message_id(MessageId id);

    ServiceId get_service_id() const { return service_id_; }
    void set_service_id(ServiceId id) { service_id_ = id; }

    MethodId get_method_id() const { return method_id_; }
    void set_method_id(MethodId id) { method_id_ = id; }

    // Is this a request/response message (vs event)?
    bool is_request() const { return (method_id_ & 0x8000) == 0; }
    bool is_event() const { return (method_id_ & 0x8000) != 0; }

    // Length field
    uint32_t get_length() const;

    // Request ID
    RequestId get_request_id() const;
    void set_request_id(RequestId id);

    ClientId get_client_id() const { return client_id_; }
    void set_client_id(ClientId id) { client_id_ = id; }

    SessionId get_session_id() const { return session_id_; }
    void set_session_id(SessionId id) { session_id_ = id; }

    // Protocol fields
    uint8_t get_protocol_version() const { return protocol_version_; }
    void set_protocol_version(uint8_t version) { protocol_version_ = version; }

    uint8_t get_interface_version() const { return interface_version_; }
    void set_interface_version(uint8_t version) { interface_version_ = version; }

    MessageType get_message_type() const { return message_type_; }
    void set_message_type(MessageType type) { message_type_ = type; }

    bool is_request_type() const {
        return message_type_ == MessageType::REQUEST ||
               message_type_ == MessageType::REQUEST_NO_RETURN ||
               message_type_ == MessageType::TP_REQUEST ||
               message_type_ == MessageType::TP_REQUEST_NO_RETURN;
    }

    bool is_response_type() const {
        return message_type_ == MessageType::RESPONSE ||
               message_type_ == MessageType::TP_RESPONSE ||
               message_type_ == MessageType::ERROR ||
               message_type_ == MessageType::TP_ERROR;
    }

    ReturnCode get_return_code() const { return return_code_; }
    void set_return_code(ReturnCode code) { return_code_ = code; }

    // Payload
    PayloadPtr get_payload() const { return payload_; }
    void set_payload(PayloadPtr payload) { payload_ = std::move(payload); }
    void set_payload(const PayloadData& data);

    // Instance (for internal routing)
    InstanceId get_instance_id() const { return instance_id_; }
    void set_instance_id(InstanceId id) { instance_id_ = id; }

    // Reliable transport flag
    bool is_reliable() const { return reliable_; }
    void set_reliable(bool reliable) { reliable_ = reliable; }

    // Serialization
    std::vector<uint8_t> serialize() const;
    size_t get_size() const;

    // Utility
    bool is_valid() const;

    std::string to_string() const;

private:
    // Header fields
    ServiceId service_id_ = 0;
    MethodId method_id_ = 0;
    ClientId client_id_ = 0;
    SessionId session_id_ = 0;
    uint8_t protocol_version_ = SOMEIP_PROTOCOL_VERSION;
    uint8_t interface_version_ = SOMEIP_INTERFACE_VERSION;
    MessageType message_type_ = MessageType::REQUEST;
    ReturnCode return_code_ = ReturnCode::E_OK;

    // Internal routing info
    InstanceId instance_id_ = 0xFFFF;
    bool reliable_ = false;

    PayloadPtr payload_;
};

}  // namespace msomeip
}  // namespace moss

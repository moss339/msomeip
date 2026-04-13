#include "msomeip/tp/tp_segmenter.h"
#include "msomeip/tp/tp_types.h"

#include <cstring>

namespace moss {
namespace msomeip {
namespace tp {

bool TpSegmenter::needs_segmentation(size_t payload_size, size_t max_segment_size) {
    return payload_size > max_segment_size;
}

size_t TpSegmenter::get_segment_count(size_t payload_size, size_t max_segment_size) {
    if (payload_size == 0) return 1;
    return (payload_size + max_segment_size - 1) / max_segment_size;
}

std::vector<MessagePtr> TpSegmenter::segment(const Message& original,
                                               size_t max_segment_size) {
    std::vector<MessagePtr> segments;

    const auto& payload = original.get_payload();
    if (!payload || payload->empty()) {
        // Empty message, no segmentation needed - create a new message
        auto msg = std::make_shared<Message>();
        msg->set_service_id(original.get_service_id());
        msg->set_instance_id(original.get_instance_id());
        msg->set_method_id(original.get_method_id());
        msg->set_client_id(original.get_client_id());
        msg->set_session_id(original.get_session_id());
        msg->set_protocol_version(original.get_protocol_version());
        msg->set_interface_version(original.get_interface_version());
        msg->set_message_type(original.get_message_type());
        msg->set_return_code(original.get_return_code());
        segments.push_back(msg);
        return segments;
    }

    size_t payload_size = payload->size();
    if (!needs_segmentation(payload_size, max_segment_size)) {
        // No segmentation needed - create a new message with payload copy
        auto msg = std::make_shared<Message>();
        msg->set_service_id(original.get_service_id());
        msg->set_instance_id(original.get_instance_id());
        msg->set_method_id(original.get_method_id());
        msg->set_client_id(original.get_client_id());
        msg->set_session_id(original.get_session_id());
        msg->set_protocol_version(original.get_protocol_version());
        msg->set_interface_version(original.get_interface_version());
        msg->set_message_type(original.get_message_type());
        msg->set_return_code(original.get_return_code());
        msg->set_payload(payload->get_data());
        segments.push_back(msg);
        return segments;
    }

    // Determine TP message type
    MessageType tp_type;
    switch (original.get_message_type()) {
        case MessageType::REQUEST:
            tp_type = MessageType::TP_REQUEST;
            break;
        case MessageType::REQUEST_NO_RETURN:
            tp_type = MessageType::TP_REQUEST_NO_RETURN;
            break;
        case MessageType::NOTIFICATION:
            tp_type = MessageType::TP_NOTIFICATION;
            break;
        case MessageType::RESPONSE:
            tp_type = MessageType::TP_RESPONSE;
            break;
        case MessageType::ERROR:
            tp_type = MessageType::TP_ERROR;
            break;
        default:
            // Unknown type, return empty
            return segments;
    }

    // Create segments
    size_t offset = 0;
    size_t segment_count = get_segment_count(payload_size, max_segment_size);

    for (size_t i = 0; i < segment_count; ++i) {
        size_t segment_payload_size = std::min(max_segment_size, payload_size - offset);
        bool is_last = (offset + segment_payload_size >= payload_size);

        // Create segment message
        auto segment = std::make_shared<Message>();
        segment->set_service_id(original.get_service_id());
        segment->set_instance_id(original.get_instance_id());
        segment->set_method_id(original.get_method_id());
        segment->set_client_id(original.get_client_id());
        segment->set_session_id(original.get_session_id());
        segment->set_protocol_version(original.get_protocol_version());
        segment->set_interface_version(original.get_interface_version());
        segment->set_message_type(tp_type);
        segment->set_return_code(original.get_return_code());

        // Build payload: TP header (4 bytes) + segment data
        std::vector<uint8_t> segment_payload;
        segment_payload.reserve(4 + segment_payload_size);

        // Add TP header
        TpHeader tp_header(static_cast<uint32_t>(offset), !is_last);
        uint32_t tp_header_net = tp_header.to_network();
        const uint8_t* tp_bytes = reinterpret_cast<const uint8_t*>(&tp_header_net);
        segment_payload.insert(segment_payload.end(), tp_bytes, tp_bytes + 4);

        // Add segment data
        const uint8_t* data = payload->data() + offset;
        segment_payload.insert(segment_payload.end(), data, data + segment_payload_size);

        segment->set_payload(segment_payload);

        segments.push_back(segment);
        offset += segment_payload_size;
    }

    return segments;
}

} // namespace tp
} // namespace msomeip
} // namespace moss

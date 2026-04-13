#include "msomeip/tp/tp_reassembler.h"
#include "msomeip/tp/tp_types.h"

#include <cstring>
#include <algorithm>

namespace moss {
namespace msomeip {
namespace tp {

TpReassembler::TpReassembler(std::chrono::milliseconds timeout)
    : timeout_(timeout) {}

bool TpReassembler::add_segment(MessagePtr segment) {
    if (!segment || !segment->is_tp_message()) {
        return false;
    }

    const auto& payload = segment->get_payload();
    if (!payload || payload->size() < 4) {
        return false;
    }

    // Parse TP header
    uint32_t tp_header_net;
    std::memcpy(&tp_header_net, payload->data(), 4);
    TpHeader tp_header = TpHeader::from_network(tp_header_net);

    uint32_t request_id = segment->get_request_id();
    uint32_t offset = tp_header.offset;
    bool is_last = !tp_header.more_flag;

    std::lock_guard<std::mutex> lock(mutex_);

    // Find or create reassembly buffer
    auto& buffer = buffers_[request_id];

    if (buffer.segments.empty()) {
        // First segment, initialize buffer
        buffer.service_id = segment->get_service_id();
        buffer.instance_id = segment->get_instance_id();
        buffer.method_id = segment->get_method_id();
        buffer.client_id = segment->get_client_id();
        buffer.interface_version = segment->get_interface_version();
        buffer.return_code = segment->get_return_code();
        buffer.message_type = segment->get_message_type();
        buffer.total_size = 0;
        buffer.first_segment_time = std::chrono::steady_clock::now();
        buffer.has_last_segment = false;
    }

    // Store segment data (skip TP header)
    const auto& payload_data = payload->get_data();
    std::vector<uint8_t> segment_data(payload_data.begin() + 4, payload_data.end());
    buffer.segments[offset] = std::move(segment_data);

    // Update total size if this is the last segment
    if (is_last) {
        buffer.total_size = offset + segment_data.size();
        buffer.has_last_segment = true;
    }

    // Check if complete
    return is_complete_unlocked(request_id);
}

bool TpReassembler::is_complete(uint32_t request_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_complete_unlocked(request_id);
}

bool TpReassembler::is_complete_unlocked(uint32_t request_id) const {
    auto it = buffers_.find(request_id);
    if (it == buffers_.end()) {
        return false;
    }

    const auto& buffer = it->second;
    if (!buffer.has_last_segment) {
        return false;
    }

    // Check if we have all segments from 0 to total_size
    size_t expected_offset = 0;
    for (const auto& [offset, data] : buffer.segments) {
        if (offset != expected_offset) {
            return false;  // Gap detected
        }
        expected_offset += data.size();
    }

    return expected_offset == buffer.total_size;
}

MessagePtr TpReassembler::reassemble(uint32_t request_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = buffers_.find(request_id);
    if (it == buffers_.end() || !is_complete_unlocked(request_id)) {
        return nullptr;
    }

    auto& buffer = it->second;

    // Convert TP message type back to original
    MessageType original_type = MessageType::REQUEST;
    switch (buffer.message_type) {
        case MessageType::TP_REQUEST:
            original_type = MessageType::REQUEST;
            break;
        case MessageType::TP_REQUEST_NO_RETURN:
            original_type = MessageType::REQUEST_NO_RETURN;
            break;
        case MessageType::TP_NOTIFICATION:
            original_type = MessageType::NOTIFICATION;
            break;
        case MessageType::TP_RESPONSE:
            original_type = MessageType::RESPONSE;
            break;
        case MessageType::TP_ERROR:
            original_type = MessageType::ERROR;
            break;
        default:
            original_type = MessageType::REQUEST;
            break;
    }

    // Build reassembled payload
    std::vector<uint8_t> full_payload;
    full_payload.reserve(buffer.total_size);

    for (const auto& [offset, data] : buffer.segments) {
        full_payload.insert(full_payload.end(), data.begin(), data.end());
    }

    // Create reassembled message
    auto message = std::make_shared<Message>();
    message->set_service_id(buffer.service_id);
    message->set_instance_id(buffer.instance_id);
    message->set_method_id(buffer.method_id);
    message->set_client_id(buffer.client_id);
    message->set_session_id(request_id & 0xFFFF);  // Extract session from request_id
    message->set_protocol_version(0x01);
    message->set_interface_version(buffer.interface_version);
    message->set_message_type(original_type);
    message->set_return_code(buffer.return_code);
    message->set_payload(full_payload);

    // Remove buffer
    buffers_.erase(it);

    return message;
}

void TpReassembler::cleanup_expired() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    for (auto it = buffers_.begin(); it != buffers_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.first_segment_time);
        if (elapsed > timeout_) {
            it = buffers_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t TpReassembler::pending_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffers_.size();
}

} // namespace tp
} // namespace msomeip
} // namespace moss

#pragma once

#include "msomeip/tp/tp_types.h"
#include "msomeip/message/message.h"

#include <map>
#include <vector>
#include <chrono>
#include <mutex>
#include <memory>

namespace moss {
namespace msomeip {
namespace tp {

class TpReassembler {
public:
    explicit TpReassembler(std::chrono::milliseconds timeout =
                            std::chrono::milliseconds(DEFAULT_REASSEMBLY_TIMEOUT_MS));

    // Add a TP segment for reassembly
    // Returns true if this completes a message
    bool add_segment(MessagePtr segment);

    // Check if a complete message is ready
    bool is_complete(uint32_t request_id) const;

    // Get the reassembled message (removes it from the buffer)
    MessagePtr reassemble(uint32_t request_id);

    // Cleanup expired incomplete messages
    void cleanup_expired();

    // Get number of pending reassemblies
    size_t pending_count() const;

private:
    bool is_complete_unlocked(uint32_t request_id) const;

    struct ReassemblyBuffer {
        ServiceId service_id;
        InstanceId instance_id;
        MethodId method_id;
        ClientId client_id;
        uint8_t interface_version;
        ReturnCode return_code;
        MessageType message_type;
        size_t total_size;
        std::map<uint32_t, std::vector<uint8_t>> segments;
        std::chrono::steady_clock::time_point first_segment_time;
        bool has_last_segment;
    };

    std::map<uint32_t, ReassemblyBuffer> buffers_;
    mutable std::mutex mutex_;
    std::chrono::milliseconds timeout_;
};

} // namespace tp
} // namespace msomeip
} // namespace moss

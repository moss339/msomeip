#pragma once

#include "msomeip/tp/tp_types.h"
#include "msomeip/message/message.h"

#include <vector>
#include <memory>

namespace moss {
namespace msomeip {
namespace tp {

class TpSegmenter {
public:
    // Segment a message into TP segments
    // max_segment_size should be UDP_SEGMENT_SIZE or TCP_SEGMENT_SIZE
    static std::vector<MessagePtr> segment(const Message& original,
                                            size_t max_segment_size);

    // Check if a message needs segmentation
    static bool needs_segmentation(size_t payload_size, size_t max_segment_size);

    // Get the number of segments needed
    static size_t get_segment_count(size_t payload_size, size_t max_segment_size);
};

} // namespace tp
} // namespace msomeip
} // namespace moss

#pragma once

#include "msomeip/types.h"
#include <cstring>

namespace moss {
namespace msomeip {

class Payload {
public:
    Payload() = default;
    explicit Payload(PayloadData data);
    explicit Payload(const uint8_t* data, size_t length);

    // Data access
    const uint8_t* data() const { return data_.data(); }
    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }

    const PayloadData& get_data() const { return data_; }
    PayloadData& get_data() { return data_; }

    // Serialization helpers
    void append(const uint8_t* data, size_t length);
    void append(const std::vector<uint8_t>& data);
    void append(uint8_t value);
    void append(uint16_t value, bool big_endian = true);
    void append(uint32_t value, bool big_endian = true);
    void append(uint64_t value, bool big_endian = true);

    // Deserialization helpers
    size_t get_position() const { return position_; }
    void set_position(size_t pos);

    bool get_uint8(uint8_t& value);
    bool get_uint16(uint16_t& value, bool big_endian = true);
    bool get_uint32(uint32_t& value, bool big_endian = true);
    bool get_uint64(uint64_t& value, bool big_endian = true);
    bool get_bytes(std::vector<uint8_t>& value, size_t length);
    bool get_string(std::string& value);
    bool get_array(std::vector<uint8_t>& value, size_t length);

    // Array with length field
    template<typename T>
    bool get_array_with_length(std::vector<T>& value) {
        uint32_t length;
        if (!get_uint32(length, true)) return false;
        size_t element_count = length / sizeof(T);
        value.resize(element_count);
        for (size_t i = 0; i < element_count; ++i) {
            std::vector<uint8_t> element_data;
            if (!get_bytes(element_data, sizeof(T))) {
                return false;
            }
            std::memcpy(&value[i], element_data.data(), sizeof(T));
        }
        return true;
    }

    // Reset
    void clear();
    PayloadPtr copy() const;

private:
    PayloadData data_;
    size_t position_ = 0;
};

}  // namespace msomeip
}  // namespace moss

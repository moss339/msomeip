#include "msomeip/message/payload.h"
#include <arpa/inet.h>
#include <cstring>

namespace moss {
namespace msomeip {

Payload::Payload(PayloadData data) : data_(std::move(data)) {}

Payload::Payload(const uint8_t* data, size_t length) {
    data_.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        data_.push_back(data[i]);
    }
}

void Payload::append(const uint8_t* data, size_t length) {
    data_.insert(data_.end(), data, data + length);
}

void Payload::append(const std::vector<uint8_t>& data) {
    data_.insert(data_.end(), data.begin(), data.end());
}

void Payload::append(uint8_t value) {
    data_.push_back(value);
}

void Payload::append(uint16_t value, bool big_endian) {
    if (big_endian) {
        data_.push_back(static_cast<uint8_t>(value >> 8));
        data_.push_back(static_cast<uint8_t>(value & 0xFF));
    } else {
        data_.push_back(static_cast<uint8_t>(value & 0xFF));
        data_.push_back(static_cast<uint8_t>(value >> 8));
    }
}

void Payload::append(uint32_t value, bool big_endian) {
    if (big_endian) {
        data_.push_back(static_cast<uint8_t>(value >> 24));
        data_.push_back(static_cast<uint8_t>(value >> 16));
        data_.push_back(static_cast<uint8_t>(value >> 8));
        data_.push_back(static_cast<uint8_t>(value & 0xFF));
    } else {
        data_.push_back(static_cast<uint8_t>(value & 0xFF));
        data_.push_back(static_cast<uint8_t>(value >> 8));
        data_.push_back(static_cast<uint8_t>(value >> 16));
        data_.push_back(static_cast<uint8_t>(value >> 24));
    }
}

void Payload::append(uint64_t value, bool big_endian) {
    if (big_endian) {
        for (int i = 7; i >= 0; --i) {
            data_.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    } else {
        for (int i = 0; i < 8; ++i) {
            data_.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    }
}

void Payload::set_position(size_t pos) {
    if (pos <= data_.size()) {
        position_ = pos;
    }
}

bool Payload::get_uint8(uint8_t& value) {
    if (position_ >= data_.size()) return false;
    value = data_[position_++];
    return true;
}

bool Payload::get_uint16(uint16_t& value, bool big_endian) {
    if (position_ + 2 > data_.size()) return false;
    if (big_endian) {
        value = (static_cast<uint16_t>(data_[position_]) << 8) |
                static_cast<uint16_t>(data_[position_ + 1]);
    } else {
        value = static_cast<uint16_t>(data_[position_]) |
                (static_cast<uint16_t>(data_[position_ + 1]) << 8);
    }
    position_ += 2;
    return true;
}

bool Payload::get_uint32(uint32_t& value, bool big_endian) {
    if (position_ + 4 > data_.size()) return false;
    if (big_endian) {
        value = (static_cast<uint32_t>(data_[position_]) << 24) |
                (static_cast<uint32_t>(data_[position_ + 1]) << 16) |
                (static_cast<uint32_t>(data_[position_ + 2]) << 8) |
                static_cast<uint32_t>(data_[position_ + 3]);
    } else {
        value = static_cast<uint32_t>(data_[position_]) |
                (static_cast<uint32_t>(data_[position_ + 1]) << 8) |
                (static_cast<uint32_t>(data_[position_ + 2]) << 16) |
                (static_cast<uint32_t>(data_[position_ + 3]) << 24);
    }
    position_ += 4;
    return true;
}

bool Payload::get_uint64(uint64_t& value, bool big_endian) {
    if (position_ + 8 > data_.size()) return false;
    value = 0;
    if (big_endian) {
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | data_[position_ + i];
        }
    } else {
        for (int i = 0; i < 8; ++i) {
            value |= (static_cast<uint64_t>(data_[position_ + i]) << (i * 8));
        }
    }
    position_ += 8;
    return true;
}

bool Payload::get_bytes(std::vector<uint8_t>& value, size_t length) {
    if (position_ + length > data_.size()) return false;
    value.assign(data_.begin() + position_, data_.begin() + position_ + length);
    position_ += length;
    return true;
}

bool Payload::get_string(std::string& value) {
    uint32_t length;
    if (!get_uint32(length, true)) return false;
    if (position_ + length > data_.size()) return false;
    value.assign(reinterpret_cast<const char*>(data_.data() + position_), length);
    position_ += length;
    return true;
}

bool Payload::get_array(std::vector<uint8_t>& value, size_t length) {
    return get_bytes(value, length);
}

void Payload::clear() {
    data_.clear();
    position_ = 0;
}

PayloadPtr Payload::copy() const {
    return std::make_shared<Payload>(data_);
}

}  // namespace msomeip
}  // namespace moss

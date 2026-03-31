#pragma once
#include "object.hpp"
#include <vector>
#include <cstdint>
#include <string>

namespace nvcs::core {

class Blob : public Object {
public:
    explicit Blob(std::vector<uint8_t> data);
    explicit Blob(const std::string& data);

    ObjectType type() const override { return ObjectType::Blob; }
    std::vector<uint8_t> serialize() const override { return data_; }

    const std::vector<uint8_t>& data() const { return data_; }
    std::string data_str() const { return {data_.begin(), data_.end()}; }

    static Blob from_envelope(const std::vector<uint8_t>& raw);

private:
    std::vector<uint8_t> data_;
};

} // namespace nvcs::core

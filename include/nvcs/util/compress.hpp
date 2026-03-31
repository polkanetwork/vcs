#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace nvcs::util {

std::vector<uint8_t> compress(const std::vector<uint8_t>& data, int level = 6);
std::vector<uint8_t> compress(const std::string& data, int level = 6);
std::vector<uint8_t> decompress(const std::vector<uint8_t>& data);
std::string decompress_to_string(const std::vector<uint8_t>& data);

class CompressError : public std::runtime_error {
public:
    explicit CompressError(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace nvcs::util

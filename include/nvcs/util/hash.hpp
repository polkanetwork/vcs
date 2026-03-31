#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace nvcs::util {

std::string sha256(const std::string& data);
std::string sha256(const std::vector<uint8_t>& data);
std::string to_hex(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> from_hex(const std::string& hex);
bool is_valid_hash(const std::string& hash);

} // namespace nvcs::util

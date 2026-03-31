#include "nvcs/util/hash.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace nvcs::util {

std::string sha256(const std::string& data) {
    return sha256(std::vector<uint8_t>(data.begin(), data.end()));
}

std::string sha256(const std::vector<uint8_t>& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), digest);
    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return ss.str();
}

std::string to_hex(const std::vector<uint8_t>& bytes) {
    std::ostringstream ss;
    for (uint8_t b : bytes)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return ss.str();
}

std::vector<uint8_t> from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0)
        throw std::invalid_argument("hex string must have even length");
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto byte_str = hex.substr(i, 2);
        result.push_back(static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16)));
    }
    return result;
}

bool is_valid_hash(const std::string& hash) {
    if (hash.size() != 64) return false;
    return std::all_of(hash.begin(), hash.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

} // namespace nvcs::util

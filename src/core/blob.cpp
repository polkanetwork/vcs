#include "nvcs/core/blob.hpp"
#include <algorithm>

namespace nvcs::core {

Blob::Blob(std::vector<uint8_t> data) : data_(std::move(data)) {}
Blob::Blob(const std::string& data) : data_(data.begin(), data.end()) {}

Blob Blob::from_envelope(const std::vector<uint8_t>& raw) {
    // Find the null byte separating header from body
    auto it = raw.begin();
    while (it != raw.end() && *it != 0) ++it;
    if (it == raw.end())
        throw ObjectError("invalid blob envelope: no null separator");
    ++it;
    return Blob(std::vector<uint8_t>(it, raw.end()));
}

} // namespace nvcs::core

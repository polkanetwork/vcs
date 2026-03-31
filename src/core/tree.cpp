#include "nvcs/core/tree.hpp"
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace nvcs::core {

Tree::Tree(std::vector<TreeEntry> entries) : entries_(std::move(entries)) {}

std::vector<uint8_t> Tree::serialize() const {
    // Format: "mode name\0hash_bytes" per entry (hash stored as hex string for simplicity)
    std::string out;
    auto sorted = entries_;
    std::sort(sorted.begin(), sorted.end(), [](const TreeEntry& a, const TreeEntry& b) {
        return a.name < b.name;
    });
    for (auto& e : sorted) {
        out += e.mode + " " + e.name + '\0' + e.hash;
    }
    return std::vector<uint8_t>(out.begin(), out.end());
}

Tree Tree::from_envelope(const std::vector<uint8_t>& raw) {
    // Skip header
    auto it = raw.begin();
    while (it != raw.end() && *it != 0) ++it;
    if (it == raw.end())
        throw ObjectError("invalid tree envelope: no null separator");
    ++it;

    std::vector<TreeEntry> entries;
    while (it != raw.end()) {
        // Read "mode name"
        auto null_pos = it;
        while (null_pos != raw.end() && *null_pos != 0) ++null_pos;
        if (null_pos == raw.end()) break;
        std::string header(it, null_pos);
        ++null_pos;

        auto space_pos = header.find(' ');
        if (space_pos == std::string::npos)
            throw ObjectError("invalid tree entry: no space in header");

        TreeEntry e;
        e.mode = header.substr(0, space_pos);
        e.name = header.substr(space_pos + 1);

        // Hash is 64 hex chars
        if (std::distance(null_pos, raw.end()) < 64)
            throw ObjectError("invalid tree entry: hash truncated");
        e.hash = std::string(null_pos, null_pos + 64);
        null_pos += 64;

        entries.push_back(std::move(e));
        it = null_pos;
    }
    return Tree(std::move(entries));
}

} // namespace nvcs::core

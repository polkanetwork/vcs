#pragma once
#include "object.hpp"
#include <string>
#include <vector>

namespace nvcs::core {

struct TreeEntry {
    std::string mode;   // e.g. "100644", "100755", "040000"
    std::string name;
    std::string hash;   // hex SHA-256 of the child object

    bool is_tree() const { return mode == "040000"; }
    bool is_executable() const { return mode == "100755"; }
};

class Tree : public Object {
public:
    Tree() = default;
    explicit Tree(std::vector<TreeEntry> entries);

    ObjectType type() const override { return ObjectType::Tree; }
    std::vector<uint8_t> serialize() const override;

    void add_entry(TreeEntry e) { entries_.push_back(std::move(e)); }
    const std::vector<TreeEntry>& entries() const { return entries_; }

    static Tree from_envelope(const std::vector<uint8_t>& raw);

private:
    std::vector<TreeEntry> entries_;
};

} // namespace nvcs::core

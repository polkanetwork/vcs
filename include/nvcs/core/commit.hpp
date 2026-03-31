#pragma once
#include "object.hpp"
#include <string>
#include <vector>
#include <ctime>

namespace nvcs::core {

struct Signature {
    std::string name;
    std::string email;
    int64_t timestamp;   // Unix epoch seconds
    int tz_offset;       // minutes from UTC

    std::string format() const;
    static Signature parse(const std::string& s);
    static Signature now(const std::string& name, const std::string& email);
};

class Commit : public Object {
public:
    Commit() = default;
    Commit(std::string tree,
           std::vector<std::string> parents,
           Signature author,
           Signature committer,
           std::string message);

    ObjectType type() const override { return ObjectType::Commit; }
    std::vector<uint8_t> serialize() const override;

    const std::string& tree() const { return tree_; }
    const std::vector<std::string>& parents() const { return parents_; }
    const Signature& author() const { return author_; }
    const Signature& committer() const { return committer_; }
    const std::string& message() const { return message_; }

    static Commit from_envelope(const std::vector<uint8_t>& raw);

private:
    std::string tree_;
    std::vector<std::string> parents_;
    Signature author_;
    Signature committer_;
    std::string message_;
};

} // namespace nvcs::core

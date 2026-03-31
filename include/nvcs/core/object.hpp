#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace nvcs::core {

enum class ObjectType { Blob, Tree, Commit, Tag };

std::string object_type_str(ObjectType t);
ObjectType object_type_from_str(const std::string& s);

class Object {
public:
    virtual ~Object() = default;
    virtual ObjectType type() const = 0;
    virtual std::vector<uint8_t> serialize() const = 0;

    // Returns the raw "type size\0data" envelope
    std::vector<uint8_t> envelope() const;
    // Hash of the envelope
    std::string hash() const;
};

class ObjectError : public std::runtime_error {
public:
    explicit ObjectError(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace nvcs::core

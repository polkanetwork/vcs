#include "nvcs/core/object.hpp"
#include "nvcs/util/hash.hpp"
#include <sstream>

namespace nvcs::core {

std::string object_type_str(ObjectType t) {
    switch (t) {
        case ObjectType::Blob:   return "blob";
        case ObjectType::Tree:   return "tree";
        case ObjectType::Commit: return "commit";
        case ObjectType::Tag:    return "tag";
    }
    return "unknown";
}

ObjectType object_type_from_str(const std::string& s) {
    if (s == "blob")   return ObjectType::Blob;
    if (s == "tree")   return ObjectType::Tree;
    if (s == "commit") return ObjectType::Commit;
    if (s == "tag")    return ObjectType::Tag;
    throw ObjectError("unknown object type: " + s);
}

std::vector<uint8_t> Object::envelope() const {
    auto body = serialize();
    std::string header = object_type_str(type()) + " " + std::to_string(body.size()) + '\0';
    std::vector<uint8_t> result;
    result.insert(result.end(), header.begin(), header.end());
    result.insert(result.end(), body.begin(), body.end());
    return result;
}

std::string Object::hash() const {
    return util::sha256(envelope());
}

} // namespace nvcs::core

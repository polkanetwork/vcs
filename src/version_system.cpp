#include "nvcs/version_system.hpp"
#include <stdexcept>

extern "C" {
    unsigned char vs_is_valid_version(const char* input);
    char* vs_normalize_version(const char* input);
    int vs_compare_versions(const char* left, const char* right);
    void vs_free_string(char* ptr);
}

namespace nvcs::version {

bool valid(const std::string& version) {
    return vs_is_valid_version(version.c_str()) != 0;
}

std::string normalize(const std::string& version) {
    char* result = vs_normalize_version(version.c_str());
    if (!result) {
        throw std::runtime_error("failed to normalize version");
    }
    std::string normalized(result);
    vs_free_string(result);
    return normalized;
}

int compare(const std::string& a, const std::string& b) {
    int result = vs_compare_versions(a.c_str(), b.c_str());
    if (result == 2) {
        throw std::invalid_argument("invalid version string");
    }
    return result;
}

} // namespace nvcs::version

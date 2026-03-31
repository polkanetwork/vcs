#pragma once

#include <string>

namespace nvcs::version {
    bool valid(const std::string& version);
    std::string normalize(const std::string& version);
    int compare(const std::string& a, const std::string& b);
}

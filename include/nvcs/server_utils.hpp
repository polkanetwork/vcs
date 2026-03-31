#pragma once

#include <string>
#include <vector>

namespace nvcs::server_utils {
    bool validate_repo_name(const std::string& name);
    std::vector<std::string> list_repositories(const std::string& repo_dir);
}

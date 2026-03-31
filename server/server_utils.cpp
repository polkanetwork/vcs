#include "nvcs/server_utils.hpp"
#include <stdexcept>

extern "C" {
    unsigned char rs_validate_repo_name(const char* input);
    char* rs_list_repositories(const char* repo_dir);
    void rs_free_string(char* ptr);
}

namespace nvcs::server_utils {

bool validate_repo_name(const std::string& name) {
    return rs_validate_repo_name(name.c_str()) != 0;
}

std::vector<std::string> list_repositories(const std::string& repo_dir) {
    std::vector<std::string> result;
    char* out = rs_list_repositories(repo_dir.c_str());
    if (!out) {
        return result;
    }
    std::string text(out);
    rs_free_string(out);
    size_t pos = 0;
    while (pos < text.size()) {
        auto next = text.find('\n', pos);
        if (next == std::string::npos) {
            result.push_back(text.substr(pos));
            break;
        }
        result.push_back(text.substr(pos, next - pos));
        pos = next + 1;
    }
    return result;
}

} // namespace nvcs::server_utils

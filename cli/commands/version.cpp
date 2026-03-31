#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

#ifndef NVCS_VERSION
#define NVCS_VERSION "unknown"
#endif

int cmd_version(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    std::cout << "nvcs version " << NVCS_VERSION << "\n";

    if (args.empty()) {
        try {
            auto repo = nvcs::core::Repository::open(fs::current_path());
            std::cout << "repo branch: " << (repo.current_branch().empty() ? "(detached)" : repo.current_branch()) << "\n";
            std::cout << "repo commit: " << (repo.current_commit().empty() ? "(none)" : repo.current_commit().substr(0, 12)) << "\n";
            auto tags = repo.refs().list_tags();
            if (!tags.empty()) {
                std::cout << "repo tags:\n";
                for (auto& tag : tags)
                    std::cout << "  " << tag << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "warning: could not read repository: " << e.what() << "\n";
        }
    }

    return 0;
}

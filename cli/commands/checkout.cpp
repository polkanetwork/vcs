#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

int cmd_checkout(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    if (args.empty()) { std::cerr << "Usage: nvcs checkout <branch|hash>\n"; return 1; }
    auto repo = nvcs::core::Repository::open(fs::current_path());

    bool create = false;
    std::string ref;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-b") create = true;
        else ref = args[i];
    }

    if (create) {
        repo.create_branch(ref);
        repo.checkout(ref);
        std::cout << "Switched to new branch '" << ref << "'\n";
    } else {
        repo.checkout(ref);
        auto branch = repo.current_branch();
        if (branch.empty())
            std::cout << "HEAD is now at " << repo.current_commit().substr(0, 12) << "\n";
        else
            std::cout << "Switched to branch '" << branch << "'\n";
    }
    return 0;
}

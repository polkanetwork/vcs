#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>
#include <optional>

int cmd_config(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;

    // Strip --global flag (treat as repo-local for now)
    std::vector<std::string> fargs;
    for (auto& a : args) if (a != "--global") fargs.push_back(a);

    if (fargs.size() == 1) {
        // Get value
        auto repo = nvcs::core::Repository::open(fs::current_path());
        auto& cfg = repo.config();
        if (fargs[0] == "user.name") std::cout << cfg.user_name << "\n";
        else if (fargs[0] == "user.email") std::cout << cfg.user_email << "\n";
        else if (fargs[0] == "core.defaultbranch") std::cout << cfg.default_branch << "\n";
        else { std::cerr << "unknown key: " << fargs[0] << "\n"; return 1; }
        return 0;
    }

    if (fargs.size() == 2) {
        auto repo = nvcs::core::Repository::open(fs::current_path());
        auto& cfg = repo.config();
        if (fargs[0] == "user.name") cfg.user_name = fargs[1];
        else if (fargs[0] == "user.email") cfg.user_email = fargs[1];
        else if (fargs[0] == "core.defaultbranch") cfg.default_branch = fargs[1];
        else { std::cerr << "unknown key: " << fargs[0] << "\n"; return 1; }
        cfg.save(repo.nvcs_dir() / "config");
        return 0;
    }

    std::cerr << "Usage: nvcs config [--global] <key> [value]\n";
    return 1;
}

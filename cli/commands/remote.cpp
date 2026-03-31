#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

int cmd_remote(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    auto repo = nvcs::core::Repository::open(fs::current_path());
    auto& cfg = repo.config();

    if (args.empty() || args[0] == "list") {
        for (auto& [name, url] : cfg.remotes)
            std::cout << name << "\t" << url << "\n";
        return 0;
    }
    if (args[0] == "add") {
        if (args.size() < 3) { std::cerr << "Usage: nvcs remote add <name> <url>\n"; return 1; }
        cfg.remotes[args[1]] = args[2];
        cfg.save(repo.nvcs_dir() / "config");
        std::cout << "Added remote '" << args[1] << "'\n";
        return 0;
    }
    if (args[0] == "remove" || args[0] == "rm") {
        if (args.size() < 2) { std::cerr << "Usage: nvcs remote remove <name>\n"; return 1; }
        cfg.remotes.erase(args[1]);
        cfg.save(repo.nvcs_dir() / "config");
        std::cout << "Removed remote '" << args[1] << "'\n";
        return 0;
    }
    if (args[0] == "set-url") {
        if (args.size() < 3) { std::cerr << "Usage: nvcs remote set-url <name> <url>\n"; return 1; }
        if (!cfg.remotes.count(args[1])) { std::cerr << "Remote not found: " << args[1] << "\n"; return 1; }
        cfg.remotes[args[1]] = args[2];
        cfg.save(repo.nvcs_dir() / "config");
        return 0;
    }
    std::cerr << "Unknown remote subcommand: " << args[0] << "\n";
    return 1;
}

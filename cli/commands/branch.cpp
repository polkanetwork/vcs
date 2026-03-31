#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

int cmd_branch(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    auto repo = nvcs::core::Repository::open(fs::current_path());
    auto current = repo.current_branch();

    if (args.empty()) {
        // List branches
        for (auto& b : repo.list_branches()) {
            std::cout << (b == current ? "* " : "  ") << b << "\n";
        }
        return 0;
    }

    if (args[0] == "-d" || args[0] == "--delete") {
        if (args.size() < 2) { std::cerr << "Usage: nvcs branch -d <name>\n"; return 1; }
        if (args[1] == current) { std::cerr << "Cannot delete current branch\n"; return 1; }
        repo.delete_branch(args[1]);
        std::cout << "Deleted branch " << args[1] << "\n";
        return 0;
    }

    // Create branch
    std::string from = args.size() > 1 ? args[1] : "";
    repo.create_branch(args[0], from);
    std::cout << "Created branch " << args[0] << "\n";
    return 0;
}

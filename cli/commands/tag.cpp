#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

int cmd_tag(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    auto repo = nvcs::core::Repository::open(fs::current_path());

    if (args.empty()) {
        for (auto& t : repo.refs().list_tags())
            std::cout << t << "\n";
        return 0;
    }

    if (args[0] == "-d" || args[0] == "--delete") {
        if (args.size() < 2) { std::cerr << "Usage: nvcs tag -d <name>\n"; return 1; }
        repo.refs().delete_tag(args[1]);
        std::cout << "Deleted tag '" << args[1] << "'\n";
        return 0;
    }

    std::string name = args[0];
    std::string hash = args.size() > 1 ? repo.refs().resolve(args[1]) : repo.current_commit();
    if (hash.empty()) { std::cerr << "error: no commit to tag\n"; return 1; }
    repo.refs().write_tag(name, hash);
    std::cout << "Created tag '" << name << "' at " << hash.substr(0, 12) << "\n";
    return 0;
}

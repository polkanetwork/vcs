#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

int cmd_add(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    if (args.empty()) {
        std::cerr << "Usage: nvcs add <file>...\n";
        return 1;
    }
    auto repo = nvcs::core::Repository::open(fs::current_path());
    int added = 0;
    for (auto& arg : args) {
        fs::path p(arg);
        // Expand directories recursively
        if (fs::is_directory(repo.work_dir() / p)) {
            for (auto& entry : fs::recursive_directory_iterator(repo.work_dir() / p)) {
                if (!entry.is_regular_file()) continue;
                auto rel = fs::relative(entry.path(), repo.work_dir());
                if (rel.string().find(".nvcs/") == 0) continue;
                try {
                    repo.stage_file(rel);
                    ++added;
                } catch (const std::exception& e) {
                    std::cerr << "warning: " << e.what() << "\n";
                }
            }
        } else {
            auto rel = p.is_absolute() ? fs::relative(p, repo.work_dir()) : p;
            repo.stage_file(rel);
            ++added;
        }
    }
    if (added == 1)
        std::cout << "Staged 1 file.\n";
    else
        std::cout << "Staged " << added << " files.\n";
    return 0;
}

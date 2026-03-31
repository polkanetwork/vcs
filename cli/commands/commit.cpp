#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>
#include <cstdlib>

int cmd_commit(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    std::string message;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-m" && i + 1 < args.size()) {
            message = args[i + 1];
            ++i;
        }
    }
    if (message.empty()) {
        // Check env var NVCS_EDITOR or fallback message
        auto editor_msg = std::getenv("NVCS_COMMIT_MSG");
        // TODO: Set fallback message??
        if (editor_msg) message = editor_msg;
        else {
            std::cerr << "Usage: nvcs commit -m <message>\n";
            return 1;
        }
    }

    auto repo = nvcs::core::Repository::open(fs::current_path());
    auto hash = repo.create_commit(message);
    auto branch = repo.current_branch();
    std::string ref_info = branch.empty() ? hash.substr(0, 12) : branch;
    std::cout << "[" << ref_info << " " << hash.substr(0, 12) << "] " << message << "\n";
    return 0;
}

#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

int cmd_init(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    fs::path dir = args.empty() ? fs::current_path() : fs::path(args[0]);
    fs::create_directories(dir);
    auto repo = nvcs::core::Repository::init(dir);
    std::cout << "Initialized empty nvcs repository in "
              << (dir / ".nvcs").string() << "\n";
    return 0;
}

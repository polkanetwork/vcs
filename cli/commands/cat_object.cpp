#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

int cmd_cat_object(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    if (args.empty()) { std::cerr << "Usage: nvcs cat-object <hash>\n"; return 1; }
    auto repo = nvcs::core::Repository::open(fs::current_path());
    auto hash = repo.refs().resolve(args[0]);
    if (hash.empty()) hash = args[0];

    auto obj = repo.read_object(hash);
    std::cout << "type: " << nvcs::core::object_type_str(obj->type()) << "\n";
    std::cout << "hash: " << hash << "\n\n";

    if (obj->type() == nvcs::core::ObjectType::Blob) {
        auto blob = dynamic_cast<nvcs::core::Blob*>(obj.get());
        std::cout << blob->data_str();
    } else if (obj->type() == nvcs::core::ObjectType::Tree) {
        auto tree = dynamic_cast<nvcs::core::Tree*>(obj.get());
        for (auto& e : tree->entries())
            std::cout << e.mode << " " << e.name << " " << e.hash << "\n";
    } else if (obj->type() == nvcs::core::ObjectType::Commit) {
        auto commit = dynamic_cast<nvcs::core::Commit*>(obj.get());
        auto body = commit->serialize();
        std::cout << std::string(body.begin(), body.end());
    }
    return 0;
}

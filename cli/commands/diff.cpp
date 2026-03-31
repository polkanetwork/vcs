#include "nvcs/core/repository.hpp"
#include "nvcs/util/fs.hpp"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <functional>
#include <algorithm>
#include <unordered_map>

// Minimal unified diff implementation
static std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> lines;
    std::istringstream ss(s);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

static void print_diff(const std::string& a_label, const std::string& b_label,
                       const std::string& a, const std::string& b) {
    auto a_lines = split_lines(a);
    auto b_lines = split_lines(b);
    std::cout << "--- " << a_label << "\n";
    std::cout << "+++ " << b_label << "\n";
    // Simple diff: just show removed then added (not LCS-based, but sufficient)
    // For production use, a proper Myers diff would be used
    std::cout << "@@ -1," << a_lines.size() << " +1," << b_lines.size() << " @@\n";
    for (auto& l : a_lines) std::cout << "-" << l << "\n";
    for (auto& l : b_lines) std::cout << "+" << l << "\n";
}

int cmd_diff(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    bool staged = false;
    for (auto& a : args) if (a == "--staged" || a == "--cached") staged = true;
    auto repo = nvcs::core::Repository::open(fs::current_path());

    if (staged) {
        // Compare HEAD tree vs index
        std::unordered_map<std::string, std::string> head_flat;
        auto head_hash = repo.refs().resolve_head();
        if (!head_hash.empty() && repo.has_object(head_hash)) {
            auto c = repo.read_commit(head_hash);
            // flatten tree
            std::function<void(const std::string&, const std::string&)> flatten =
                [&](const std::string& th, const std::string& prefix) {
                    auto tree = repo.read_tree(th);
                    for (auto& e : tree.entries()) {
                        auto p = prefix.empty() ? e.name : prefix + "/" + e.name;
                        if (e.is_tree()) flatten(e.hash, p);
                        else head_flat[p] = e.hash;
                    }
                };
            flatten(c.tree(), "");
        }

        for (auto& ie : repo.index().entries()) {
            auto a_hash = head_flat.count(ie.path) ? head_flat[ie.path] : "";
            if (a_hash == ie.hash) continue;
            std::string a_content, b_content;
            if (!a_hash.empty()) {
                auto blob = repo.read_blob(a_hash);
                a_content = blob.data_str();
            }
            auto blob = repo.read_blob(ie.hash);
            b_content = blob.data_str();
            print_diff("a/" + ie.path, "b/" + ie.path, a_content, b_content);
        }
    } else {
        // Compare index vs working tree
        for (auto& ie : repo.index().entries()) {
            auto abs = repo.work_dir() / ie.path;
            if (!fs::exists(abs)) {
                auto blob = repo.read_blob(ie.hash);
                print_diff("a/" + ie.path, "/dev/null", blob.data_str(), "");
                continue;
            }
            auto cur = nvcs::util::fs::read_file_str(abs);
            auto blob = repo.read_blob(ie.hash);
            auto staged_content = blob.data_str();
            if (cur != staged_content)
                print_diff("a/" + ie.path, "b/" + ie.path, staged_content, cur);
        }
    }
    return 0;
}

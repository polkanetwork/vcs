#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>

int cmd_status(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    auto repo = nvcs::core::Repository::open(fs::current_path());

    auto branch = repo.current_branch();
    if (branch.empty()) {
        auto h = repo.current_commit();
        std::cout << "HEAD detached at " << h.substr(0, 12) << "\n";
    } else {
        std::cout << "On branch " << branch << "\n";
    }

    auto entries = repo.status();
    bool has_staged = false, has_unstaged = false, has_untracked = false;

    for (auto& e : entries) {
        if (e.index_state != nvcs::core::StatusEntry::State::Unmodified &&
            e.index_state != nvcs::core::StatusEntry::State::Untracked) has_staged = true;
        if (e.work_state == nvcs::core::StatusEntry::State::Modified ||
            e.work_state == nvcs::core::StatusEntry::State::Deleted) has_unstaged = true;
        if (e.work_state == nvcs::core::StatusEntry::State::Untracked) has_untracked = true;
    }

    if (!has_staged && !has_unstaged && !has_untracked) {
        std::cout << "nothing to commit, working tree clean\n";
        return 0;
    }

    auto state_char = [](nvcs::core::StatusEntry::State s) -> char {
        using S = nvcs::core::StatusEntry::State;
        switch (s) {
            case S::Added: return 'A';
            case S::Modified: return 'M';
            case S::Deleted: return 'D';
            case S::Renamed: return 'R';
            default: return ' ';
        }
    };

    if (has_staged) {
        std::cout << "\nChanges to be committed:\n";
        for (auto& e : entries) {
            char c = state_char(e.index_state);
            if (c == ' ') continue;
            std::cout << "  " << c << "  " << e.path << "\n";
        }
    }
    if (has_unstaged) {
        std::cout << "\nChanges not staged for commit:\n";
        for (auto& e : entries) {
            if (e.index_state == nvcs::core::StatusEntry::State::Untracked) continue;
            char c = state_char(e.work_state);
            if (c == ' ') continue;
            std::cout << "  " << c << "  " << e.path << "\n";
        }
    }
    if (has_untracked) {
        std::cout << "\nUntracked files:\n";
        for (auto& e : entries) {
            if (e.work_state == nvcs::core::StatusEntry::State::Untracked)
                std::cout << "  " << e.path << "\n";
        }
    }
    std::cout << "\n";
    return 0;
}

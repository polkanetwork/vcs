#include "nvcs/core/index.hpp"
#include "nvcs/util/fs.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace nvcs::core {

Index::Index(stdfs::path index_path) : path_(std::move(index_path)) {}

void Index::load() {
    entries_.clear();
    path_map_.clear();
    if (!stdfs::exists(path_)) return;

    auto data = util::fs::read_file_str(path_);
    std::istringstream ss(data);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        // Format: "hash mode size mtime path"
        std::istringstream ls(line);
        IndexEntry e;
        ls >> e.hash >> e.mode >> e.size >> e.mtime >> e.path;
        e.staged = true;
        if (!e.path.empty()) {
            path_map_[e.path] = entries_.size();
            entries_.push_back(std::move(e));
        }
    }
}

void Index::save() const {
    std::ostringstream ss;
    for (auto& e : entries_) {
        ss << e.hash << " " << e.mode << " "
           << e.size << " " << e.mtime << " " << e.path << "\n";
    }
    util::fs::write_file(path_, ss.str());
}

void Index::stage(const IndexEntry& entry) {
    auto it = path_map_.find(entry.path);
    if (it != path_map_.end()) {
        entries_[it->second] = entry;
    } else {
        path_map_[entry.path] = entries_.size();
        entries_.push_back(entry);
    }
}

void Index::remove(const std::string& path) {
    auto it = path_map_.find(path);
    if (it == path_map_.end()) return;
    entries_.erase(entries_.begin() + it->second);
    // Rebuild path_map
    path_map_.clear();
    for (size_t i = 0; i < entries_.size(); ++i)
        path_map_[entries_[i].path] = i;
}

bool Index::has(const std::string& path) const {
    return path_map_.count(path) > 0;
}

const IndexEntry* Index::get(const std::string& path) const {
    auto it = path_map_.find(path);
    if (it == path_map_.end()) return nullptr;
    return &entries_[it->second];
}

std::vector<IndexEntry> Index::staged_entries() const {
    std::vector<IndexEntry> result;
    for (auto& e : entries_)
        if (e.staged) result.push_back(e);
    return result;
}

} // namespace nvcs::core

#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <ctime>

namespace nvcs::core {

namespace stdfs = std::filesystem;

struct IndexEntry {
    std::string path;       // relative path from repo root
    std::string hash;       // blob hash
    std::string mode;       // "100644" or "100755"
    uint64_t size;
    int64_t mtime;          // modification time (seconds)
    bool staged;            // whether it is staged vs tracked
};

class Index {
public:
    explicit Index(stdfs::path index_path);

    void load();
    void save() const;

    void stage(const IndexEntry& entry);
    void remove(const std::string& path);
    bool has(const std::string& path) const;
    const IndexEntry* get(const std::string& path) const;

    const std::vector<IndexEntry>& entries() const { return entries_; }

    // Returns staged entries (new or modified vs HEAD)
    std::vector<IndexEntry> staged_entries() const;

private:
    stdfs::path path_;
    std::vector<IndexEntry> entries_;
    std::unordered_map<std::string, size_t> path_map_;
};

} // namespace nvcs::core

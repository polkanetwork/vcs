#include "nvcs/util/fs.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace nvcs::util::fs {

void write_file(const stdfs::path& path, const std::vector<uint8_t>& data) {
    ensure_dir(path.parent_path());
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("cannot open for write: " + path.string());
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void write_file(const stdfs::path& path, const std::string& data) {
    write_file(path, std::vector<uint8_t>(data.begin(), data.end()));
}

std::vector<uint8_t> read_file(const stdfs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open for read: " + path.string());
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
}

std::string read_file_str(const stdfs::path& path) {
    auto bytes = read_file(path);
    return std::string(bytes.begin(), bytes.end());
}

void ensure_dir(const stdfs::path& path) {
    if (!path.empty() && !stdfs::exists(path))
        stdfs::create_directories(path);
}

bool exists(const stdfs::path& path) {
    return stdfs::exists(path);
}

std::string read_line(const stdfs::path& path) {
    std::string s = read_file_str(path);
    // Strip trailing newline
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    return s;
}

void write_line(const stdfs::path& path, const std::string& line) {
    write_file(path, line + "\n");
}

std::vector<stdfs::path> list_files_recursive(const stdfs::path& dir) {
    std::vector<stdfs::path> result;
    if (!stdfs::exists(dir)) return result;
    for (auto& entry : stdfs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file())
            result.push_back(entry.path());
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<stdfs::path> find_repo_root(const stdfs::path& start) {
    auto cur = stdfs::absolute(start);
    while (true) {
        if (stdfs::exists(cur / ".nvcs")) return cur;
        auto parent = cur.parent_path();
        if (parent == cur) return std::nullopt;
        cur = parent;
    }
}

} // namespace nvcs::util::fs

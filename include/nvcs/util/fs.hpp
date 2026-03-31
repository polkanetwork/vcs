#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace nvcs::util::fs {

namespace stdfs = std::filesystem;

void write_file(const stdfs::path& path, const std::vector<uint8_t>& data);
void write_file(const stdfs::path& path, const std::string& data);
std::vector<uint8_t> read_file(const stdfs::path& path);
std::string read_file_str(const stdfs::path& path);
void ensure_dir(const stdfs::path& path);
bool exists(const stdfs::path& path);
std::string read_line(const stdfs::path& path);
void write_line(const stdfs::path& path, const std::string& line);
std::vector<stdfs::path> list_files_recursive(const stdfs::path& dir);
std::optional<stdfs::path> find_repo_root(const stdfs::path& start);

} // namespace nvcs::util::fs

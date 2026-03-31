#include "nvcs/core/refs.hpp"
#include "nvcs/util/fs.hpp"
#include "nvcs/util/hash.hpp"
#include <stdexcept>
#include <algorithm>

namespace nvcs::core {

Refs::Refs(stdfs::path nvcs_dir) : dir_(std::move(nvcs_dir)) {}

stdfs::path Refs::ref_path(const std::string& ref) const {
    return dir_ / ref;
}

std::string Refs::read_head() const {
    auto head_path = dir_ / "HEAD";
    if (!util::fs::exists(head_path))
        throw std::runtime_error("HEAD not found");
    auto content = util::fs::read_line(head_path);
    if (content.substr(0, 5) == "ref: ")
        return content.substr(5);  // branch reference
    return content;  // detached hash
}

bool Refs::head_is_detached() const {
    auto head = read_head();
    return util::is_valid_hash(head);
}

std::string Refs::resolve_head() const {
    auto head = read_head();
    if (util::is_valid_hash(head)) return head;
    return resolve(head);
}

void Refs::set_head_branch(const std::string& branch) {
    util::fs::write_line(dir_ / "HEAD", "ref: refs/heads/" + branch);
}

void Refs::set_head_detached(const std::string& hash) {
    util::fs::write_line(dir_ / "HEAD", hash);
}

std::optional<std::string> Refs::read_branch(const std::string& name) const {
    auto p = dir_ / "refs" / "heads" / name;
    if (!util::fs::exists(p)) return std::nullopt;
    return util::fs::read_line(p);
}

void Refs::write_branch(const std::string& name, const std::string& hash) {
    util::fs::write_line(dir_ / "refs" / "heads" / name, hash);
}

void Refs::delete_branch(const std::string& name) {
    auto p = dir_ / "refs" / "heads" / name;
    if (util::fs::exists(p)) stdfs::remove(p);
}

std::vector<std::string> Refs::list_branches() const {
    auto dir = dir_ / "refs" / "heads";
    if (!stdfs::exists(dir)) return {};
    std::vector<std::string> result;
    for (auto& entry : stdfs::directory_iterator(dir))
        if (entry.is_regular_file())
            result.push_back(entry.path().filename().string());
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<std::string> Refs::read_tag(const std::string& name) const {
    auto p = dir_ / "refs" / "tags" / name;
    if (!util::fs::exists(p)) return std::nullopt;
    return util::fs::read_line(p);
}

void Refs::write_tag(const std::string& name, const std::string& hash) {
    util::fs::write_line(dir_ / "refs" / "tags" / name, hash);
}

void Refs::delete_tag(const std::string& name) {
    auto p = dir_ / "refs" / "tags" / name;
    if (util::fs::exists(p)) stdfs::remove(p);
}

std::vector<std::string> Refs::list_tags() const {
    auto dir = dir_ / "refs" / "tags";
    if (!stdfs::exists(dir)) return {};
    std::vector<std::string> result;
    for (auto& entry : stdfs::directory_iterator(dir))
        if (entry.is_regular_file())
            result.push_back(entry.path().filename().string());
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<std::string> Refs::read_remote_ref(const std::string& remote, const std::string& branch) const {
    auto p = dir_ / "refs" / "remotes" / remote / branch;
    if (!util::fs::exists(p)) return std::nullopt;
    return util::fs::read_line(p);
}

void Refs::write_remote_ref(const std::string& remote, const std::string& branch, const std::string& hash) {
    util::fs::write_line(dir_ / "refs" / "remotes" / remote / branch, hash);
}

std::vector<std::string> Refs::list_remote_refs(const std::string& remote) const {
    auto dir = dir_ / "refs" / "remotes" / remote;
    if (!stdfs::exists(dir)) return {};
    std::vector<std::string> result;
    for (auto& entry : stdfs::directory_iterator(dir))
        if (entry.is_regular_file())
            result.push_back(entry.path().filename().string());
    std::sort(result.begin(), result.end());
    return result;
}

std::string Refs::resolve(const std::string& ref) const {
    if (util::is_valid_hash(ref)) return ref;
    // Try as branch
    auto b = read_branch(ref);
    if (b) return *b;
    // Try as tag
    auto t = read_tag(ref);
    if (t) return *t;
    // Try as full ref path
    if (ref.substr(0, 11) == "refs/heads/") {
        auto b2 = read_branch(ref.substr(11));
        if (b2) return *b2;
    }
    if (ref.substr(0, 10) == "refs/tags/") {
        auto t2 = read_tag(ref.substr(10));
        if (t2) return *t2;
    }
    return "";
}

} // namespace nvcs::core

#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace nvcs::core {

namespace stdfs = std::filesystem;

class Refs {
public:
    explicit Refs(stdfs::path nvcs_dir);

    // HEAD management
    std::string read_head() const;            // returns branch name or hash
    bool head_is_detached() const;
    std::string resolve_head() const;         // always returns a hash (or empty)
    void set_head_branch(const std::string& branch);
    void set_head_detached(const std::string& hash);

    // Branch ops
    std::optional<std::string> read_branch(const std::string& name) const;
    void write_branch(const std::string& name, const std::string& hash);
    void delete_branch(const std::string& name);
    std::vector<std::string> list_branches() const;

    // Tag ops
    std::optional<std::string> read_tag(const std::string& name) const;
    void write_tag(const std::string& name, const std::string& hash);
    void delete_tag(const std::string& name);
    std::vector<std::string> list_tags() const;

    // Remote tracking refs
    std::optional<std::string> read_remote_ref(const std::string& remote, const std::string& branch) const;
    void write_remote_ref(const std::string& remote, const std::string& branch, const std::string& hash);
    std::vector<std::string> list_remote_refs(const std::string& remote) const;

    // Resolve any ref string to a hash
    std::string resolve(const std::string& ref) const;

private:
    stdfs::path dir_;
    stdfs::path ref_path(const std::string& ref) const;
};

} // namespace nvcs::core

#pragma once
#include "object.hpp"
#include "blob.hpp"
#include "tree.hpp"
#include "commit.hpp"
#include "index.hpp"
#include "refs.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

namespace nvcs::core {

namespace stdfs = std::filesystem;

struct Config {
    std::string user_name;
    std::string user_email;
    std::unordered_map<std::string, std::string> remotes;   // name -> url
    std::string default_branch = "main";

    void load(const stdfs::path& config_path);
    void save(const stdfs::path& config_path) const;
};

struct StatusEntry {
    enum class State { Untracked, Modified, Deleted, Added, Renamed, Unmodified };
    std::string path;
    State index_state;   // staged
    State work_state;    // unstaged
};

class Repository {
public:
    explicit Repository(stdfs::path work_dir);
    ~Repository() = default;

    static Repository init(const stdfs::path& dir, const std::string& default_branch = "main");
    static Repository open(const stdfs::path& start_dir);

    // Object store
    std::string write_object(const Object& obj);
    std::unique_ptr<Object> read_object(const std::string& hash) const;
    Blob read_blob(const std::string& hash) const;
    Tree read_tree(const std::string& hash) const;
    Commit read_commit(const std::string& hash) const;
    bool has_object(const std::string& hash) const;

    // Working directory operations
    void stage_file(const stdfs::path& rel_path);
    void unstage_file(const stdfs::path& rel_path);
    std::string create_commit(const std::string& message);
    std::vector<StatusEntry> status() const;

    // Branch/ref operations
    std::string current_branch() const;
    std::string current_commit() const;
    void create_branch(const std::string& name, const std::string& from = "");
    void delete_branch(const std::string& name);
    void checkout(const std::string& ref);
    std::vector<std::string> list_branches() const;

    // Log
    std::vector<Commit> log(const std::string& from = "", int max = -1) const;

    // Pack operations (for push/pull)
    std::vector<uint8_t> create_pack(const std::vector<std::string>& hashes) const;
    void apply_pack(const std::vector<uint8_t>& pack_data);

    // Config
    Config& config() { return config_; }
    const Config& config() const { return config_; }

    // Paths
    const stdfs::path& work_dir() const { return work_dir_; }
    const stdfs::path& nvcs_dir() const { return nvcs_dir_; }
    Index& index() { return *index_; }
    Refs& refs() { return *refs_; }

private:
    stdfs::path work_dir_;
    stdfs::path nvcs_dir_;
    Config config_;
    std::unique_ptr<Index> index_;
    std::unique_ptr<Refs> refs_;

    stdfs::path object_path(const std::string& hash) const;
    Tree build_tree(const stdfs::path& dir, const stdfs::path& root);
    void checkout_tree(const std::string& tree_hash, const stdfs::path& dir);
    void collect_reachable(const std::string& hash, std::vector<std::string>& out) const;

    // Status helpers
    std::unordered_map<std::string, std::string> head_tree_flat() const;
    void flatten_tree(const std::string& tree_hash, const std::string& prefix,
                      std::unordered_map<std::string, std::string>& out) const;
};

} // namespace nvcs::core

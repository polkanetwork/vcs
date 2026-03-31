#include "nvcs/core/repository.hpp"
#include "nvcs/util/fs.hpp"
#include "nvcs/util/hash.hpp"
#include "nvcs/util/compress.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <set>
#include <map>
#include <filesystem>
#include <sys/stat.h>

namespace nvcs::core {

// ─── Config ─────────────────────────────────────────────────────────────────

void Config::load(const stdfs::path& p) {
    if (!stdfs::exists(p)) return;
    std::ifstream f(p);
    std::string line;
    std::string section;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            section = line.substr(1, line.find(']') - 1);
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        // trim leading/trailing whitespace from key and leading from val
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key = key.substr(1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val = val.substr(1);
        while (!val.empty() && (val.back() == '\r' || val.back() == '\n')) val.pop_back();
        if (section == "user") {
            if (key == "name") user_name = val;
            if (key == "email") user_email = val;
        } else if (section.substr(0, 7) == "remote ") {
            auto remote_name = section.substr(8, section.size() - 9); // strip quotes
            if (key == "url") remotes[remote_name] = val;
        } else if (section == "core") {
            if (key == "defaultbranch") default_branch = val;
        }
    }
}

void Config::save(const stdfs::path& p) const {
    std::ostringstream ss;
    ss << "[core]\n";
    ss << "    defaultbranch = " << default_branch << "\n";
    ss << "[user]\n";
    ss << "    name = " << user_name << "\n";
    ss << "    email = " << user_email << "\n";
    for (auto& [name, url] : remotes)
        ss << "[remote \"" << name << "\"]\n    url = " << url << "\n";
    util::fs::write_file(p, ss.str());
}

// ─── Repository ─────────────────────────────────────────────────────────────

Repository::Repository(stdfs::path work_dir)
    : work_dir_(std::move(work_dir)),
      nvcs_dir_(work_dir_ / ".nvcs") {
    config_.load(nvcs_dir_ / "config");
    index_ = std::make_unique<Index>(nvcs_dir_ / "index");
    index_->load();
    refs_ = std::make_unique<Refs>(nvcs_dir_);
}

Repository Repository::init(const stdfs::path& dir, const std::string& default_branch) {
    auto nvcs = dir / ".nvcs";
    if (stdfs::exists(nvcs))
        throw std::runtime_error("repository already initialized at " + dir.string());

    util::fs::ensure_dir(nvcs / "objects");
    util::fs::ensure_dir(nvcs / "refs" / "heads");
    util::fs::ensure_dir(nvcs / "refs" / "tags");
    util::fs::ensure_dir(nvcs / "refs" / "remotes");

    util::fs::write_line(nvcs / "HEAD", "ref: refs/heads/" + default_branch);

    Config cfg;
    cfg.default_branch = default_branch;
    cfg.user_name = "Unknown User";
    cfg.user_email = "user@example.com";
    cfg.save(nvcs / "config");

    return Repository(dir);
}

Repository Repository::open(const stdfs::path& start_dir) {
    auto root = util::fs::find_repo_root(stdfs::absolute(start_dir));
    if (!root) throw std::runtime_error("not a nvcs repository (or any parent directory)");
    return Repository(*root);
}

stdfs::path Repository::object_path(const std::string& hash) const {
    return nvcs_dir_ / "objects" / hash.substr(0, 2) / hash.substr(2);
}

std::string Repository::write_object(const Object& obj) {
    auto env = obj.envelope();
    auto hash = obj.hash();
    auto path = object_path(hash);
    if (!stdfs::exists(path)) {
        auto compressed = util::compress(env);
        util::fs::write_file(path, compressed);
    }
    return hash;
}

std::unique_ptr<Object> Repository::read_object(const std::string& hash) const {
    auto path = object_path(hash);
    if (!stdfs::exists(path))
        throw std::runtime_error("object not found: " + hash);
    auto compressed = util::fs::read_file(path);
    auto raw = util::decompress(compressed);

    // Parse header
    auto null_pos = std::find(raw.begin(), raw.end(), '\0');
    if (null_pos == raw.end())
        throw ObjectError("invalid object: no null separator in " + hash);
    std::string header(raw.begin(), null_pos);
    auto space = header.find(' ');
    auto type_str = header.substr(0, space);
    auto type = object_type_from_str(type_str);

    switch (type) {
        case ObjectType::Blob:   return std::make_unique<Blob>(Blob::from_envelope(raw));
        case ObjectType::Tree:   return std::make_unique<Tree>(Tree::from_envelope(raw));
        case ObjectType::Commit: return std::make_unique<Commit>(Commit::from_envelope(raw));
        default: throw ObjectError("unsupported object type: " + type_str);
    }
}

Blob Repository::read_blob(const std::string& hash) const {
    return Blob::from_envelope(util::decompress(util::fs::read_file(object_path(hash))));
}

Tree Repository::read_tree(const std::string& hash) const {
    return Tree::from_envelope(util::decompress(util::fs::read_file(object_path(hash))));
}

Commit Repository::read_commit(const std::string& hash) const {
    return Commit::from_envelope(util::decompress(util::fs::read_file(object_path(hash))));
}

bool Repository::has_object(const std::string& hash) const {
    return stdfs::exists(object_path(hash));
}

void Repository::stage_file(const stdfs::path& rel_path) {
    auto abs_path = work_dir_ / rel_path;
    if (!stdfs::exists(abs_path))
        throw std::runtime_error("file not found: " + rel_path.string());

    auto data = util::fs::read_file(abs_path);
    Blob blob(data);
    auto hash = write_object(blob);

    auto mode = stdfs::status(abs_path).permissions();
    bool exec = (mode & stdfs::perms::owner_exec) != stdfs::perms::none;

    IndexEntry e;
    e.path = rel_path.string();
    e.hash = hash;
    e.mode = exec ? "100755" : "100644";
    e.size = data.size();
    struct stat st;
    if (::stat(abs_path.string().c_str(), &st) == 0)
        e.mtime = static_cast<int64_t>(st.st_mtime);
    else
        e.mtime = 0;
    e.staged = true;

    index_->stage(e);
    index_->save();
}

void Repository::unstage_file(const stdfs::path& rel_path) {
    index_->remove(rel_path.string());
    index_->save();
}

Tree Repository::build_tree(const stdfs::path& dir, const stdfs::path& root) {
    Tree t;
    for (auto& entry : stdfs::directory_iterator(dir)) {
        auto rel = stdfs::relative(entry.path(), root);
        auto name = entry.path().filename().string();
        if (name == ".nvcs") continue;
        if (entry.is_directory()) {
            auto subtree = build_tree(entry.path(), root);
            auto hash = write_object(subtree);
            TreeEntry te2; te2.mode = "040000"; te2.name = name; te2.hash = hash;
            t.add_entry(te2);
        }
    }
    // Add staged files from index that live under this dir
    auto dir_rel = stdfs::relative(dir, root).string();
    if (dir_rel == ".") dir_rel = "";
    for (auto& ie : index_->entries()) {
        stdfs::path file_path(ie.path);
        std::string parent = file_path.has_parent_path() ? file_path.parent_path().string() : "";
        if (parent == dir_rel)
            t.add_entry({ie.mode, file_path.filename().string(), ie.hash});
    }
    return t;
}

std::string Repository::create_commit(const std::string& message) {
    if (index_->entries().empty())
        throw std::runtime_error("nothing to commit (staging area is empty)");

    // Build tree from index
    // We build a flat index, create subtrees as needed
    // Simpler approach: build tree entries per directory
    std::unordered_map<std::string, Tree> trees;
    // First pass: group entries by directory
    std::map<std::string, std::vector<IndexEntry>> by_dir;
    for (auto& e : index_->entries()) {
        stdfs::path p(e.path);
        std::string dir = p.has_parent_path() ? p.parent_path().string() : "";
        by_dir[dir].push_back(e);
    }

    // Build bottom-up
    std::map<std::string, Tree, std::greater<std::string>> dir_trees;
    for (auto& [dir, entries] : by_dir) {
        Tree t;
        for (auto& e : entries) {
            TreeEntry te;
            te.mode = e.mode;
            te.name = stdfs::path(e.path).filename().string();
            te.hash = e.hash;
            t.add_entry(te);
        }
        dir_trees[dir] = std::move(t);
    }
    // Fold subtrees into parents
    for (auto& [dir, tree] : dir_trees) {
        if (dir.empty()) continue;
        stdfs::path dp(dir);
        std::string parent = dp.has_parent_path() ? dp.parent_path().string() : "";
        auto& parent_tree = dir_trees[parent];
        auto hash = write_object(tree);
        TreeEntry te;
        te.mode = "040000";
        te.name = dp.filename().string();
        te.hash = hash;
        parent_tree.add_entry(te);
    }

    auto& root_tree = dir_trees[""];
    auto tree_hash = write_object(root_tree);

    std::string parent_hash = refs_->resolve_head();
    std::vector<std::string> parents;
    if (!parent_hash.empty()) parents.push_back(parent_hash);

    std::string name = config_.user_name.empty() ? "Unknown" : config_.user_name;
    std::string email = config_.user_email.empty() ? "unknown@example.com" : config_.user_email;
    auto sig = Signature::now(name, email);
    Commit c(tree_hash, parents, sig, sig, message);
    auto commit_hash = write_object(c);

    // Update branch ref
    if (!refs_->head_is_detached()) {
        auto head_ref = refs_->read_head();
        // head_ref is "refs/heads/<branch-name>" — strip the prefix
        std::string branch_name = head_ref;
        const std::string prefix = "refs/heads/";
        if (head_ref.substr(0, prefix.size()) == prefix)
            branch_name = head_ref.substr(prefix.size());
        refs_->write_branch(branch_name, commit_hash);
    } else {
        refs_->set_head_detached(commit_hash);
    }

    return commit_hash;
}

void Repository::flatten_tree(const std::string& tree_hash, const std::string& prefix,
                               std::unordered_map<std::string, std::string>& out) const {
    if (!has_object(tree_hash)) return;
    auto tree = read_tree(tree_hash);
    for (auto& e : tree.entries()) {
        auto path = prefix.empty() ? e.name : prefix + "/" + e.name;
        if (e.is_tree())
            flatten_tree(e.hash, path, out);
        else
            out[path] = e.hash;
    }
}

std::unordered_map<std::string, std::string> Repository::head_tree_flat() const {
    std::unordered_map<std::string, std::string> result;
    auto head_hash = refs_->resolve_head();
    if (head_hash.empty()) return result;
    if (!has_object(head_hash)) return result;
    auto commit = read_commit(head_hash);
    flatten_tree(commit.tree(), "", result);
    return result;
}

std::vector<StatusEntry> Repository::status() const {
    auto head_files = head_tree_flat();
    std::vector<StatusEntry> result;
    std::set<std::string> seen;

    for (auto& ie : index_->entries()) {
        seen.insert(ie.path);
        StatusEntry se;
        se.path = ie.path;

        auto head_it = head_files.find(ie.path);
        if (head_it == head_files.end())
            se.index_state = StatusEntry::State::Added;
        else if (head_it->second != ie.hash)
            se.index_state = StatusEntry::State::Modified;
        else
            se.index_state = StatusEntry::State::Unmodified;

        // Working tree state
        auto abs = work_dir_ / ie.path;
        if (!stdfs::exists(abs)) {
            se.work_state = StatusEntry::State::Deleted;
        } else {
            auto data = util::fs::read_file(abs);
            Blob b(data);
            auto cur_hash = b.hash();
            se.work_state = (cur_hash == ie.hash) ? StatusEntry::State::Unmodified
                                                   : StatusEntry::State::Modified;
        }
        result.push_back(se);
    }

    // Files in HEAD but not in index
    for (auto& [path, hash] : head_files) {
        if (seen.count(path)) continue;
        StatusEntry se;
        se.path = path;
        se.index_state = StatusEntry::State::Deleted;
        se.work_state = stdfs::exists(work_dir_ / path) ? StatusEntry::State::Unmodified
                                                         : StatusEntry::State::Deleted;
        result.push_back(se);
    }

    // Untracked files
    auto all_files = util::fs::list_files_recursive(work_dir_);
    for (auto& f : all_files) {
        auto rel = stdfs::relative(f, work_dir_).string();
        if (rel.find(".nvcs/") == 0 || rel == ".nvcs") continue;
        if (seen.count(rel) || head_files.count(rel)) continue;
        StatusEntry se;
        se.path = rel;
        se.index_state = StatusEntry::State::Untracked;
        se.work_state = StatusEntry::State::Untracked;
        result.push_back(se);
    }

    return result;
}

std::string Repository::current_branch() const {
    if (refs_->head_is_detached()) return "";
    auto head = refs_->read_head();
    const std::string prefix = "refs/heads/";
    if (head.substr(0, prefix.size()) == prefix)
        return head.substr(prefix.size());
    return head;
}

std::string Repository::current_commit() const {
    return refs_->resolve_head();
}

void Repository::create_branch(const std::string& name, const std::string& from) {
    auto hash = from.empty() ? refs_->resolve_head() : refs_->resolve(from);
    if (hash.empty()) hash = from;
    refs_->write_branch(name, hash);
}

void Repository::delete_branch(const std::string& name) {
    refs_->delete_branch(name);
}

void Repository::checkout_tree(const std::string& tree_hash, const stdfs::path& dir) {
    auto tree = read_tree(tree_hash);
    for (auto& e : tree.entries()) {
        auto path = dir / e.name;
        if (e.is_tree()) {
            util::fs::ensure_dir(path);
            checkout_tree(e.hash, path);
        } else {
            auto blob = read_blob(e.hash);
            util::fs::write_file(path, blob.data());
            if (e.mode == "100755") {
                stdfs::permissions(path,
                    stdfs::perms::owner_exec | stdfs::perms::group_exec |
                    stdfs::perms::others_exec,
                    stdfs::perm_options::add);
            }
        }
    }
}

void Repository::checkout(const std::string& ref) {
    auto hash = refs_->resolve(ref);
    if (hash.empty()) {
        // Try as direct hash
        if (util::is_valid_hash(ref) && has_object(ref)) hash = ref;
        else throw std::runtime_error("unknown ref: " + ref);
    }
    auto commit = read_commit(hash);
    checkout_tree(commit.tree(), work_dir_);

    // Rebuild index from checked-out tree
    index_ = std::make_unique<Index>(nvcs_dir_ / "index");
    std::unordered_map<std::string, std::string> flat;
    flatten_tree(commit.tree(), "", flat);
    for (auto& [path, blob_hash] : flat) {
        IndexEntry e;
        e.path = path;
        e.hash = blob_hash;
        e.mode = "100644";
        e.staged = true;
        auto abs = work_dir_ / path;
        e.size = stdfs::exists(abs) ? stdfs::file_size(abs) : 0;
        e.mtime = 0;
        index_->stage(e);
    }
    index_->save();

    // Update HEAD
    auto branch = refs_->read_branch(ref);
    if (branch)
        refs_->set_head_branch(ref);
    else
        refs_->set_head_detached(hash);
}

std::vector<std::string> Repository::list_branches() const {
    return refs_->list_branches();
}

std::vector<Commit> Repository::log(const std::string& from, int max) const {
    std::string start = from.empty() ? refs_->resolve_head() : refs_->resolve(from);
    if (start.empty()) return {};

    std::vector<Commit> result;
    std::set<std::string> visited;
    std::vector<std::string> queue = {start};

    while (!queue.empty() && (max < 0 || (int)result.size() < max)) {
        auto h = queue.back(); queue.pop_back();
        if (visited.count(h) || h.empty()) continue;
        if (!has_object(h)) break;
        visited.insert(h);
        auto c = read_commit(h);
        result.push_back(c);
        for (auto& p : c.parents()) queue.push_back(p);
    }
    return result;
}

void Repository::collect_reachable(const std::string& hash, std::vector<std::string>& out) const {
    if (hash.empty() || !has_object(hash)) return;
    std::set<std::string> seen(out.begin(), out.end());
    std::vector<std::string> stack = {hash};
    while (!stack.empty()) {
        auto h = stack.back(); stack.pop_back();
        if (seen.count(h)) continue;
        seen.insert(h);
        out.push_back(h);
        auto obj = read_object(h);
        if (auto* c = dynamic_cast<Commit*>(obj.get())) {
            stack.push_back(c->tree());
            for (auto& p : c->parents()) stack.push_back(p);
        } else if (auto* t = dynamic_cast<Tree*>(obj.get())) {
            for (auto& e : t->entries()) stack.push_back(e.hash);
        }
    }
}

std::vector<uint8_t> Repository::create_pack(const std::vector<std::string>& hashes) const {
    // Pack format: [4-byte count][entry*]
    // Entry: [4-byte hash_len][hash][4-byte data_len][compressed_data]
    std::vector<std::string> all;
    for (auto& h : hashes) collect_reachable(h, all);

    std::vector<uint8_t> pack;
    auto write_u32 = [&](uint32_t v) {
        pack.push_back((v >> 24) & 0xFF);
        pack.push_back((v >> 16) & 0xFF);
        pack.push_back((v >>  8) & 0xFF);
        pack.push_back((v      ) & 0xFF);
    };

    write_u32(static_cast<uint32_t>(all.size()));
    for (auto& h : all) {
        auto path = object_path(h);
        if (!stdfs::exists(path)) continue;
        auto data = util::fs::read_file(path); // already compressed
        write_u32(static_cast<uint32_t>(h.size()));
        pack.insert(pack.end(), h.begin(), h.end());
        write_u32(static_cast<uint32_t>(data.size()));
        pack.insert(pack.end(), data.begin(), data.end());
    }
    return pack;
}

void Repository::apply_pack(const std::vector<uint8_t>& pack_data) {
    if (pack_data.size() < 4)
        throw std::runtime_error("invalid pack: too short");
    size_t pos = 0;
    auto read_u32 = [&]() -> uint32_t {
        if (pos + 4 > pack_data.size()) throw std::runtime_error("pack truncated");
        uint32_t v = ((uint32_t)pack_data[pos] << 24) |
                     ((uint32_t)pack_data[pos+1] << 16) |
                     ((uint32_t)pack_data[pos+2] << 8)  |
                     ((uint32_t)pack_data[pos+3]);
        pos += 4;
        return v;
    };

    uint32_t count = read_u32();
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t hash_len = read_u32();
        if (pos + hash_len > pack_data.size()) throw std::runtime_error("pack truncated hash");
        std::string hash(pack_data.begin() + pos, pack_data.begin() + pos + hash_len);
        pos += hash_len;

        uint32_t data_len = read_u32();
        if (pos + data_len > pack_data.size()) throw std::runtime_error("pack truncated data");
        std::vector<uint8_t> data(pack_data.begin() + pos, pack_data.begin() + pos + data_len);
        pos += data_len;

        auto path = object_path(hash);
        if (!stdfs::exists(path))
            util::fs::write_file(path, data);
    }
}

} // namespace nvcs::core

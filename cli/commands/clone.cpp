#include "nvcs/core/repository.hpp"
#include "nvcs/net/transport.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>

int cmd_clone(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    if (args.empty()) { std::cerr << "Usage: nvcs clone <url> [dir]\n"; return 1; }

    std::string url_str = args[0];
    auto remote_url = nvcs::net::RemoteURL::parse(url_str);

    fs::path dest = args.size() > 1 ? fs::path(args[1]) : fs::path(remote_url.repo_name);
    if (fs::exists(dest / ".nvcs")) {
        std::cerr << "Destination already exists and is a repository\n"; return 1;
    }
    fs::create_directories(dest);
    std::cout << "Cloning into '" << dest.filename().string() << "'...\n";

    nvcs::net::HttpTransport transport(remote_url);

    // Get remote refs
    auto adv = transport.fetch_refs();
    if (adv.refs.empty()) {
        std::cerr << "Remote repository is empty\n"; return 1;
    }

    // Find default branch HEAD
    std::string default_branch = adv.default_branch.empty() ? "main" : adv.default_branch;
    std::string head_hash;
    {
        auto it = adv.refs.find("refs/heads/" + default_branch);
        if (it != adv.refs.end()) head_hash = it->second;
        else {
            // Fallback: first branch
            for (auto& [ref, hash] : adv.refs) {
                if (ref.substr(0, 11) == "refs/heads/") {
                    default_branch = ref.substr(11);
                    head_hash = hash;
                    break;
                }
            }
        }
    }
    if (head_hash.empty()) { std::cerr << "Cannot determine remote HEAD\n"; return 1; }

    // Download all objects
    nvcs::net::UploadRequest req;
    for (auto& [ref, hash] : adv.refs)
        req.wants.push_back(hash);
    // deduplicate
    std::sort(req.wants.begin(), req.wants.end());
    req.wants.erase(std::unique(req.wants.begin(), req.wants.end()), req.wants.end());

    auto pack = transport.upload_pack(req);

    // Initialize repository and apply pack
    auto repo = nvcs::core::Repository::init(dest, default_branch);
    repo.apply_pack(pack);

    // Set up refs
    for (auto& [ref, hash] : adv.refs) {
        if (ref.substr(0, 11) == "refs/heads/") {
            auto branch = ref.substr(11);
            repo.refs().write_branch(branch, hash);
            repo.refs().write_remote_ref("origin", branch, hash);
        } else if (ref.substr(0, 10) == "refs/tags/") {
            repo.refs().write_tag(ref.substr(10), hash);
        }
    }

    // Add origin remote
    repo.config().remotes["origin"] = url_str;
    repo.config().save(repo.nvcs_dir() / "config");

    // Checkout default branch
    repo.checkout(default_branch);
    std::cout << "Done. Checked out '" << default_branch << "'\n";
    return 0;
}

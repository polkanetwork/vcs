#include "nvcs/core/repository.hpp"
#include "nvcs/net/transport.hpp"
#include <iostream>
#include <filesystem>

int cmd_pull(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    std::string remote_name = args.size() > 0 ? args[0] : "origin";
    auto repo = nvcs::core::Repository::open(fs::current_path());

    std::string branch = args.size() > 1 ? args[1] : repo.current_branch();
    if (branch.empty()) { std::cerr << "Cannot pull in detached HEAD state\n"; return 1; }

    auto& cfg = repo.config();
    auto it = cfg.remotes.find(remote_name);
    if (it == cfg.remotes.end()) {
        std::cerr << "Remote not found: " << remote_name << "\n"; return 1;
    }

    std::cout << "Pulling " << branch << " from " << remote_name << "...\n";

    auto url = nvcs::net::RemoteURL::parse(it->second);
    nvcs::net::HttpTransport transport(url);

    // Get remote refs
    auto adv = transport.fetch_refs();
    auto ref_key = "refs/heads/" + branch;
    auto ri = adv.refs.find(ref_key);
    if (ri == adv.refs.end()) {
        std::cerr << "Remote branch not found: " << branch << "\n"; return 1;
    }

    auto remote_hash = ri->second;
    auto local_hash = repo.refs().resolve(branch);

    if (remote_hash == local_hash) {
        std::cout << "Already up to date.\n"; return 0;
    }

    // Download objects
    nvcs::net::UploadRequest req;
    req.wants = {remote_hash};
    if (!local_hash.empty()) req.haves = {local_hash};

    auto pack = transport.upload_pack(req);
    repo.apply_pack(pack);

    // Fast-forward or create branch
    repo.refs().write_branch(branch, remote_hash);
    repo.refs().write_remote_ref(remote_name, branch, remote_hash);

    // Update working tree if on the same branch
    if (repo.current_branch() == branch) {
        repo.checkout(branch);
        std::cout << "Fast-forwarded " << branch << " to " << remote_hash.substr(0, 12) << "\n";
    } else {
        std::cout << "Updated " << remote_name << "/" << branch << "\n";
    }
    return 0;
}

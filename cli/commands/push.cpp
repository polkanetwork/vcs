#include "nvcs/core/repository.hpp"
#include "nvcs/net/transport.hpp"
#include <iostream>
#include <filesystem>

int cmd_push(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    std::string remote_name = args.size() > 0 ? args[0] : "origin";
    std::string branch;
    auto repo = nvcs::core::Repository::open(fs::current_path());

    if (args.size() > 1) branch = args[1];
    else branch = repo.current_branch();
    if (branch.empty()) { std::cerr << "Cannot push in detached HEAD state\n"; return 1; }

    auto& cfg = repo.config();
    auto it = cfg.remotes.find(remote_name);
    if (it == cfg.remotes.end()) {
        std::cerr << "Remote not found: " << remote_name << "\n"; return 1;
    }

    auto local_hash = repo.refs().resolve(branch);
    if (local_hash.empty()) { std::cerr << "Branch has no commits: " << branch << "\n"; return 1; }

    std::cout << "Pushing " << branch << " to " << remote_name << "...\n";

    auto url = nvcs::net::RemoteURL::parse(it->second);
    nvcs::net::HttpTransport transport(url);

    // Fetch remote refs to find what they have
    std::string remote_hash;
    try {
        auto adv = transport.fetch_refs();
        auto ref_key = "refs/heads/" + branch;
        auto ri = adv.refs.find(ref_key);
        if (ri != adv.refs.end()) remote_hash = ri->second;
    } catch (...) {}

    if (remote_hash == local_hash) {
        std::cout << "Everything up-to-date.\n";
        return 0;
    }

    // Build pack of new objects
    auto pack = repo.create_pack({local_hash});

    nvcs::net::ReceiveRequest req;
    nvcs::net::ReceiveRequest::Update upd;
    upd.ref_name = "refs/heads/" + branch;
    upd.old_hash = remote_hash;
    upd.new_hash = local_hash;
    req.updates.push_back(upd);
    req.pack_data = pack;

    transport.receive_pack(req);
    repo.refs().write_remote_ref(remote_name, branch, local_hash);
    std::cout << branch << " -> " << remote_name << "/" << branch << "\n";
    return 0;
}

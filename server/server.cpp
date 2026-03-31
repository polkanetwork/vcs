#include "server.hpp"
#include <nvcs/server_utils.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace nvcs::server {

static fs::path repo_path(const std::string& name, const std::string& repo_dir) {
    if (!nvcs::server_utils::validate_repo_name(name))
        throw std::invalid_argument("invalid repo name: " + name);
    return fs::path(repo_dir) / name;
}

void setup_routes(httplib::Server& svr, const std::string& repo_dir, std::mutex& repo_mutex) {
    auto make_repo_path = [repo_dir](const std::string& name) {
        return repo_path(name, repo_dir);
    };

    svr.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"server\":\"nvcs/1.0\"}", "application/json");
    });

    svr.Get("/repos", [repo_dir](const httplib::Request&, httplib::Response& res) {
        json repos = json::array();
        for (auto& name : nvcs::server_utils::list_repositories(repo_dir)) {
            repos.push_back(name);
        }
        json resp;
        resp["repos"] = repos;
        res.set_content(resp.dump(), "application/json");
    });

    svr.Post("/repos", [repo_dir](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string name = j["name"].get<std::string>();
            auto path = repo_path(name, repo_dir);
            if (fs::exists(path / ".nvcs")) {
                res.status = 409;
                res.set_content("{\"error\":\"repository already exists\"}", "application/json");
                return;
            }
            fs::create_directories(path);
            core::Repository::init(path);
            json resp;
            resp["name"] = name;
            resp["path"] = fs::absolute(path).string();
            res.status = 201;
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err;
            err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Delete(R"(/repos/([^/]+))", [repo_dir](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name, repo_dir);
            if (!fs::exists(path / ".nvcs")) {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
                return;
            }
            fs::remove_all(path);
            res.set_content("{\"deleted\":true}", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err;
            err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Get(R"(/repos/([^/]+)/info/refs)", [repo_dir](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name, repo_dir);
            if (!fs::exists(path / ".nvcs")) {
                res.status = 404;
                res.set_content("{\"error\":\"repository not found\"}", "application/json");
                return;
            }
            core::Repository repo(path);
            net::RefAdvertisement adv;
            adv.server_version = "nvcs/1.0";
            adv.default_branch = repo.config().default_branch;

            for (auto& b : repo.refs().list_branches()) {
                auto hash = repo.refs().read_branch(b);
                if (hash)
                    adv.refs["refs/heads/" + b] = *hash;
            }
            for (auto& t : repo.refs().list_tags()) {
                auto hash = repo.refs().read_tag(t);
                if (hash)
                    adv.refs["refs/tags/" + t] = *hash;
            }
            res.set_content(net::serialize_ref_adv(adv), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            json err;
            err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    svr.Post(R"(/repos/([^/]+)/upload-pack)", [repo_dir](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name, repo_dir);
            if (!fs::exists(path / ".nvcs")) {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
                return;
            }
            core::Repository repo(path);
            auto upload_req = net::parse_upload_req(req.body);
            std::vector<std::string> send_hashes;
            for (auto& w : upload_req.wants) {
                if (repo.has_object(w))
                    send_hashes.push_back(w);
            }
            net::PackResponse resp;
            resp.success = true;
            resp.pack_data = repo.create_pack(send_hashes);
            res.set_content(net::serialize_pack_response(resp), "application/json");
        } catch (const std::exception& e) {
            net::PackResponse resp;
            resp.success = false;
            resp.error_message = e.what();
            res.status = 500;
            res.set_content(net::serialize_pack_response(resp), "application/json");
        }
    });

    svr.Post(R"(/repos/([^/]+)/receive-pack)", [&repo_dir, &repo_mutex](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name, repo_dir);
            std::lock_guard<std::mutex> lock(repo_mutex);
            if (!fs::exists(path / ".nvcs")) {
                fs::create_directories(path);
                core::Repository::init(path);
                std::cout << "Auto-created repository: " << name << "\n";
            }
            core::Repository repo(path);
            auto recv_req = net::parse_receive_req(req.body);
            repo.apply_pack(recv_req.pack_data);
            for (auto& upd : recv_req.updates) {
                if (upd.ref_name.rfind("refs/heads/", 0) == 0) {
                    auto branch = upd.ref_name.substr(11);
                    auto existing = repo.refs().read_branch(branch);
                    if (!upd.old_hash.empty() && existing && *existing != upd.old_hash) {
                        net::PackResponse resp;
                        resp.success = false;
                        resp.error_message = "non-fast-forward push rejected for " + branch;
                        res.status = 409;
                        res.set_content(net::serialize_pack_response(resp), "application/json");
                        return;
                    }
                    repo.refs().write_branch(branch, upd.new_hash);
                } else if (upd.ref_name.rfind("refs/tags/", 0) == 0) {
                    repo.refs().write_tag(upd.ref_name.substr(10), upd.new_hash);
                }
            }
            net::PackResponse resp;
            resp.success = true;
            res.set_content(net::serialize_pack_response(resp), "application/json");
        } catch (const std::exception& e) {
            net::PackResponse resp;
            resp.success = false;
            resp.error_message = e.what();
            res.status = 500;
            res.set_content(net::serialize_pack_response(resp), "application/json");
        }
    });

    svr.Get(R"(/repos/([^/]+))", [repo_dir](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name, repo_dir);
            if (!fs::exists(path / ".nvcs")) {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
                return;
            }
            core::Repository repo(path);
            json info;
            info["name"] = name;
            info["branches"] = repo.list_branches();
            info["default_branch"] = repo.config().default_branch;
            auto head = repo.current_commit();
            if (!head.empty())
                info["head"] = head;
            res.set_content(info.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            json err;
            err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });
}
}

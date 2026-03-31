#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../third_party/httplib.h"
#include "../third_party/json.hpp"
#include "nvcs/core/repository.hpp"
#include "nvcs/net/protocol.hpp"
#include <iostream>
#include <filesystem>
#include <string>
#include <mutex>
#include <unordered_map>
#include <csignal>
#include <atomic>

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace nvcs;

static std::atomic<bool> g_running{true};
static std::string g_repo_dir = "./nvcs-repos";

static std::mutex g_repo_mutex;

static fs::path repo_path(const std::string& name) {
    // Sanitize name
    for (char c : name)
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '.')
            throw std::invalid_argument("invalid repo name: " + name);
    return fs::path(g_repo_dir) / name;
}

static void setup_routes(httplib::Server& svr) {

    // ── Health check ─────────────────────────────────────────────────────────
    svr.Get("/healthz", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\",\"server\":\"nvcs/1.0\"}", "application/json");
    });

    // ── List repositories ────────────────────────────────────────────────────
    svr.Get("/repos", [](const httplib::Request&, httplib::Response& res) {
        json repos = json::array();
        if (fs::exists(g_repo_dir)) {
            for (auto& entry : fs::directory_iterator(g_repo_dir)) {
                if (entry.is_directory() && fs::exists(entry.path() / ".nvcs"))
                    repos.push_back(entry.path().filename().string());
            }
        }
        json resp;
        resp["repos"] = repos;
        res.set_content(resp.dump(), "application/json");
    });

    // ── Create repository ────────────────────────────────────────────────────
    svr.Post("/repos", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string name = j["name"].get<std::string>();
            auto path = repo_path(name);
            if (fs::exists(path / ".nvcs")) {
                res.status = 409;
                res.set_content("{\"error\":\"repository already exists\"}", "application/json");
                return;
            }
            fs::create_directories(path);
            core::Repository::init(path);
            json resp;
            resp["name"] = name;
            resp["path"] = path.string();
            res.status = 201;
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err; err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── Delete repository ────────────────────────────────────────────────────
    svr.Delete(R"(/repos/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name);
            if (!fs::exists(path / ".nvcs")) {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
                return;
            }
            fs::remove_all(path);
            res.set_content("{\"deleted\":true}", "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            json err; err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── Advertise refs ───────────────────────────────────────────────────────
    svr.Get(R"(/repos/([^/]+)/info/refs)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name);
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
                if (hash) adv.refs["refs/heads/" + b] = *hash;
            }
            for (auto& t : repo.refs().list_tags()) {
                auto hash = repo.refs().read_tag(t);
                if (hash) adv.refs["refs/tags/" + t] = *hash;
            }

            res.set_content(net::serialize_ref_adv(adv), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            json err; err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── Upload pack (client fetches/pulls) ───────────────────────────────────
    svr.Post(R"(/repos/([^/]+)/upload-pack)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name);
            if (!fs::exists(path / ".nvcs")) {
                res.status = 404; res.set_content("{\"error\":\"not found\"}", "application/json"); return;
            }
            core::Repository repo(path);
            auto upload_req = net::parse_upload_req(req.body);

            // Build pack of wanted objects minus haves
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
            net::PackResponse resp; resp.success = false; resp.error_message = e.what();
            res.status = 500;
            res.set_content(net::serialize_pack_response(resp), "application/json");
        }
    });

    // ── Receive pack (client pushes) ─────────────────────────────────────────
    svr.Post(R"(/repos/([^/]+)/receive-pack)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name);
            std::lock_guard<std::mutex> lock(g_repo_mutex);
            // Auto-create repository on first push (like hosted VCS services)
            if (!fs::exists(path / ".nvcs")) {
                fs::create_directories(path);
                core::Repository::init(path);
                std::cout << "Auto-created repository: " << name << "\n";
            }
            core::Repository repo(path);
            auto recv_req = net::parse_receive_req(req.body);

            repo.apply_pack(recv_req.pack_data);

            for (auto& upd : recv_req.updates) {
                if (upd.ref_name.substr(0, 11) == "refs/heads/") {
                    auto branch = upd.ref_name.substr(11);
                    auto existing = repo.refs().read_branch(branch);
                    if (!upd.old_hash.empty() && existing && *existing != upd.old_hash) {
                        net::PackResponse resp; resp.success = false;
                        resp.error_message = "non-fast-forward push rejected for " + branch;
                        res.status = 409;
                        res.set_content(net::serialize_pack_response(resp), "application/json");
                        return;
                    }
                    repo.refs().write_branch(branch, upd.new_hash);
                } else if (upd.ref_name.substr(0, 10) == "refs/tags/") {
                    repo.refs().write_tag(upd.ref_name.substr(10), upd.new_hash);
                }
            }

            net::PackResponse resp; resp.success = true;
            res.set_content(net::serialize_pack_response(resp), "application/json");
        } catch (const std::exception& e) {
            net::PackResponse resp; resp.success = false; resp.error_message = e.what();
            res.status = 500;
            res.set_content(net::serialize_pack_response(resp), "application/json");
        }
    });

    // ── Get repo info ────────────────────────────────────────────────────────
    svr.Get(R"(/repos/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string name = req.matches[1];
            auto path = repo_path(name);
            if (!fs::exists(path / ".nvcs")) {
                res.status = 404; res.set_content("{\"error\":\"not found\"}", "application/json"); return;
            }
            core::Repository repo(path);
            json info;
            info["name"] = name;
            info["branches"] = repo.list_branches();
            info["default_branch"] = repo.config().default_branch;
            auto head = repo.current_commit();
            if (!head.empty()) info["head"] = head;
            res.set_content(info.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500; json err; err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });
}

static void print_banner(const std::string& host, int port) {
    std::cout << "\n";
    std::cout << "  ███╗   ██╗██╗   ██╗ ██████╗███████╗\n";
    std::cout << "  ████╗  ██║██║   ██║██╔════╝██╔════╝\n";
    std::cout << "  ██╔██╗ ██║██║   ██║██║     ███████╗\n";
    std::cout << "  ██║╚██╗██║╚██╗ ██╔╝██║     ╚════██║\n";
    std::cout << "  ██║ ╚████║ ╚████╔╝ ╚██████╗███████║\n";
    std::cout << "  ╚═╝  ╚═══╝  ╚═══╝   ╚═════╝╚══════╝\n";
    std::cout << "\n  NVCS Server v1.0 — listening on " << host << ":" << port << "\n";
    std::cout << "  Repository storage: " << fs::absolute(g_repo_dir).string() << "\n\n";
}

int main(int argc, char** argv) {
    std::string host = "0.0.0.0";
    int port = 7878;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--host" && i + 1 < argc) host = argv[++i];
        else if (arg == "--repo-dir" && i + 1 < argc) g_repo_dir = argv[++i];
        else if (arg == "--help") {
            std::cout << "Usage: nvcs-server [--host HOST] [--port PORT] [--repo-dir DIR]\n";
            return 0;
        }
    }

    // Ensure repo directory exists
    fs::create_directories(g_repo_dir);

    httplib::Server svr;
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << req.method << " " << req.path << " -> " << res.status << "\n";
    });
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        json err; err["error"] = "internal server error";
        res.set_content(err.dump(), "application/json");
    });

    setup_routes(svr);
    print_banner(host, port);

    signal(SIGINT, [](int) {
        std::cout << "\nShutting down...\n";
        exit(0);
    });

    if (!svr.listen(host.c_str(), port)) {
        std::cerr << "Failed to start server on " << host << ":" << port << "\n";
        return 1;
    }
    return 0;
}

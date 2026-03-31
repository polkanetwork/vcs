#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "server.hpp"
#include <iostream>
#include <filesystem>
#include <string>
#include <mutex>
#include <csignal>
#include <atomic>

namespace fs = std::filesystem;
using namespace nvcs;

static std::atomic<bool> g_running{true};
static std::string g_repo_dir = "./nvcs-repos";
static std::mutex g_repo_mutex;

static void print_banner(const std::string& host, int port);

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

    fs::create_directories(g_repo_dir);

    httplib::Server svr;
    svr.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::cout << req.method << " " << req.path << " -> " << res.status << "\n";
    });
    svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        nlohmann::json err; err["error"] = "internal server error";
        res.set_content(err.dump(), "application/json");
    });

    nvcs::server::setup_routes(svr, g_repo_dir, g_repo_mutex);
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


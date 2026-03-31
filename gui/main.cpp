#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_IMPLEMENTATION
#include "nuklear.h"

#include <cassert>
#include <cstring>
#ifndef NK_MEMSET
#define NK_MEMSET memset
#endif
#ifndef NK_MEMCPY
#define NK_MEMCPY memcpy
#endif
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>
#define NK_GLFW_GL3_IMPLEMENTATION
#include "nuklear_glfw_gl3.h"

#include <httplib.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../server/server.hpp"

using namespace nvcs;
namespace fs = std::filesystem;

static struct nk_glfw glfw = {};

static void append_log(std::string& storage, const std::string& line, std::mutex& mutex) {
    std::lock_guard lock(mutex);
    storage += line;
    storage += '\n';
    if (storage.size() > 32 * 1024)
        storage.erase(0, storage.size() - 32 * 1024);
}

static bool validate_repo_name(const std::string& name) {
    if (name.empty())
        return false;
    for (char c : name) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (!std::isalnum(uc) && c != '-' && c != '_' && c != '.')
            return false;
    }
    return true;
}

static void refresh_repo_list(const std::string& repo_dir, std::vector<std::string>& repos) {
    repos.clear();
    if (!fs::exists(repo_dir))
        return;
    for (auto& entry : fs::directory_iterator(repo_dir)) {
        if (entry.is_directory() && fs::exists(entry.path() / ".nvcs"))
            repos.push_back(entry.path().filename().string());
    }
    std::sort(repos.begin(), repos.end());
}

static std::string build_repo_summary(const std::string& repo_dir, const std::string& name) {
    if (name.empty())
        return "";
    auto path = fs::path(repo_dir) / name;
    if (!fs::exists(path / ".nvcs"))
        return "Repository not found.";
    try {
        core::Repository repo(path);
        auto branches = repo.refs().list_branches();
        auto tags = repo.refs().list_tags();
        std::string result = "Branches: " + std::to_string(branches.size()) + "\n";
        result += "Tags: " + std::to_string(tags.size()) + "\n";
        result += "Default branch: " + repo.config().default_branch + "\n";
        auto head = repo.current_commit();
        if (!head.empty())
            result += "HEAD: " + head + "\n";
        return result;
    } catch (const std::exception& e) {
        return std::string("Failed to read repository: ") + e.what();
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    GLFWwindow* window = glfwCreateWindow(1024, 760, "NVCS Server GUI", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    struct nk_context* ctx = nk_glfw3_init(&glfw, window, NK_GLFW3_INSTALL_CALLBACKS);
    struct nk_font_atlas* atlas;
    nk_glfw3_font_stash_begin(&glfw, &atlas);
    struct nk_font* font = nk_font_atlas_add_default(atlas, 14, 0);
    nk_glfw3_font_stash_end(&glfw);
    nk_style_set_font(ctx, &font->handle);

    char repo_dir_buf[256] = "./nvcs-repos";
    char host_buf[64] = "0.0.0.0";
    char port_buf[16] = "7878";
    char new_repo_buf[128] = "";
    char selected_repo_buf[128] = "";

    std::string logs;
    std::mutex logs_mutex;
    std::mutex repo_mutex;
    std::vector<std::string> repo_names;
    std::shared_ptr<httplib::Server> server;
    std::thread server_thread;
    std::atomic<bool> server_running{false};

    refresh_repo_list(repo_dir_buf, repo_names);

    auto start_gui_server = [&](const std::string& host, int port, const std::string& repo_dir) {
        if (server_running)
            return;
        fs::create_directories(repo_dir);
        server = std::make_shared<httplib::Server>();
        server->set_logger([&](const httplib::Request& req, const httplib::Response& res) {
            append_log(logs, req.method + " " + req.path + " -> " + std::to_string(res.status), logs_mutex);
        });
        server->set_error_handler([&](const httplib::Request&, httplib::Response& res) {
            nlohmann::json err;
            err["error"] = "internal server error";
            res.set_content(err.dump(), "application/json");
        });
        nvcs::server::setup_routes(*server, repo_dir, repo_mutex);

        server_running = true;
        append_log(logs, "Starting NVCS server at " + host + ":" + std::to_string(port), logs_mutex);

        server_thread = std::thread([server, host, port, &logs, &logs_mutex, &server_running] {
            if (!server->listen(host.c_str(), port)) {
                append_log(logs, "Failed to bind server on " + host + ":" + std::to_string(port), logs_mutex);
            }
            server_running = false;
            append_log(logs, "NVCS server stopped.", logs_mutex);
        });
    };

    auto stop_gui_server = [&] {
        if (!server_running || !server)
            return;
        append_log(logs, "Stopping NVCS server...", logs_mutex);
        server->stop();
        if (server_thread.joinable())
            server_thread.join();
        server_running = false;
        server.reset();
    };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        nk_glfw3_new_frame(&glfw);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.11f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (nk_begin(ctx, "NVCS Server", nk_rect(20, 20, 980, 720), NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
            nk_layout_row_dynamic(ctx, 22, 1);
            nk_label(ctx, "Server configuration", NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 24, 1);
            nk_label(ctx, "Repository root directory:", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, repo_dir_buf, sizeof(repo_dir_buf), nk_filter_default);

            nk_layout_row_dynamic(ctx, 24, 2);
            nk_label(ctx, "Host:", NK_TEXT_LEFT);
            nk_label(ctx, host_buf, NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 24, 2);
            nk_label(ctx, "Port:", NK_TEXT_LEFT);
            nk_label(ctx, port_buf, NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 30, 3);
            if (!server_running && nk_button_label(ctx, "Start Server")) {
                int port = std::atoi(port_buf);
                if (port <= 0) port = 7878;
                start_gui_server(host_buf, port, repo_dir_buf);
            }
            if (server_running && nk_button_label(ctx, "Stop Server")) {
                stop_gui_server();
            }
            if (nk_button_label(ctx, "Refresh repos")) {
                refresh_repo_list(repo_dir_buf, repo_names);
            }

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, server_running ? "Server is running" : "Server is stopped", NK_TEXT_LEFT);
            {
                std::string root_label = std::string("Repo root: ") + repo_dir_buf;
                nk_label(ctx, root_label.c_str(), NK_TEXT_LEFT);
            }

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "Repository manager", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, new_repo_buf, sizeof(new_repo_buf), nk_filter_default);

            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Create repository")) {
                std::string new_repo = new_repo_buf;
                if (!validate_repo_name(new_repo)) {
                    append_log(logs, "Invalid repository name.", logs_mutex);
                } else {
                    try {
                        auto path = fs::path(repo_dir_buf) / new_repo;
                        if (fs::exists(path / ".nvcs")) {
                            append_log(logs, "Repository already exists: " + new_repo, logs_mutex);
                        } else {
                            fs::create_directories(path);
                            core::Repository::init(path);
                            append_log(logs, "Created repository: " + new_repo, logs_mutex);
                            refresh_repo_list(repo_dir_buf, repo_names);
                        }
                    } catch (const std::exception& e) {
                        append_log(logs, std::string("Create failed: ") + e.what(), logs_mutex);
                    }
                }
            }
            if (nk_button_label(ctx, "Delete selected")) {
                std::string selected = selected_repo_buf;
                if (!selected.empty()) {
                    try {
                        auto path = fs::path(repo_dir_buf) / selected;
                        if (fs::exists(path / ".nvcs")) {
                            fs::remove_all(path);
                            append_log(logs, "Deleted repository: " + selected, logs_mutex);
                            selected_repo_buf[0] = '\0';
                            refresh_repo_list(repo_dir_buf, repo_names);
                        } else {
                            append_log(logs, "Selected repository does not exist.", logs_mutex);
                        }
                    } catch (const std::exception& e) {
                        append_log(logs, std::string("Delete failed: ") + e.what(), logs_mutex);
                    }
                }
            }

            nk_layout_row_dynamic(ctx, 18, 1);
            nk_label(ctx, "Available repositories:", NK_TEXT_LEFT);
            for (auto& repo_name : repo_names) {
                nk_layout_row_dynamic(ctx, 22, 1);
                if (nk_button_label(ctx, repo_name.c_str())) {
                    std::strncpy(selected_repo_buf, repo_name.c_str(), sizeof(selected_repo_buf) - 1);
                    selected_repo_buf[sizeof(selected_repo_buf) - 1] = '\0';
                }
            }

            if (selected_repo_buf[0] != '\0') {
                nk_layout_row_dynamic(ctx, 20, 1);
                {
                    std::string selected_label = std::string("Selected repository: ") + selected_repo_buf;
                    nk_label(ctx, selected_label.c_str(), NK_TEXT_LEFT);
                }
                std::string summary = build_repo_summary(repo_dir_buf, selected_repo_buf);
                nk_layout_row_dynamic(ctx, 80, 1);
                nk_label_wrap(ctx, summary.c_str());
            }

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "Activity log:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 140, 1);
            {
                std::lock_guard lock(logs_mutex);
                nk_label_wrap(ctx, logs.c_str());
            }
        }
        nk_end(ctx);

        nk_glfw3_render(&glfw, NK_ANTI_ALIASING_ON, 512 * 1024, 128 * 1024);
        glfwSwapBuffers(window);
    }

    stop_gui_server();
    nk_glfw3_shutdown(&glfw);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

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

#include <nvcs/core/repository.hpp>
#include <nvcs/net/transport.hpp>
#include <httplib.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

using namespace nvcs;
namespace fs = std::filesystem;

static struct nk_glfw gui_glfw = {};

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
        std::string result = "Branches: " + std::to_string(branches.size()) + "\n";
        result += "Default branch: " + repo.config().default_branch + "\n";
        auto head = repo.current_commit();
        if (!head.empty())
            result += "HEAD: " + head + "\n";
        if (!repo.config().remotes.empty()) {
            result += "Remotes:\n";
            for (auto& [name, url] : repo.config().remotes)
                result += "  " + name + " = " + url + "\n";
        }
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

    GLFWwindow* window = glfwCreateWindow(1024, 760, "NVCS Client GUI", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    struct nk_context* ctx = nk_glfw3_init(&gui_glfw, window, NK_GLFW3_INSTALL_CALLBACKS);
    struct nk_font_atlas* atlas;
    nk_glfw3_font_stash_begin(&gui_glfw, &atlas);
    struct nk_font* font = nk_font_atlas_add_default(atlas, 14, 0);
    nk_glfw3_font_stash_end(&gui_glfw);
    nk_style_set_font(ctx, &font->handle);

    char repo_dir_buf[256] = "./nvcs-repos";
    char remote_url_buf[512] = "http://localhost:7878/repos/my-project";
    char clone_dest_buf[256] = "./clone-target";
    char selected_repo_buf[128] = "";

    std::string logs;
    std::string remote_refs;
    std::mutex logs_mutex;
    std::vector<std::string> repo_names;
    refresh_repo_list(repo_dir_buf, repo_names);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        nk_glfw3_new_frame(&gui_glfw);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.11f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (nk_begin(ctx, "NVCS Client", nk_rect(20, 20, 980, 720), NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
            nk_layout_row_dynamic(ctx, 22, 1);
            nk_label(ctx, "Local repository root:", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, repo_dir_buf, sizeof(repo_dir_buf), nk_filter_default);

            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Refresh local repos")) {
                refresh_repo_list(repo_dir_buf, repo_names);
            }
            if (nk_button_label(ctx, "Clear logs")) {
                std::lock_guard lock(logs_mutex);
                logs.clear();
            }

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "Available local repositories:", NK_TEXT_LEFT);
            for (auto& repo_name : repo_names) {
                nk_layout_row_dynamic(ctx, 22, 1);
                if (nk_button_label(ctx, repo_name.c_str())) {
                    std::strncpy(selected_repo_buf, repo_name.c_str(), sizeof(selected_repo_buf) - 1);
                    selected_repo_buf[sizeof(selected_repo_buf) - 1] = '\0';
                }
            }

            if (selected_repo_buf[0] != '\0') {
                nk_layout_row_dynamic(ctx, 20, 1);
                std::string selected_label = std::string("Selected repository: ") + selected_repo_buf;
                nk_label(ctx, selected_label.c_str(), NK_TEXT_LEFT);

                std::string summary = build_repo_summary(repo_dir_buf, selected_repo_buf);
                nk_layout_row_dynamic(ctx, 120, 1);
                nk_label_wrap(ctx, summary.c_str());
            }

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "Remote operations:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, remote_url_buf, sizeof(remote_url_buf), nk_filter_default);
            nk_layout_row_dynamic(ctx, 24, 1);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, clone_dest_buf, sizeof(clone_dest_buf), nk_filter_default);

            nk_layout_row_dynamic(ctx, 30, 2);
            if (nk_button_label(ctx, "Clone remote repository")) {
                try {
                    auto url = net::RemoteURL::parse(remote_url_buf);
                    fs::path dest = clone_dest_buf[0] ? fs::path(clone_dest_buf) : fs::path(url.repo_name);
                    if (fs::exists(dest / ".nvcs")) {
                        append_log(logs, "Destination already exists and is already a repository.", logs_mutex);
                    } else {
                        fs::create_directories(dest);
                        append_log(logs, "Cloning " + url.base_url() + "...", logs_mutex);
                        net::HttpTransport transport(url);
                        auto adv = transport.fetch_refs();
                        if (adv.refs.empty()) {
                            append_log(logs, "Remote repository is empty.", logs_mutex);
                        } else {
                            std::string default_branch = adv.default_branch.empty() ? "main" : adv.default_branch;
                            std::string head_hash;
                            for (auto& [ref, hash] : adv.refs) {
                                if (ref == "refs/heads/" + default_branch) {
                                    head_hash = hash;
                                    break;
                                }
                            }
                            if (head_hash.empty()) {
                                for (auto& [ref, hash] : adv.refs) {
                                    if (ref.rfind("refs/heads/", 0) == 0) {
                                        default_branch = ref.substr(11);
                                        head_hash = hash;
                                        break;
                                    }
                                }
                            }
                            if (head_hash.empty()) {
                                append_log(logs, "Cannot determine remote HEAD.", logs_mutex);
                            } else {
                                net::UploadRequest req;
                                for (auto& [ref, hash] : adv.refs)
                                    req.wants.push_back(hash);
                                std::sort(req.wants.begin(), req.wants.end());
                                req.wants.erase(std::unique(req.wants.begin(), req.wants.end()), req.wants.end());
                                auto pack = transport.upload_pack(req);
                                auto repo = core::Repository::init(dest, default_branch);
                                repo.apply_pack(pack);
                                for (auto& [ref, hash] : adv.refs) {
                                    if (ref.rfind("refs/heads/", 0) == 0) {
                                        auto branch = ref.substr(11);
                                        repo.refs().write_branch(branch, hash);
                                        repo.refs().write_remote_ref("origin", branch, hash);
                                    } else if (ref.rfind("refs/tags/", 0) == 0) {
                                        repo.refs().write_tag(ref.substr(10), hash);
                                    }
                                }
                                repo.config().remotes["origin"] = remote_url_buf;
                                repo.config().save(repo.nvcs_dir() / "config");
                                repo.checkout(default_branch);
                                append_log(logs, "Cloned repository to " + dest.string(), logs_mutex);
                                refresh_repo_list(repo_dir_buf, repo_names);
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    append_log(logs, std::string("Clone failed: ") + e.what(), logs_mutex);
                }
            }

            if (nk_button_label(ctx, "Fetch remote refs")) {
                try {
                    auto url = net::RemoteURL::parse(remote_url_buf);
                    net::HttpTransport transport(url);
                    auto adv = transport.fetch_refs();
                    std::string result = std::string("Remote refs for ") + remote_url_buf + ":\n";
                    for (auto& [ref, hash] : adv.refs)
                        result += ref + " -> " + hash + "\n";
                    result += "Default branch: " + adv.default_branch;
                    remote_refs = result;
                    append_log(logs, "Fetched remote refs.", logs_mutex);
                } catch (const std::exception& e) {
                    remote_refs = "Failed to fetch remote refs: " + std::string(e.what());
                    append_log(logs, remote_refs, logs_mutex);
                }
            }

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "Remote refs:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 120, 1);
            nk_label_wrap(ctx, remote_refs.c_str());

            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "Activity log:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 120, 1);
            {
                std::lock_guard lock(logs_mutex);
                nk_label_wrap(ctx, logs.c_str());
            }
        }
        nk_end(ctx);

        nk_glfw3_render(&gui_glfw, NK_ANTI_ALIASING_ON, 512 * 1024, 128 * 1024);
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    nk_glfw3_shutdown(&gui_glfw);
    glfwTerminate();
    return 0;
}

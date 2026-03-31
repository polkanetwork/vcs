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

static const char* status_state_name(core::StatusEntry::State state) {
    switch (state) {
        case core::StatusEntry::State::Untracked: return "Untracked";
        case core::StatusEntry::State::Modified: return "Modified";
        case core::StatusEntry::State::Deleted: return "Deleted";
        case core::StatusEntry::State::Added: return "Added";
        case core::StatusEntry::State::Renamed: return "Renamed";
        case core::StatusEntry::State::Unmodified: return "Unmodified";
        default: return "Unknown";
    }
}

static bool has_index_changes(const core::StatusEntry& entry) {
    return entry.index_state != core::StatusEntry::State::Unmodified;
}

static bool has_worktree_changes(const core::StatusEntry& entry) {
    return entry.work_state != core::StatusEntry::State::Unmodified;
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
    char commit_message_buf[512] = "";
    char new_branch_buf[128] = "";
    char branch_selected_buf[128] = "";

    std::string logs;
    std::string remote_refs;
    std::string push_status = "Idle";
    std::string pull_status = "Idle";
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

                auto repo_path = fs::path(repo_dir_buf) / selected_repo_buf;
                if (!fs::exists(repo_path / ".nvcs")) {
                    nk_layout_row_dynamic(ctx, 20, 1);
                    nk_label(ctx, "Selected path is not a repository.", NK_TEXT_LEFT);
                } else {
                    try {
                        core::Repository repo(repo_path);
                        auto status_entries = repo.status();
                        auto branch_names = repo.list_branches();
                        auto current_branch = repo.current_branch();
                        auto current_commit = repo.current_commit();

                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, "Repository summary:", NK_TEXT_LEFT);
                        nk_layout_row_dynamic(ctx, 60, 1);
                        {
                            std::string summary = "Branch: " + (current_branch.empty() ? "(detached)" : current_branch);
                            summary += "\nHEAD: " + (current_commit.empty() ? "(none)" : current_commit);
                            summary += "\nBranches: " + std::to_string(branch_names.size());
                            summary += "\nRemotes: " + std::to_string(repo.config().remotes.size());
                            nk_label_wrap(ctx, summary.c_str());
                        }

                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, "Worktree / staging status:", NK_TEXT_LEFT);
                        if (status_entries.empty()) {
                            nk_layout_row_dynamic(ctx, 20, 1);
                            nk_label(ctx, "Repository is clean.", NK_TEXT_LEFT);
                        } else {
                            for (auto& entry : status_entries) {
                                nk_layout_row_dynamic(ctx, 22, 4);
                                nk_label(ctx, entry.path.c_str(), NK_TEXT_LEFT);
                                nk_label(ctx, status_state_name(entry.index_state), NK_TEXT_CENTERED);
                                nk_label(ctx, status_state_name(entry.work_state), NK_TEXT_CENTERED);
                                if (has_worktree_changes(entry)) {
                                    if (nk_button_label(ctx, "Stage")) {
                                        repo.stage_file(entry.path);
                                        append_log(logs, "Staged " + entry.path, logs_mutex);
                                    }
                                } else {
                                    nk_label(ctx, "", NK_TEXT_LEFT);
                                }
                            }

                            nk_layout_row_dynamic(ctx, 22, 4);
                            nk_label(ctx, "File", NK_TEXT_LEFT);
                            nk_label(ctx, "Index", NK_TEXT_CENTERED);
                            nk_label(ctx, "Worktree", NK_TEXT_CENTERED);
                            nk_label(ctx, "Action", NK_TEXT_LEFT);
                        }

                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, "Commit changes:", NK_TEXT_LEFT);
                        nk_layout_row_dynamic(ctx, 24, 1);
                        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, commit_message_buf, sizeof(commit_message_buf), nk_filter_default);
                        nk_layout_row_dynamic(ctx, 30, 2);
                        if (nk_button_label(ctx, "Commit staged changes")) {
                            if (commit_message_buf[0] == '\0') {
                                append_log(logs, "Commit message cannot be empty.", logs_mutex);
                            } else {
                                try {
                                    auto hash = repo.create_commit(commit_message_buf);
                                    append_log(logs, "Created commit " + hash + " on " + (current_branch.empty() ? "detached" : current_branch), logs_mutex);
                                    commit_message_buf[0] = '\0';
                                } catch (const std::exception& e) {
                                    append_log(logs, std::string("Commit failed: ") + e.what(), logs_mutex);
                                }
                            }
                        }

                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, "Branches:", NK_TEXT_LEFT);
                        nk_layout_row_dynamic(ctx, 24, 2);
                        nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, new_branch_buf, sizeof(new_branch_buf), nk_filter_default);
                        if (nk_button_label(ctx, "Create branch")) {
                            if (new_branch_buf[0] == '\0') {
                                append_log(logs, "Branch name cannot be empty.", logs_mutex);
                            } else {
                                try {
                                    repo.create_branch(new_branch_buf, current_branch);
                                    append_log(logs, std::string("Created branch: ") + new_branch_buf, logs_mutex);
                                    std::strncpy(new_branch_buf, "", sizeof(new_branch_buf));
                                } catch (const std::exception& e) {
                                    append_log(logs, std::string("Create branch failed: ") + e.what(), logs_mutex);
                                }
                            }
                        }

                        nk_layout_row_dynamic(ctx, 22, 3);
                        nk_label(ctx, "Branch", NK_TEXT_LEFT);
                        nk_label(ctx, "Current", NK_TEXT_CENTERED);
                        nk_label(ctx, "Actions", NK_TEXT_LEFT);
                        for (auto& branch : branch_names) {
                            nk_layout_row_dynamic(ctx, 22, 3);
                            if (nk_button_label(ctx, branch.c_str())) {
                                std::strncpy(branch_selected_buf, branch.c_str(), sizeof(branch_selected_buf) - 1);
                                branch_selected_buf[sizeof(branch_selected_buf) - 1] = '\0';
                            }
                            nk_label(ctx, branch == current_branch ? "yes" : "", NK_TEXT_CENTERED);
                            if (branch == current_branch) {
                                if (nk_button_label(ctx, "Checkout")) {
                                    append_log(logs, "Already on branch " + branch, logs_mutex);
                                }
                                nk_label(ctx, "", NK_TEXT_LEFT);
                            } else {
                                if (nk_button_label(ctx, "Checkout")) {
                                    try {
                                        repo.checkout(branch);
                                        append_log(logs, "Checked out branch " + branch, logs_mutex);
                                        std::strncpy(branch_selected_buf, branch.c_str(), sizeof(branch_selected_buf) - 1);
                                        branch_selected_buf[sizeof(branch_selected_buf) - 1] = '\0';
                                    } catch (const std::exception& e) {
                                        append_log(logs, std::string("Checkout failed: ") + e.what(), logs_mutex);
                                    }
                                }
                                if (nk_button_label(ctx, "Delete")) {
                                    try {
                                        repo.delete_branch(branch);
                                        append_log(logs, "Deleted branch " + branch, logs_mutex);
                                        if (std::string(branch_selected_buf) == branch)
                                            branch_selected_buf[0] = '\0';
                                    } catch (const std::exception& e) {
                                        append_log(logs, std::string("Delete branch failed: ") + e.what(), logs_mutex);
                                    }
                                }
                            }
                        }

                        if (branch_selected_buf[0] != '\0') {
                            nk_layout_row_dynamic(ctx, 22, 1);
                            std::string selected_branch_label = std::string("Selected branch: ") + branch_selected_buf;
                            nk_label(ctx, selected_branch_label.c_str(), NK_TEXT_LEFT);
                        }

                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, "Push / pull status:", NK_TEXT_LEFT);
                        nk_layout_row_dynamic(ctx, 30, 3);
                        if (nk_button_label(ctx, "Push current branch")) {
                            try {
                                std::string branch = current_branch.empty() ? std::string(branch_selected_buf) : current_branch;
                                if (branch.empty()) {
                                    push_status = "No branch selected or checked out.";
                                } else if (repo.config().remotes.empty()) {
                                    push_status = "No remote configured.";
                                } else {
                                    auto remote_url = repo.config().remotes.begin()->second;
                                    auto url = net::RemoteURL::parse(remote_url);
                                    net::HttpTransport transport(url);
                                    auto local_hash = repo.refs().resolve(branch);
                                    if (local_hash.empty()) throw std::runtime_error("Branch has no commits");
                                    std::string remote_hash;
                                    try {
                                        auto adv = transport.fetch_refs();
                                        auto ref_key = "refs/heads/" + branch;
                                        auto ri = adv.refs.find(ref_key);
                                        if (ri != adv.refs.end()) remote_hash = ri->second;
                                    } catch (...) {}
                                    if (remote_hash == local_hash) {
                                        push_status = "Everything up-to-date.";
                                    } else {
                                        auto pack = repo.create_pack({local_hash});
                                        nvcs::net::ReceiveRequest req;
                                        nvcs::net::ReceiveRequest::Update upd;
                                        upd.ref_name = "refs/heads/" + branch;
                                        upd.old_hash = remote_hash;
                                        upd.new_hash = local_hash;
                                        req.updates.push_back(upd);
                                        req.pack_data = pack;
                                        transport.receive_pack(req);
                                        repo.refs().write_remote_ref(repo.config().remotes.begin()->first, branch, local_hash);
                                        push_status = "Pushed " + branch + " to " + repo.config().remotes.begin()->first + ".";
                                    }
                                }
                            } catch (const std::exception& e) {
                                push_status = std::string("Push failed: ") + e.what();
                            }
                        }
                        if (nk_button_label(ctx, "Pull current branch")) {
                            try {
                                std::string branch = current_branch.empty() ? std::string(branch_selected_buf) : current_branch;
                                if (branch.empty()) {
                                    pull_status = "No branch selected or checked out.";
                                } else if (repo.config().remotes.empty()) {
                                    pull_status = "No remote configured.";
                                } else {
                                    auto remote_url = repo.config().remotes.begin()->second;
                                    auto url = net::RemoteURL::parse(remote_url);
                                    net::HttpTransport transport(url);
                                    auto adv = transport.fetch_refs();
                                    auto ref_key = "refs/heads/" + branch;
                                    auto ri = adv.refs.find(ref_key);
                                    if (ri == adv.refs.end()) {
                                        pull_status = "Remote branch not found.";
                                    } else {
                                        auto remote_hash = ri->second;
                                        auto local_hash = repo.refs().resolve(branch);
                                        if (remote_hash == local_hash) {
                                            pull_status = "Already up to date.";
                                        } else {
                                            nvcs::net::UploadRequest req;
                                            req.wants = {remote_hash};
                                            if (!local_hash.empty()) req.haves = {local_hash};
                                            auto pack = transport.upload_pack(req);
                                            repo.apply_pack(pack);
                                            repo.refs().write_branch(branch, remote_hash);
                                            repo.refs().write_remote_ref(repo.config().remotes.begin()->first, branch, remote_hash);
                                            if (repo.current_branch() == branch) {
                                                repo.checkout(branch);
                                                pull_status = "Fast-forwarded " + branch + ".";
                                            } else {
                                                pull_status = "Updated remote branch " + branch + ".";
                                            }
                                        }
                                    }
                                }
                            } catch (const std::exception& e) {
                                pull_status = std::string("Pull failed: ") + e.what();
                            }
                        }

                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, push_status.c_str(), NK_TEXT_LEFT);
                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, pull_status.c_str(), NK_TEXT_LEFT);
                    } catch (const std::exception& e) {
                        nk_layout_row_dynamic(ctx, 20, 1);
                        nk_label(ctx, std::string("Repository error: " + std::string(e.what())).c_str(), NK_TEXT_LEFT);
                    }
                }
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

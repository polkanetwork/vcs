#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdlib>
#include "nvcs/core/repository.hpp"
#include "nvcs/net/transport.hpp"

// Forward declarations of command handlers
int cmd_init(const std::vector<std::string>& args);
int cmd_add(const std::vector<std::string>& args);
int cmd_commit(const std::vector<std::string>& args);
int cmd_status(const std::vector<std::string>& args);
int cmd_log(const std::vector<std::string>& args);
int cmd_branch(const std::vector<std::string>& args);
int cmd_checkout(const std::vector<std::string>& args);
int cmd_diff(const std::vector<std::string>& args);
int cmd_tag(const std::vector<std::string>& args);
int cmd_remote(const std::vector<std::string>& args);
int cmd_push(const std::vector<std::string>& args);
int cmd_pull(const std::vector<std::string>& args);
int cmd_clone(const std::vector<std::string>& args);
int cmd_config(const std::vector<std::string>& args);
int cmd_cat_object(const std::vector<std::string>& args);
int cmd_version(const std::vector<std::string>& args);

static void print_usage() {
    std::cerr <<
        "Usage: nvcs <command> [options]\n\n"
        "Repository commands:\n"
        "  init [dir]                    Initialize a new repository\n"
        "  clone <url> [dir]             Clone a repository\n"
        "\nWorkflow commands:\n"
        "  add <file>...                 Stage files\n"
        "  commit -m <message>           Create a commit\n"
        "  status                        Show working tree status\n"
        "  log [--oneline] [-n <count>]  Show commit history\n"
        "  diff [--staged]               Show changes\n"
        "\nBranching:\n"
        "  branch [name] [-d name]       List or create/delete branches\n"
        "  checkout <branch|hash>        Switch branch or detach HEAD\n"
        "  tag [name] [hash]             List or create tags\n"
        "\nRemotes:\n"
        "  remote add <name> <url>       Add a remote\n"
        "  remote remove <name>          Remove a remote\n"
        "  remote list                   List remotes\n"
        "  push <remote> <branch>        Push to remote\n"
        "  pull <remote> <branch>        Pull from remote\n"
        "\nOther:\n"
        "  config [--global] key value   Get/set config\n"
        "  cat-object <hash>             Print object contents\n"
        "  version                       Show nvcs and repo version\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);

    static const std::unordered_map<std::string, std::function<int(const std::vector<std::string>&)>> commands = {
        {"init",       cmd_init},
        {"add",        cmd_add},
        {"commit",     cmd_commit},
        {"status",     cmd_status},
        {"log",        cmd_log},
        {"branch",     cmd_branch},
        {"checkout",   cmd_checkout},
        {"diff",       cmd_diff},
        {"tag",        cmd_tag},
        {"remote",     cmd_remote},
        {"push",       cmd_push},
        {"pull",       cmd_pull},
        {"clone",      cmd_clone},
        {"config",     cmd_config},
        {"cat-object", cmd_cat_object},
        {"version",    cmd_version},
    };

    auto it = commands.find(cmd);
    if (it == commands.end()) {
        std::cerr << "nvcs: unknown command '" << cmd << "'\n";
        print_usage();
        return 1;
    }

    try {
        return it->second(args);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

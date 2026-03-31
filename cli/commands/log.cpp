#include "nvcs/core/repository.hpp"
#include <iostream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>

static std::string format_time(int64_t ts) {
    std::time_t t = static_cast<std::time_t>(ts);
    std::tm* tm_info = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(tm_info, "%a %b %d %H:%M:%S %Y");
    return ss.str();
}

int cmd_log(const std::vector<std::string>& args) {
    namespace fs = std::filesystem;
    bool oneline = false;
    int max_count = -1;
    std::string start;

    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--oneline") oneline = true;
        else if (args[i] == "-n" && i + 1 < args.size()) max_count = std::stoi(args[++i]);
        else if (args[i][0] != '-') start = args[i];
    }

    auto repo = nvcs::core::Repository::open(fs::current_path());
    auto commits = repo.log(start, max_count);

    if (commits.empty()) {
        std::cout << "No commits yet.\n";
        return 0;
    }

    // We need hashes alongside commits; recompute
    std::string cur_hash = start.empty() ? repo.current_commit() : repo.refs().resolve(start);
    for (auto& c : commits) {
        auto hash = c.hash();
        if (oneline) {
            auto msg = c.message();
            auto nl = msg.find('\n');
            if (nl != std::string::npos) msg = msg.substr(0, nl);
            std::cout << hash.substr(0, 12) << " " << msg << "\n";
        } else {
            std::cout << "commit " << hash << "\n";
            std::cout << "Author: " << c.author().name << " <" << c.author().email << ">\n";
            std::cout << "Date:   " << format_time(c.author().timestamp) << "\n\n";
            std::cout << "    " << c.message() << "\n\n";
        }
    }
    return 0;
}

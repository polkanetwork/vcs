#include "nvcs/core/commit.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <stdexcept>

namespace nvcs::core {

std::string Signature::format() const {
    std::ostringstream ss;
    int sign = tz_offset >= 0 ? 1 : -1;
    int abs_offset = std::abs(tz_offset);
    ss << name << " <" << email << "> "
       << timestamp << " "
       << (sign >= 0 ? "+" : "-")
       << std::setw(2) << std::setfill('0') << (abs_offset / 60)
       << std::setw(2) << std::setfill('0') << (abs_offset % 60);
    return ss.str();
}

Signature Signature::parse(const std::string& s) {
    // "Name <email> timestamp +HHMM"
    Signature sig;
    auto lt = s.rfind('<');
    auto gt = s.rfind('>');
    if (lt == std::string::npos || gt == std::string::npos)
        throw std::invalid_argument("invalid signature: " + s);
    sig.name = s.substr(0, lt - 1);
    sig.email = s.substr(lt + 1, gt - lt - 1);
    std::istringstream rest(s.substr(gt + 2));
    rest >> sig.timestamp;
    std::string tz;
    rest >> tz;
    int sign = (tz[0] == '+') ? 1 : -1;
    int hh = std::stoi(tz.substr(1, 2));
    int mm = std::stoi(tz.substr(3, 2));
    sig.tz_offset = sign * (hh * 60 + mm);
    return sig;
}

Signature Signature::now(const std::string& name, const std::string& email) {
    Signature sig;
    sig.name = name;
    sig.email = email;
    sig.timestamp = static_cast<int64_t>(std::time(nullptr));
    sig.tz_offset = 0;
    return sig;
}

Commit::Commit(std::string tree, std::vector<std::string> parents,
               Signature author, Signature committer, std::string message)
    : tree_(std::move(tree)), parents_(std::move(parents)),
      author_(std::move(author)), committer_(std::move(committer)),
      message_(std::move(message)) {}

std::vector<uint8_t> Commit::serialize() const {
    std::ostringstream ss;
    ss << "tree " << tree_ << "\n";
    for (auto& p : parents_)
        ss << "parent " << p << "\n";
    ss << "author " << author_.format() << "\n";
    ss << "committer " << committer_.format() << "\n";
    ss << "\n" << message_;
    auto s = ss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

Commit Commit::from_envelope(const std::vector<uint8_t>& raw) {
    // Skip header (find first null)
    auto it = raw.begin();
    while (it != raw.end() && *it != 0) ++it;
    if (it == raw.end())
        throw ObjectError("invalid commit envelope");
    ++it;
    std::string body(it, raw.end());

    Commit c;
    std::istringstream ss(body);
    std::string line;
    bool in_message = false;
    std::string msg;

    while (std::getline(ss, line)) {
        if (in_message) {
            if (!msg.empty()) msg += "\n";
            msg += line;
            continue;
        }
        if (line.empty()) { in_message = true; continue; }

        if (line.substr(0, 5) == "tree ") {
            c.tree_ = line.substr(5);
        } else if (line.substr(0, 7) == "parent ") {
            c.parents_.push_back(line.substr(7));
        } else if (line.substr(0, 7) == "author ") {
            c.author_ = Signature::parse(line.substr(7));
        } else if (line.substr(0, 10) == "committer ") {
            c.committer_ = Signature::parse(line.substr(10));
        }
    }
    c.message_ = msg;
    return c;
}

} // namespace nvcs::core

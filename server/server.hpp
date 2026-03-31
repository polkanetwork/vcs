#pragma once

#include "httplib.h"
#include "json.hpp"
#include <nvcs/core/repository.hpp>
#include <nvcs/net/protocol.hpp>
#include <filesystem>
#include <mutex>
#include <string>

namespace nvcs::server {
    void setup_routes(httplib::Server& svr, const std::string& repo_dir, std::mutex& repo_mutex);
}

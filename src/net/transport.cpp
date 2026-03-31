#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "nvcs/net/transport.hpp"
#include "../../third_party/httplib.h"
#include <sstream>
#include <stdexcept>
#include <regex>

namespace nvcs::net {

RemoteURL RemoteURL::parse(const std::string& url) {
    // Match http[s]://host[:port]/repos/name
    std::regex re(R"((https?)://([^:/]+)(?::(\d+))?/repos/([^/]+))");
    std::smatch m;
    if (!std::regex_match(url, m, re))
        throw TransportError("invalid remote URL: " + url);
    RemoteURL r;
    r.scheme = m[1];
    r.host = m[2];
    r.port = m[3].matched ? std::stoi(m[3]) : (r.scheme == "https" ? 443 : 80);
    r.repo_name = m[4];
    return r;
}

std::string RemoteURL::base_url() const {
    return scheme + "://" + host + ":" + std::to_string(port);
}

HttpTransport::HttpTransport(const RemoteURL& remote) : remote_(remote) {}

std::string HttpTransport::get(const std::string& path) const {
    httplib::Client cli(remote_.host, remote_.port);
    cli.set_connection_timeout(10, 0);
    cli.set_read_timeout(60, 0);
    auto res = cli.Get(path.c_str());
    if (!res) throw TransportError("HTTP GET failed: " + path);
    if (res->status != 200)
        throw TransportError("HTTP GET " + path + " returned " + std::to_string(res->status));
    return res->body;
}

std::string HttpTransport::post_json(const std::string& path, const std::string& body) const {
    httplib::Client cli(remote_.host, remote_.port);
    cli.set_connection_timeout(10, 0);
    cli.set_read_timeout(120, 0);
    auto res = cli.Post(path.c_str(), body, "application/json");
    if (!res) throw TransportError("HTTP POST failed: " + path);
    if (res->status != 200)
        throw TransportError("HTTP POST " + path + " returned " + std::to_string(res->status)
                             + ": " + res->body);
    return res->body;
}

RefAdvertisement HttpTransport::fetch_refs() const {
    auto path = "/repos/" + remote_.repo_name + "/info/refs";
    auto body = get(path);
    return parse_ref_adv(body);
}

std::vector<uint8_t> HttpTransport::upload_pack(const UploadRequest& req) const {
    auto path = "/repos/" + remote_.repo_name + "/upload-pack";
    auto body = post_json(path, serialize_upload_req(req));
    auto resp = parse_pack_response(body);
    if (!resp.success) throw TransportError("upload-pack failed: " + resp.error_message);
    return resp.pack_data;
}

bool HttpTransport::receive_pack(const ReceiveRequest& req) const {
    auto path = "/repos/" + remote_.repo_name + "/receive-pack";
    auto body = post_json(path, serialize_receive_req(req));
    auto resp = parse_pack_response(body);
    if (!resp.success) throw TransportError("receive-pack failed: " + resp.error_message);
    return true;
}

bool HttpTransport::create_repo(const std::string& name) const {
    std::string body = "{\"name\":\"" + name + "\"}";
    try {
        post_json("/repos", body);
        return true;
    } catch (...) { return false; }
}

} // namespace nvcs::net

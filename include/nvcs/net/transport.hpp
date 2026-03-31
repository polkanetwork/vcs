#pragma once
#include "protocol.hpp"
#include <string>
#include <vector>
#include <stdexcept>

namespace nvcs::net {

// Parses "http://host:port/repos/name" style URLs
struct RemoteURL {
    std::string scheme;   // "http" or "https"
    std::string host;
    int port;
    std::string repo_name;

    static RemoteURL parse(const std::string& url);
    std::string base_url() const;
};

class TransportError : public std::runtime_error {
public:
    explicit TransportError(const std::string& msg) : std::runtime_error(msg) {}
};

class HttpTransport {
public:
    explicit HttpTransport(const RemoteURL& remote);

    // Fetch ref advertisements from server
    RefAdvertisement fetch_refs() const;

    // Download objects the client needs (returns pack bytes)
    std::vector<uint8_t> upload_pack(const UploadRequest& req) const;

    // Upload objects to server
    bool receive_pack(const ReceiveRequest& req) const;

    // Create a new repository on the server
    bool create_repo(const std::string& name) const;

private:
    RemoteURL remote_;
    std::string post_json(const std::string& path, const std::string& body) const;
    std::string get(const std::string& path) const;
};

} // namespace nvcs::net

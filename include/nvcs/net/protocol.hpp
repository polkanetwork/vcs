#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace nvcs::net {

// Pack-protocol messages exchanged between client and server

struct RefAdvertisement {
    std::unordered_map<std::string, std::string> refs;  // ref_name -> hash
    std::string default_branch;
    std::string server_version;
};

struct UploadRequest {
    std::vector<std::string> wants;   // hashes client wants
    std::vector<std::string> haves;   // hashes client already has
};

struct ReceiveRequest {
    struct Update {
        std::string ref_name;
        std::string old_hash;
        std::string new_hash;
    };
    std::vector<Update> updates;
    std::vector<uint8_t> pack_data;
};

struct PackResponse {
    bool success;
    std::string error_message;
    std::vector<uint8_t> pack_data;
};

// Serialize / deserialize to JSON for HTTP body
std::string serialize_ref_adv(const RefAdvertisement& adv);
RefAdvertisement parse_ref_adv(const std::string& json);

std::string serialize_upload_req(const UploadRequest& req);
UploadRequest parse_upload_req(const std::string& json);

std::string serialize_receive_req(const ReceiveRequest& req);
ReceiveRequest parse_receive_req(const std::string& json);

std::string serialize_pack_response(const PackResponse& resp);
PackResponse parse_pack_response(const std::string& json);

// Base64 helpers for embedding binary pack data in JSON
std::string base64_encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> base64_decode(const std::string& encoded);

} // namespace nvcs::net

#include "nvcs/net/protocol.hpp"
#include "../../third_party/json.hpp"
#include <stdexcept>
#include <array>

using json = nlohmann::json;

namespace nvcs::net {

// ─── Base64 ──────────────────────────────────────────────────────────────────

static const std::string B64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(B64_CHARS[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(B64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::vector<uint8_t> base64_decode(const std::string& encoded) {
    std::array<int, 256> T;
    T.fill(-1);
    for (int i = 0; i < 64; i++) T[(uint8_t)B64_CHARS[i]] = i;

    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (uint8_t c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return out;
}

// ─── Serializers ─────────────────────────────────────────────────────────────

std::string serialize_ref_adv(const RefAdvertisement& adv) {
    json j;
    j["refs"] = adv.refs;
    j["default_branch"] = adv.default_branch;
    j["server_version"] = adv.server_version;
    return j.dump();
}

RefAdvertisement parse_ref_adv(const std::string& s) {
    auto j = json::parse(s);
    RefAdvertisement adv;
    adv.refs = j["refs"].get<std::unordered_map<std::string,std::string>>();
    adv.default_branch = j.value("default_branch", "main");
    adv.server_version = j.value("server_version", "");
    return adv;
}

std::string serialize_upload_req(const UploadRequest& req) {
    json j;
    j["wants"] = req.wants;
    j["haves"] = req.haves;
    return j.dump();
}

UploadRequest parse_upload_req(const std::string& s) {
    auto j = json::parse(s);
    UploadRequest r;
    r.wants = j["wants"].get<std::vector<std::string>>();
    r.haves = j.value("haves", std::vector<std::string>{});
    return r;
}

std::string serialize_receive_req(const ReceiveRequest& req) {
    json j;
    json updates = json::array();
    for (auto& u : req.updates)
        updates.push_back({{"ref", u.ref_name}, {"old", u.old_hash}, {"new", u.new_hash}});
    j["updates"] = updates;
    j["pack"] = base64_encode(req.pack_data);
    return j.dump();
}

ReceiveRequest parse_receive_req(const std::string& s) {
    auto j = json::parse(s);
    ReceiveRequest r;
    for (auto& u : j["updates"]) {
        ReceiveRequest::Update upd;
        upd.ref_name = u["ref"].get<std::string>();
        upd.old_hash = u.value("old", std::string{});
        upd.new_hash = u["new"].get<std::string>();
        r.updates.push_back(std::move(upd));
    }
    r.pack_data = base64_decode(j["pack"].get<std::string>());
    return r;
}

std::string serialize_pack_response(const PackResponse& resp) {
    json j;
    j["success"] = resp.success;
    j["error"] = resp.error_message;
    j["pack"] = base64_encode(resp.pack_data);
    return j.dump();
}

PackResponse parse_pack_response(const std::string& s) {
    auto j = json::parse(s);
    PackResponse r;
    r.success = j["success"].get<bool>();
    r.error_message = j.value("error", std::string{});
    r.pack_data = base64_decode(j.value("pack", std::string{}));
    return r;
}

} // namespace nvcs::net

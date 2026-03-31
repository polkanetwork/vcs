#include "nvcs/util/compress.hpp"
#include <zlib.h>
#include <stdexcept>

namespace nvcs::util {

std::vector<uint8_t> compress(const std::vector<uint8_t>& data, int level) {
    uLongf dest_len = compressBound(data.size());
    std::vector<uint8_t> dest(dest_len);
    int res = ::compress2(dest.data(), &dest_len,
                          data.data(), data.size(), level);
    if (res != Z_OK)
        throw CompressError("zlib compress failed: " + std::to_string(res));
    dest.resize(dest_len);
    return dest;
}

std::vector<uint8_t> compress(const std::string& data, int level) {
    return compress(std::vector<uint8_t>(data.begin(), data.end()), level);
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) {
    // We don't know the original size; try doubling the buffer until it fits
    uLongf dest_len = data.size() * 4;
    std::vector<uint8_t> dest;
    int res;
    do {
        dest_len *= 2;
        dest.resize(dest_len);
        res = ::uncompress(dest.data(), &dest_len, data.data(), data.size());
    } while (res == Z_BUF_ERROR && dest_len < 256 * 1024 * 1024);

    if (res != Z_OK)
        throw CompressError("zlib decompress failed: " + std::to_string(res));
    dest.resize(dest_len);
    return dest;
}

std::string decompress_to_string(const std::vector<uint8_t>& data) {
    auto bytes = decompress(data);
    return std::string(bytes.begin(), bytes.end());
}

} // namespace nvcs::util

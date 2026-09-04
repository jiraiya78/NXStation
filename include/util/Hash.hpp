#pragma once

#include <cstdint>
#include <string>

namespace sf {

class Hash {
public:
    /** CRC32 of entire file. Returns empty string on failure. */
    static std::string crc32File(const std::string& path);

    /** MD5 hex digest of entire file. Returns empty string on failure. */
    static std::string md5File(const std::string& path);

    static uint32_t crc32Buffer(const void* data, size_t len, uint32_t seed = 0);
};

} // namespace sf

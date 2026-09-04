#include "util/Hash.hpp"
#include "util/Logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace sf {

static uint32_t crcTable[256];
static bool crcReady = false;

static void initCrc()
{
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crcTable[i] = c;
    }
    crcReady = true;
}

uint32_t Hash::crc32Buffer(const void* data, size_t len, uint32_t seed)
{
    if (!crcReady)
        initCrc();
    uint32_t c = seed ^ 0xFFFFFFFFu;
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i)
        c = crcTable[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

std::string Hash::crc32File(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SF_LOG_W("Hash", "Cannot open for CRC32: %s", path.c_str());
        return {};
    }

    if (!crcReady)
        initCrc();

    uint32_t c = 0xFFFFFFFFu;
    char buf[64 * 1024];
    while (in) {
        in.read(buf, sizeof(buf));
        auto n = static_cast<size_t>(in.gcount());
        for (size_t i = 0; i < n; ++i)
            c = crcTable[(c ^ static_cast<uint8_t>(buf[i])) & 0xFFu] ^ (c >> 8);
    }
    c ^= 0xFFFFFFFFu;

    char hex[16];
    std::snprintf(hex, sizeof(hex), "%08X", c);
    return hex;
}

// Minimal MD5 (RFC 1321) — enough for ScreenScraper ROM ID
namespace {

struct MD5Ctx {
    uint32_t a = 0x67452301, b = 0xefcdab89, c = 0x98badcfe, d = 0x10325476;
    uint64_t bits = 0;
    uint8_t buffer[64]{};
    size_t bufferLen = 0;
};

static uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
static uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
static uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
static uint32_t rotl(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

static void md5Transform(MD5Ctx& ctx, const uint8_t block[64])
{
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
    static const uint32_t S[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};

    uint32_t M[16];
    for (int i = 0; i < 16; ++i)
        M[i] = static_cast<uint32_t>(block[i * 4]) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 8) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 3]) << 24);

    uint32_t a = ctx.a, b = ctx.b, c = ctx.c, d = ctx.d;
    for (uint32_t i = 0; i < 64; ++i) {
        uint32_t f, g;
        if (i < 16) { f = F(b, c, d); g = i; }
        else if (i < 32) { f = G(b, c, d); g = (5 * i + 1) % 16; }
        else if (i < 48) { f = H(b, c, d); g = (3 * i + 5) % 16; }
        else { f = I(b, c, d); g = (7 * i) % 16; }
        uint32_t tmp = d;
        d = c;
        c = b;
        b = b + rotl(a + f + K[i] + M[g], S[i]);
        a = tmp;
    }
    ctx.a += a;
    ctx.b += b;
    ctx.c += c;
    ctx.d += d;
}

static void md5Update(MD5Ctx& ctx, const uint8_t* data, size_t len)
{
    ctx.bits += len * 8;
    while (len > 0) {
        size_t n = std::min(len, 64 - ctx.bufferLen);
        std::memcpy(ctx.buffer + ctx.bufferLen, data, n);
        ctx.bufferLen += n;
        data += n;
        len -= n;
        if (ctx.bufferLen == 64) {
            md5Transform(ctx, ctx.buffer);
            ctx.bufferLen = 0;
        }
    }
}

static std::string md5Final(MD5Ctx& ctx)
{
    uint8_t pad[64]{};
    pad[0] = 0x80;
    size_t padLen = (ctx.bufferLen < 56) ? (56 - ctx.bufferLen) : (120 - ctx.bufferLen);
    md5Update(ctx, pad, padLen);
    uint8_t bits[8];
    for (int i = 0; i < 8; ++i)
        bits[i] = static_cast<uint8_t>((ctx.bits >> (8 * i)) & 0xFF);
    md5Update(ctx, bits, 8);

    uint8_t digest[16];
    uint32_t vals[4] = {ctx.a, ctx.b, ctx.c, ctx.d};
    for (int i = 0; i < 4; ++i) {
        digest[i * 4]     = static_cast<uint8_t>(vals[i] & 0xFF);
        digest[i * 4 + 1] = static_cast<uint8_t>((vals[i] >> 8) & 0xFF);
        digest[i * 4 + 2] = static_cast<uint8_t>((vals[i] >> 16) & 0xFF);
        digest[i * 4 + 3] = static_cast<uint8_t>((vals[i] >> 24) & 0xFF);
    }

    char hex[33];
    for (int i = 0; i < 16; ++i)
        std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return hex;
}

} // namespace

std::string Hash::md5File(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        SF_LOG_W("Hash", "Cannot open for MD5: %s", path.c_str());
        return {};
    }
    MD5Ctx ctx;
    char buf[64 * 1024];
    while (in) {
        in.read(buf, sizeof(buf));
        auto n = static_cast<size_t>(in.gcount());
        if (n)
            md5Update(ctx, reinterpret_cast<uint8_t*>(buf), n);
    }
    return md5Final(ctx);
}

} // namespace sf

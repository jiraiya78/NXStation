#include "media/TextureCache.hpp"
#include "app/AppState.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"

#include <algorithm>
#include <cstring>

#ifdef __SWITCH__
// Borealis/nanovg already embeds stb_image; use Borealis file loading on Switch.
#define SF_HAVE_STB 0
#elif __has_include("stb_image.h")
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"
#define SF_HAVE_STB 1
#else
#define SF_HAVE_STB 0
#endif

#if SF_HAVE_STB
#include <GLES2/gl2.h>
#elif !defined(__SWITCH__)
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
static void glGenTextures(int, unsigned int* id)
{
    static unsigned int n = 1;
    *id = n++;
}
static void glBindTexture(unsigned int, unsigned int) {}
static void glTexParameteri(unsigned int, unsigned int, int) {}
static void glTexImage2D(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*) {}
static void glDeleteTextures(int, const unsigned int*) {}
#endif

namespace sf {

TextureCache::TextureCache(size_t maxEntries)
    : maxEntries_(std::max<size_t>(4, maxEntries))
{
}

TextureCache::~TextureCache()
{
    flush();
}

void TextureCache::setMaxEntries(size_t n)
{
    maxEntries_ = std::max<size_t>(4, n);
    std::lock_guard<std::mutex> lock(cacheMutex_);
    evictIfNeeded();
}

void TextureCache::touch(const std::string& path)
{
    auto it = cache_.find(path);
    if (it == cache_.end())
        return;
    lru_.erase(it->second.lruIt);
    lru_.push_front(path);
    it->second.lruIt = lru_.begin();
}

void TextureCache::destroyTexture(unsigned int id)
{
#if SF_HAVE_STB
    if (id)
        glDeleteTextures(1, &id);
#else
    (void)id;
#endif
}

void TextureCache::evictIfNeeded()
{
    while (cache_.size() > maxEntries_ && !lru_.empty()) {
        const std::string victim = lru_.back();
        lru_.pop_back();
        auto it = cache_.find(victim);
        if (it != cache_.end()) {
            destroyTexture(it->second.handle.id);
            cache_.erase(it);
        }
    }
}

unsigned int TextureCache::uploadRgba(const uint8_t* data, int w, int h)
{
#if SF_HAVE_STB
    unsigned int id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
#else
    (void)data;
    (void)w;
    (void)h;
    return 1;
#endif
}

void TextureCache::request(const std::string& path, TextureReadyCb cb)
{
    if (path.empty()) {
        if (cb)
            cb({});
        return;
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = cache_.find(path);
        if (it != cache_.end()) {
            touch(path);
            if (cb)
                cb(it->second.handle);
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(inflightMutex_);
        auto it = inflight_.find(path);
        if (it != inflight_.end()) {
            it->second.push_back(std::move(cb));
            return;
        }
        inflight_[path].push_back(std::move(cb));
    }

    AppState::instance().pool().enqueue([this, path]() {
        PendingUpload upload;
        upload.path = path;

        auto fail = [&]() {
            std::vector<TextureReadyCb> cbs;
            {
                std::lock_guard<std::mutex> lock(inflightMutex_);
                auto it = inflight_.find(path);
                if (it != inflight_.end()) {
                    cbs = std::move(it->second);
                    inflight_.erase(it);
                }
            }
            for (auto& c : cbs)
                if (c)
                    c({});
        };

        if (!FileSystem::exists(path)) {
            SF_LOG_W("Tex", "Missing image: %s", path.c_str());
            fail();
            return;
        }

#if SF_HAVE_STB
        std::string fileData = FileSystem::readFile(path);
        int w = 0, h = 0, channels = 0;
        unsigned char* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(fileData.data()),
            static_cast<int>(fileData.size()), &w, &h, &channels, 4);

        if (!pixels) {
            SF_LOG_W("Tex", "Decode failed: %s", path.c_str());
            fail();
            return;
        }

        if (!highResAllowed_ && (w > 512 || h > 512)) {
            int nw = std::max(1, w / 2);
            int nh = std::max(1, h / 2);
            upload.rgba.resize(static_cast<size_t>(nw * nh * 4));
            for (int y = 0; y < nh; ++y) {
                for (int x = 0; x < nw; ++x) {
                    int sx = x * 2, sy = y * 2;
                    size_t di = static_cast<size_t>((y * nw + x) * 4);
                    size_t si = static_cast<size_t>((sy * w + sx) * 4);
                    upload.rgba[di] = pixels[si];
                    upload.rgba[di + 1] = pixels[si + 1];
                    upload.rgba[di + 2] = pixels[si + 2];
                    upload.rgba[di + 3] = pixels[si + 3];
                }
            }
            upload.width = nw;
            upload.height = nh;
        } else {
            upload.width = w;
            upload.height = h;
            upload.rgba.assign(pixels, pixels + static_cast<size_t>(w * h * 4));
        }
        stbi_image_free(pixels);
#else
        // No stb_image — mark as ready so UI can load via Borealis setImageFromFile.
        // Insert a sentinel cache entry (id=1 means "file present") without GPU upload.
        upload.width = 0;
        upload.height = 0;
#endif

        {
            std::lock_guard<std::mutex> lock(inflightMutex_);
            auto it = inflight_.find(path);
            if (it != inflight_.end()) {
                upload.callbacks = std::move(it->second);
                inflight_.erase(it);
            }
        }

        std::lock_guard<std::mutex> lock(uploadMutex_);
        pendingUploads_.push_back(std::move(upload));
    });
}

void TextureCache::pumpUploads()
{
    std::vector<PendingUpload> local;
    {
        std::lock_guard<std::mutex> lock(uploadMutex_);
        local.swap(pendingUploads_);
    }

    for (auto& u : local) {
        TextureHandle handle;
        handle.path = u.path;
        handle.width = u.width;
        handle.height = u.height;

        if (!u.rgba.empty()) {
            handle.id = uploadRgba(u.rgba.data(), u.width, u.height);
        } else if (FileSystem::exists(u.path)) {
            // Sentinel: file validated, Borealis should load it
            handle.id = 1;
        }

        {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            CacheEntry entry;
            entry.handle = handle;
            lru_.push_front(u.path);
            entry.lruIt = lru_.begin();
            cache_[u.path] = entry;
            evictIfNeeded();
        }

        for (auto& cb : u.callbacks) {
            if (cb)
                cb(handle);
        }
        u.rgba.clear();
        u.rgba.shrink_to_fit();
    }
}

void TextureCache::retainOnly(const std::vector<std::string>& keepPaths)
{
    std::unordered_map<std::string, bool> keep;
    for (const auto& p : keepPaths)
        keep[p] = true;

    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (keep.count(it->first) == 0) {
            if (it->second.handle.id > 1)
                destroyTexture(it->second.handle.id);
            lru_.erase(it->second.lruIt);
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void TextureCache::flush()
{
    {
        std::lock_guard<std::mutex> lock(uploadMutex_);
        pendingUploads_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(inflightMutex_);
        inflight_.clear();
    }
    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (auto& [_, entry] : cache_) {
        if (entry.handle.id > 1)
            destroyTexture(entry.handle.id);
    }
    cache_.clear();
    lru_.clear();
}

} // namespace sf

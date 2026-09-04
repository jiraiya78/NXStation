#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace sf {

struct TextureHandle {
    unsigned int id = 0; // OpenGL texture id (0 = invalid / placeholder)
    int width = 0;
    int height = 0;
    std::string path;
};

using TextureReadyCb = std::function<void(TextureHandle)>;

/**
 * Async image loader with LRU eviction.
 * Decoding happens on worker threads; GL texture upload MUST occur on main thread.
 */
class TextureCache {
public:
    explicit TextureCache(size_t maxEntries = 48);
    ~TextureCache();

    void setMaxEntries(size_t n);
    void setHighResAllowed(bool allowed) { highResAllowed_ = allowed; }

    /** Request texture. Calls cb on main thread when ready (or immediately if cached). */
    void request(const std::string& path, TextureReadyCb cb);

    /** Evict textures whose paths are not in keepSet (call when views leave screen). */
    void retainOnly(const std::vector<std::string>& keepPaths);

    void flush();
    void pumpUploads(); // call from main-thread update loop

private:
    struct CacheEntry {
        TextureHandle handle;
        std::list<std::string>::iterator lruIt;
    };

    struct PendingUpload {
        std::string path;
        std::vector<uint8_t> rgba;
        int width = 0;
        int height = 0;
        std::vector<TextureReadyCb> callbacks;
    };

    void touch(const std::string& path);
    void evictIfNeeded();
    unsigned int uploadRgba(const uint8_t* data, int w, int h);
    void destroyTexture(unsigned int id);

    size_t maxEntries_;
    bool highResAllowed_ = true;

    std::mutex cacheMutex_;
    std::unordered_map<std::string, CacheEntry> cache_;
    std::list<std::string> lru_;

    std::mutex uploadMutex_;
    std::vector<PendingUpload> pendingUploads_;

    std::mutex inflightMutex_;
    std::unordered_map<std::string, std::vector<TextureReadyCb>> inflight_;
};

} // namespace sf

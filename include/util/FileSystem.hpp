#pragma once

#include <functional>
#include <string>
#include <vector>

namespace sf {

struct RomEntry {
    std::string path;
    std::string filename;
    std::string stem;
    std::string extension;
    uint64_t size = 0;
};

struct DirEntry {
    std::string name;
    std::string path;
    bool isDirectory = false;
    uint64_t size = 0;
};

class FileSystem {
public:
    static bool exists(const std::string& path);
    static bool isDirectory(const std::string& path);
    static bool createDirectories(const std::string& path);
    static bool ensureAppDirectories();
    /** Move legacy root-level JSON/logs into settings/ and log/. */
    static void migrateLegacyStorage();

    static std::string readFile(const std::string& path);
    static bool writeFile(const std::string& path, const std::string& data);
    static bool removeFile(const std::string& path);
    /** Rename/move a file. Returns false if `from` missing or `to` cannot be written. */
    static bool renameFile(const std::string& from, const std::string& to);
    /** Stream-copy `from` onto `to` via stdio (fails on locked running NROs). */
    static bool copyFile(const std::string& from, const std::string& to);
    /**
     * Overwrite `to` with contents of `from`.
     * On Switch uses native fsFs CreateFile/OpenWrite/SetSize (Sphaira/Breeze pattern) so a
     * running NRO can be replaced on disk while mapped in memory.
     */
    static bool overwriteFile(const std::string& from, const std::string& to);

    /** Recursively collect files whose extension is in `extensions` (lowercase, with dot). */
    static std::vector<RomEntry> scanRoms(const std::string& root,
                                          const std::vector<std::string>& extensions);

    /** List one directory level (non-recursive). Sorted: directories first, then files. */
    static std::vector<DirEntry> listDirectory(const std::string& path);

    /** Parent directory path, or empty if none. */
    static std::string parentPath(const std::string& path);

    static std::string toLower(std::string s);
    static std::string extensionOf(const std::string& path);
    static std::string filenameOf(const std::string& path);
    static std::string stemOf(const std::string& path);
    static std::string join(const std::string& a, const std::string& b);
};

} // namespace sf

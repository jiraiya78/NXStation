#include "util/FileSystem.hpp"
#include "util/Logger.hpp"
#include "util/Paths.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>

#ifdef __SWITCH__
#include <dirent.h>
#include <switch.h>
#include <unistd.h>
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace sf {

bool FileSystem::exists(const std::string& path)
{
#ifdef __SWITCH__
    struct stat st {};
    return stat(path.c_str(), &st) == 0;
#else
    return fs::exists(path);
#endif
}

bool FileSystem::isDirectory(const std::string& path)
{
#ifdef __SWITCH__
    struct stat st {};
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
#else
    return fs::is_directory(path);
#endif
}

bool FileSystem::createDirectories(const std::string& path)
{
#ifdef __SWITCH__
    if (path.empty() || exists(path))
        return exists(path) && isDirectory(path);

    std::string current;
    current.reserve(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        char c = path[i];
        current.push_back(c);
        if (c == '/' || i + 1 == path.size()) {
            if (current.size() <= 1)
                continue;
            // skip drive-like prefixes e.g. "sdmc:"
            if (current.back() == ':')
                continue;
            if (!exists(current)) {
                if (mkdir(current.c_str(), 0777) != 0 && !exists(current))
                    return false;
            }
        }
    }
    return true;
#else
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec;
#endif
}

bool FileSystem::ensureAppDirectories()
{
    const char* dirs[] = {
        paths::APP_ROOT,
        paths::DATA_DIR,
        paths::CACHE_DIR,
        paths::META_DIR,
        paths::ARTWORK_DIR,
        paths::VIDEO_DIR,
        paths::BACKGROUNDS_DIR,
        paths::USER_RESOURCES_DIR,
        paths::LOG_DIR,
        paths::THEME_DIR,
        paths::SETTINGS_DIR,
        paths::FAVORITES_SECTION_DIR,
        paths::LAST_PLAYED_SECTION_DIR,
        paths::CLOUD_DIR,
    };
    for (const char* d : dirs) {
        if (!createDirectories(d)) {
            std::fprintf(stderr, "[FS] Failed to create %s\n", d);
            return false;
        }
    }
    migrateLegacyStorage();
    return true;
}

void FileSystem::migrateLegacyStorage()
{
    struct Move {
        const char* from;
        const char* to;
    };

    const Move moves[] = {
        {"sdmc:/switch/NXStation/roms_config.json", paths::CONFIG_PATH},
        {"sdmc:/switch/NXStation/user_settings.json", paths::USER_SETTINGS_PATH},
        {"sdmc:/switch/NXStation/user_cores.json", paths::USER_CORES_PATH},
        {"sdmc:/switch/NXStation/user_screenscraper.json", paths::USER_SCREENSCRAPER_PATH},
        {"sdmc:/switch/NXStation/user_favorites.json", paths::USER_FAVORITES_PATH},
        {"sdmc:/switch/NXStation/navigation_state.json", paths::NAV_STATE_PATH},
        {"sdmc:/switch/NXStation/NXStation.log", paths::LOG_PATH},
        {"sdmc:/switch/NXStation/boot.log", paths::BOOT_LOG_PATH},
        {"sdmc:/switch/NXStation/crash.log", paths::CRASH_LOG_PATH},
    };

    for (const auto& move : moves) {
        if (exists(move.to) || !exists(move.from))
            continue;
        const std::string data = readFile(move.from);
        if (data.empty())
            continue;
        if (writeFile(move.to, data)) {
            removeFile(move.from);
            SF_LOG_I("FS", "Migrated %s -> %s", move.from, move.to);
        }
    }
}

std::string FileSystem::readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool FileSystem::writeFile(const std::string& path, const std::string& data)
{
    // Ensure parent directory exists
    auto slash = path.find_last_of('/');
    if (slash != std::string::npos)
        createDirectories(path.substr(0, slash));

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

bool FileSystem::removeFile(const std::string& path)
{
#ifdef __SWITCH__
    return unlink(path.c_str()) == 0 || !exists(path);
#else
    std::error_code ec;
    fs::remove(path, ec);
    return !ec || !exists(path);
#endif
}

bool FileSystem::renameFile(const std::string& from, const std::string& to)
{
    if (from.empty() || to.empty() || from == to)
        return false;
    if (!exists(from))
        return false;
    if (exists(to))
        return false;

    const std::string parent = parentPath(to);
    if (!parent.empty())
        createDirectories(parent);

#ifdef __SWITCH__
    return ::rename(from.c_str(), to.c_str()) == 0;
#else
    std::error_code ec;
    fs::rename(from, to, ec);
    return !ec;
#endif
}

bool FileSystem::copyFile(const std::string& from, const std::string& to)
{
    if (from.empty() || to.empty() || from == to)
        return false;
    if (!exists(from))
        return false;

    const std::string parent = parentPath(to);
    if (!parent.empty())
        createDirectories(parent);

    std::ifstream in(from, std::ios::binary);
    if (!in)
        return false;

    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;

    constexpr std::size_t kBuf = 256 * 1024;
    std::vector<char> buf(kBuf);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto got = in.gcount();
        if (got > 0)
            out.write(buf.data(), got);
        if (!out)
            return false;
    }
    out.flush();
    return out.good() && in.eof();
}

namespace {

#ifdef __SWITCH__
/** Strip sdmc: so fsFs* APIs get "/switch/...". */
std::string toNativeSdPath(std::string path)
{
    if (path.rfind("sdmc:", 0) == 0)
        path.erase(0, 5);
    if (path.empty() || path[0] != '/')
        path.insert(path.begin(), '/');
    return path;
}

bool overwriteFileNative(const std::string& from, const std::string& to)
{
    std::ifstream in(from, std::ios::binary | std::ios::ate);
    if (!in) {
        SF_LOG_W("FS", "overwriteFile: cannot open source %s", from.c_str());
        return false;
    }
    const auto size = static_cast<s64>(in.tellg());
    if (size < 0) {
        SF_LOG_W("FS", "overwriteFile: bad source size %s", from.c_str());
        return false;
    }
    in.seekg(0, std::ios::beg);

    // Borealis keeps romfsInit() for the whole process, which holds the running NRO
    // open → OpenFile(Write) returns 0xE02 (FsError_TargetLocked). Sphaira only mounts
    // romfs briefly; release it before self-replace.
    SF_LOG_I("FS", "overwriteFile: romfsExit() before replacing NRO");
    romfsExit();

    FsFileSystem owned {};
    FsFileSystem* sd = nullptr;
    bool closeOwned = false;
    if (R_SUCCEEDED(fsOpenSdCardFileSystem(&owned))) {
        sd = &owned;
        closeOwned = true;
    } else {
        sd = fsdevGetDeviceFileSystem("sdmc:");
        if (!sd)
            sd = fsdevGetDeviceFileSystem("sdmc");
    }
    if (!sd) {
        SF_LOG_W("FS", "overwriteFile: no SD filesystem handle");
        romfsInit();
        return false;
    }

    auto fail = [&](const char* why, Result rc) {
        SF_LOG_W("FS", "overwriteFile: %s 0x%X%s", why, rc, rc == 0xE02 ? " (TargetLocked)" : "");
        if (closeOwned)
            fsFsClose(&owned);
        romfsInit();
        return false;
    };

    const std::string parent = FileSystem::parentPath(to);
    if (!parent.empty())
        FileSystem::createDirectories(parent);

    const std::string fsPath = toNativeSdPath(to);
    u32 createOpt = 0;
    if (static_cast<u64>(size) >= 4ULL * 1024 * 1024 * 1024)
        createOpt |= FsCreateOption_BigFile;

    Result rc = fsFsCreateFile(sd, fsPath.c_str(), size, createOpt);
    if (R_FAILED(rc))
        SF_LOG_I("FS", "overwriteFile: CreateFile returned 0x%X (existing OK)", rc);

    FsFile file {};
    rc = fsFsOpenFile(sd, fsPath.c_str(), FsOpenMode_Write, &file);
    if (R_FAILED(rc))
        return fail("OpenFile(Write)", rc);

    rc = fsFileSetSize(&file, size);
    if (R_FAILED(rc)) {
        fsFileClose(&file);
        return fail("SetSize", rc);
    }

    constexpr std::size_t kBuf = 256 * 1024;
    std::vector<char> buf(kBuf);
    s64 offset = 0;
    while (offset < size) {
        const s64 remain = size - offset;
        const auto chunk = static_cast<std::size_t>(remain < static_cast<s64>(kBuf) ? remain : kBuf);
        in.read(buf.data(), static_cast<std::streamsize>(chunk));
        const auto got = in.gcount();
        if (got <= 0) {
            fsFileClose(&file);
            return fail("short read", 0);
        }
        rc = fsFileWrite(&file, offset, buf.data(), static_cast<u64>(got), FsWriteOption_None);
        if (R_FAILED(rc)) {
            fsFileClose(&file);
            return fail("Write", rc);
        }
        offset += got;
    }

    fsFileFlush(&file);
    fsFileClose(&file);
    fsFsCommit(sd);
    if (closeOwned)
        fsFsClose(&owned);

    romfsInit();
    SF_LOG_I("FS", "overwriteFile native OK: %s → %s (%lld bytes)", from.c_str(), fsPath.c_str(),
             static_cast<long long>(size));
    return true;
}
#endif

} // namespace

bool FileSystem::overwriteFile(const std::string& from, const std::string& to)
{
    if (from.empty() || to.empty() || from == to)
        return false;
    if (!exists(from))
        return false;

#ifdef __SWITCH__
    // Sphaira/Breeze-style: native FS write can replace a running NRO; POSIX trunc cannot.
    if (overwriteFileNative(from, to))
        return true;
    SF_LOG_W("FS", "native overwrite failed — falling back to stdio copy");
#endif
    return copyFile(from, to);
}

std::string FileSystem::toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string FileSystem::extensionOf(const std::string& path)
{
    auto slash = path.find_last_of("/\\");
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return {};
    return toLower(path.substr(dot));
}

std::string FileSystem::filenameOf(const std::string& path)
{
    auto slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
        return path;
    return path.substr(slash + 1);
}

std::string FileSystem::stemOf(const std::string& path)
{
    auto name = filenameOf(path);
    auto dot = name.find_last_of('.');
    if (dot == std::string::npos)
        return name;
    return name.substr(0, dot);
}

std::string FileSystem::join(const std::string& a, const std::string& b)
{
    if (a.empty())
        return b;
    if (b.empty())
        return a;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

static bool extAllowed(const std::string& ext, const std::vector<std::string>& extensions)
{
    for (const auto& e : extensions) {
        if (FileSystem::toLower(e) == ext)
            return true;
    }
    return false;
}

#ifdef __SWITCH__
static void scanRecursive(const std::string& dir,
                          const std::vector<std::string>& extensions,
                          std::vector<RomEntry>& out)
{
    DIR* d = opendir(dir.c_str());
    if (!d)
        return;

    dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.')
            continue;

        std::string full = FileSystem::join(dir, ent->d_name);
        struct stat st {};
        if (stat(full.c_str(), &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode)) {
            scanRecursive(full, extensions, out);
            continue;
        }

        std::string ext = FileSystem::extensionOf(full);
        if (!extAllowed(ext, extensions))
            continue;

        RomEntry e;
        e.path = full;
        e.filename = ent->d_name;
        e.stem = FileSystem::stemOf(full);
        e.extension = ext;
        e.size = static_cast<uint64_t>(st.st_size);
        out.push_back(std::move(e));
    }
    closedir(d);
}
#else
static void scanRecursive(const std::string& dir,
                          const std::vector<std::string>& extensions,
                          std::vector<RomEntry>& out)
{
    std::error_code ec;
    if (!fs::exists(dir, ec))
        return;

    for (auto it = fs::recursive_directory_iterator(dir, ec);
         it != fs::recursive_directory_iterator(); ++it) {
        if (ec)
            break;
        if (!it->is_regular_file(ec))
            continue;
        auto path = it->path().string();
        // normalize separators
        std::replace(path.begin(), path.end(), '\\', '/');
        std::string ext = FileSystem::extensionOf(path);
        if (!extAllowed(ext, extensions))
            continue;
        RomEntry e;
        e.path = path;
        e.filename = it->path().filename().string();
        e.stem = FileSystem::stemOf(path);
        e.extension = ext;
        e.size = static_cast<uint64_t>(it->file_size(ec));
        out.push_back(std::move(e));
    }
}
#endif

std::string FileSystem::parentPath(const std::string& path)
{
    if (path.empty())
        return {};
    auto end = path.size();
    while (end > 0 && path[end - 1] == '/')
        --end;
    if (end <= 1)
        return {};
    auto slash = path.rfind('/', end - 1);
    if (slash == std::string::npos)
        return {};
    if (slash == 0)
        return path.substr(0, 1);
    // preserve sdmc: prefix roots
    if (slash > 0 && path[slash - 1] == ':')
        return path.substr(0, slash + 1);
    return path.substr(0, slash);
}

#ifdef __SWITCH__
static std::vector<DirEntry> listDirectorySwitch(const std::string& path)
{
    std::vector<DirEntry> out;
    DIR* d = opendir(path.c_str());
    if (!d)
        return out;

    dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.')
            continue;

        DirEntry entry;
        entry.name = ent->d_name;
        entry.path = FileSystem::join(path, ent->d_name);

        struct stat st {};
        if (stat(entry.path.c_str(), &st) != 0)
            continue;

        entry.isDirectory = S_ISDIR(st.st_mode);
        entry.size = static_cast<uint64_t>(st.st_size);
        out.push_back(std::move(entry));
    }
    closedir(d);

    std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.isDirectory != b.isDirectory)
            return a.isDirectory > b.isDirectory;
        return a.name < b.name;
    });
    return out;
}
#else
static std::vector<DirEntry> listDirectoryDesktop(const std::string& path)
{
    std::vector<DirEntry> out;
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_directory(path, ec))
        return out;

    for (const auto& item : fs::directory_iterator(path, ec)) {
        if (ec)
            break;
        DirEntry entry;
        entry.name = item.path().filename().string();
        entry.path = item.path().string();
        std::replace(entry.path.begin(), entry.path.end(), '\\', '/');
        entry.isDirectory = item.is_directory(ec);
        if (!entry.isDirectory)
            entry.size = static_cast<uint64_t>(item.file_size(ec));
        out.push_back(std::move(entry));
    }

    std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.isDirectory != b.isDirectory)
            return a.isDirectory > b.isDirectory;
        return a.name < b.name;
    });
    return out;
}
#endif

std::vector<DirEntry> FileSystem::listDirectory(const std::string& path)
{
#ifdef __SWITCH__
    return listDirectorySwitch(path);
#else
    return listDirectoryDesktop(path);
#endif
}

std::vector<RomEntry> FileSystem::scanRoms(const std::string& root,
                                           const std::vector<std::string>& extensions)
{
    std::vector<RomEntry> out;
    if (!exists(root)) {
        SF_LOG_W("FS", "ROM root missing: %s", root.c_str());
        return out;
    }
    scanRecursive(root, extensions, out);
    std::sort(out.begin(), out.end(),
              [](const RomEntry& a, const RomEntry& b) { return a.stem < b.stem; });
    SF_LOG_I("FS", "Scanned %s -> %zu roms", root.c_str(), out.size());
    return out;
}

} // namespace sf

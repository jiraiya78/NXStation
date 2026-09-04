#include "cloud/RetroArchPaths.hpp"
#include "app/Config.hpp"
#include "util/FileSystem.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

namespace sf::cloud {

namespace {

std::string readIniValue(const std::string& text, const std::string& key)
{
    const std::string needle = key + " = \"";
    const auto pos = text.find(needle);
    if (pos == std::string::npos)
        return {};
    const auto start = pos + needle.size();
    const auto end = text.find('"', start);
    if (end == std::string::npos || end <= start)
        return {};
    return text.substr(start, end - start);
}

std::string toLower(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool endsWith(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isRomSidecarSave(const std::string& filename)
{
    const std::string lower = toLower(filename);
    if (endsWith(lower, ".srm") || endsWith(lower, ".sav") || endsWith(lower, ".eep") ||
        endsWith(lower, ".rtc") || endsWith(lower, ".auto"))
        return true;
    if (lower.find(".state") != std::string::npos)
        return true;
    return false;
}

void appendFilesRecursive(const std::string& root, const std::string& zipPrefix,
                          std::vector<SavePathEntry>& out)
{
    if (!FileSystem::exists(root))
        return;

    for (const auto& entry : FileSystem::listDirectory(root)) {
        const std::string rel = zipPrefix.empty() ? entry.name : zipPrefix + "/" + entry.name;
        if (entry.isDirectory)
            appendFilesRecursive(entry.path, rel, out);
        else
            out.push_back({entry.path, rel});
    }
}

void appendRomSidecarsRecursive(const std::string& root, const std::string& zipPrefix,
                                std::vector<SavePathEntry>& out)
{
    if (!FileSystem::exists(root))
        return;

    for (const auto& entry : FileSystem::listDirectory(root)) {
        const std::string rel = zipPrefix.empty() ? entry.name : zipPrefix + "/" + entry.name;
        if (entry.isDirectory) {
            appendRomSidecarsRecursive(entry.path, rel, out);
            continue;
        }
        if (!isRomSidecarSave(entry.name))
            continue;
        out.push_back({entry.path, rel});
    }
}

} // namespace

RetroArchSaveRoots discoverRetroArchSaveRoots()
{
    RetroArchSaveRoots roots;
    const std::string cfgPath = "sdmc:/retroarch/retroarch.cfg";
    const std::string cfg = FileSystem::readFile(cfgPath);

    roots.savesDir = readIniValue(cfg, "savefile_directory");
    roots.statesDir = readIniValue(cfg, "savestate_directory");

    if (roots.savesDir.empty())
        roots.savesDir = "sdmc:/retroarch/saves";
    if (roots.statesDir.empty())
        roots.statesDir = "sdmc:/retroarch/states";

    return roots;
}

std::vector<SavePathEntry> collectAllSavePaths()
{
    std::vector<SavePathEntry> out;

    const RetroArchSaveRoots roots = discoverRetroArchSaveRoots();
    appendFilesRecursive(roots.savesDir, "saves", out);
    appendFilesRecursive(roots.statesDir, "states", out);

    for (const auto& sys : Config::instance().systems()) {
        if (sys.path.empty())
            continue;
        appendRomSidecarsRecursive(sys.path, "rom_saves/" + sys.id, out);
    }

    std::sort(out.begin(), out.end(),
              [](const SavePathEntry& a, const SavePathEntry& b) { return a.absPath < b.absPath; });
    out.erase(std::unique(out.begin(), out.end(),
                          [](const SavePathEntry& a, const SavePathEntry& b) {
                              return a.absPath == b.absPath;
                          }),
              out.end());
    return out;
}

std::optional<std::string> resolveRestoreTarget(const std::string& zipEntryPath)
{
    std::string entry = zipEntryPath;
    for (char& c : entry) {
        if (c == '\\')
            c = '/';
    }
    if (entry.empty() || entry.back() == '/')
        return std::nullopt;

    const RetroArchSaveRoots roots = discoverRetroArchSaveRoots();

    if (entry.rfind("saves/", 0) == 0) {
        const std::string rel = entry.substr(6);
        if (rel.empty())
            return std::nullopt;
        return FileSystem::join(roots.savesDir, rel);
    }
    if (entry.rfind("states/", 0) == 0) {
        const std::string rel = entry.substr(7);
        if (rel.empty())
            return std::nullopt;
        return FileSystem::join(roots.statesDir, rel);
    }
    if (entry.rfind("rom_saves/", 0) == 0) {
        const std::string rest = entry.substr(10);
        const auto slash = rest.find('/');
        if (slash == std::string::npos)
            return std::nullopt;
        const std::string systemId = rest.substr(0, slash);
        const std::string relFile = rest.substr(slash + 1);
        if (relFile.empty())
            return std::nullopt;
        const SystemConfig* sys = Config::instance().findSystem(systemId);
        if (!sys || sys->path.empty())
            return std::nullopt;
        return FileSystem::join(sys->path, relFile);
    }

    return std::nullopt;
}

} // namespace sf::cloud

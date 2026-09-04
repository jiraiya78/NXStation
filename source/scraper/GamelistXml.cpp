#include "scraper/GamelistXml.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"

#include <tinyxml2.h>

#include <algorithm>

namespace sf {

namespace {

std::string textOf(const tinyxml2::XMLElement* parent, const char* tag)
{
    if (!parent)
        return {};
    const tinyxml2::XMLElement* child = parent->FirstChildElement(tag);
    if (!child || !child->GetText())
        return {};
    return child->GetText();
}

GameMetadata parseGame(const tinyxml2::XMLElement* game, const std::string& romRoot,
                       const std::string& systemId)
{
    GameMetadata m;
    m.systemId = systemId;
    m.scraped = true;

    const std::string pathField = textOf(game, "path");
    m.romPath = GamelistXml::resolvePath(romRoot, pathField);
    m.title = textOf(game, "name");
    m.description = textOf(game, "desc");
    m.developer = textOf(game, "developer");
    m.publisher = textOf(game, "publisher");
    m.genre = textOf(game, "genre");
    m.releaseDate = textOf(game, "releasedate");
    m.crc32 = textOf(game, "md5");
    m.boxArtPath = GamelistXml::resolvePath(romRoot, textOf(game, "image"));
    m.logoPath = GamelistXml::resolvePath(romRoot, textOf(game, "thumbnail"));
    m.videoPath = GamelistXml::resolvePath(romRoot, textOf(game, "video"));
    m.manualPath = GamelistXml::resolvePath(romRoot, textOf(game, "manual"));

    if (m.title.empty() && !m.romPath.empty())
        m.title = FileSystem::stemOf(m.romPath);

    return m;
}

std::string esdeImagePath(const std::string& romRoot, const std::string& romStem)
{
    const std::string dir = FileSystem::join(romRoot, "images");
    for (const char* ext : {".png", ".jpg", ".jpeg", ".webp"}) {
        const std::string path = FileSystem::join(dir, romStem + "-image" + ext);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

std::string esdeThumbPath(const std::string& romRoot, const std::string& romStem)
{
    const std::string dir = FileSystem::join(romRoot, "images");
    for (const char* ext : {".png", ".jpg", ".jpeg", ".webp"}) {
        const std::string path = FileSystem::join(dir, romStem + "-thumb" + ext);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

std::string esdeManualPdfPath(const std::string& romRoot, const std::string& romStem)
{
    const std::string dir = FileSystem::join(romRoot, "manuals");
    for (const char* ext : {".pdf"}) {
        const std::string path = FileSystem::join(dir, romStem + "-manual" + ext);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

std::string esdeVideoPath(const std::string& romRoot, const std::string& romStem)
{
    const std::string dir = FileSystem::join(romRoot, "videos");
    for (const char* ext : {".mp4", ".webm", ".mkv"}) {
        const std::string path = FileSystem::join(dir, romStem + "-video" + ext);
        if (FileSystem::exists(path))
            return path;
    }
    return {};
}

void setOrCreateText(tinyxml2::XMLElement* game, const char* tag, const std::string& value)
{
    tinyxml2::XMLElement* child = game->FirstChildElement(tag);
    if (value.empty()) {
        if (child)
            game->DeleteChild(child);
        return;
    }
    if (!child) {
        child = game->GetDocument()->NewElement(tag);
        game->InsertEndChild(child);
    }
    child->SetText(value.c_str());
}

} // namespace

std::string GamelistXml::gamelistPath(const std::string& romRoot)
{
    return FileSystem::join(romRoot, "gamelist.xml");
}

std::string GamelistXml::resolvePath(const std::string& romRoot, const std::string& entry)
{
    if (entry.empty())
        return {};
    if (entry.find(":/") != std::string::npos)
        return entry;
    if (entry.front() == '/')
        return entry;
    std::string rel = entry;
    if (rel.rfind("./", 0) == 0)
        rel = rel.substr(2);
    return FileSystem::join(romRoot, rel);
}

std::string GamelistXml::romKeyFromPath(const std::string& pathField)
{
    std::string key = pathField;
    if (key.rfind("./", 0) == 0)
        key = key.substr(2);
    return FileSystem::filenameOf(key);
}

GamelistXml::EntryMap GamelistXml::load(const std::string& romRoot, const std::string& systemId)
{
    EntryMap out;
    const std::string path = gamelistPath(romRoot);
    const std::string xml = FileSystem::readFile(path);
    if (xml.empty())
        return out;

    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
        SF_LOG_W("Meta", "Failed to parse %s", path.c_str());
        return out;
    }

    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root)
        return out;

    for (const tinyxml2::XMLElement* game = root->FirstChildElement("game"); game;
         game = game->NextSiblingElement("game")) {
        GameMetadata meta = parseGame(game, romRoot, systemId);
        const std::string key = romKeyFromPath(textOf(game, "path"));
        if (key.empty())
            continue;
        out[key] = std::move(meta);
    }

    SF_LOG_I("Meta", "Loaded %zu entries from %s", out.size(), path.c_str());
    return out;
}

void GamelistXml::applyFallbackMedia(GameMetadata& meta, const std::string& romRoot,
                                     const std::string& romFilename, const std::string& romStem)
{
    (void)romFilename;
    if (meta.boxArtPath.empty() || !FileSystem::exists(meta.boxArtPath)) {
        if (std::string box = esdeImagePath(romRoot, romStem); !box.empty())
            meta.boxArtPath = std::move(box);
    }
    if (meta.logoPath.empty() || !FileSystem::exists(meta.logoPath)) {
        if (std::string thumb = esdeThumbPath(romRoot, romStem); !thumb.empty())
            meta.logoPath = std::move(thumb);
    }
    if (meta.videoPath.empty() || !FileSystem::exists(meta.videoPath)) {
        if (std::string video = esdeVideoPath(romRoot, romStem); !video.empty())
            meta.videoPath = std::move(video);
    }
    if (meta.manualPath.empty() || !FileSystem::exists(meta.manualPath)) {
        if (std::string manual = esdeManualPdfPath(romRoot, romStem); !manual.empty())
            meta.manualPath = std::move(manual);
    }
}

bool GamelistXml::saveEntry(const std::string& romRoot, const GameMetadata& meta,
                            const std::string& romFilename)
{
    if (romFilename.empty())
        return false;

    const std::string path = gamelistPath(romRoot);
    FileSystem::createDirectories(romRoot);
    FileSystem::createDirectories(FileSystem::join(romRoot, "images"));
    FileSystem::createDirectories(FileSystem::join(romRoot, "videos"));
    FileSystem::createDirectories(FileSystem::join(romRoot, "manuals"));

    tinyxml2::XMLDocument doc;
    const std::string existing = FileSystem::readFile(path);
    if (!existing.empty()) {
        if (doc.Parse(existing.c_str(), existing.size()) != tinyxml2::XML_SUCCESS) {
            SF_LOG_W("Meta", "gamelist.xml corrupt — rewriting %s", path.c_str());
            doc.Clear();
        }
    }

    tinyxml2::XMLElement* root = doc.RootElement();
    if (!root) {
        auto* decl = doc.NewDeclaration(R"(xml version="1.0" encoding="UTF-8")");
        doc.InsertFirstChild(decl);
        root = doc.NewElement("gameList");
        doc.InsertEndChild(root);
    }

    const std::string relPath = "./" + romFilename;
    tinyxml2::XMLElement* target = nullptr;
    for (tinyxml2::XMLElement* game = root->FirstChildElement("game"); game;
         game = game->NextSiblingElement("game")) {
        if (romKeyFromPath(textOf(game, "path")) == romFilename) {
            target = game;
            break;
        }
    }

    if (!target) {
        target = doc.NewElement("game");
        root->InsertEndChild(target);
        setOrCreateText(target, "path", relPath);
    }

    auto relMedia = [&](const std::string& abs) -> std::string {
        if (abs.empty())
            return {};
        if (abs.rfind(romRoot, 0) == 0) {
            std::string rel = abs.substr(romRoot.size());
            if (!rel.empty() && rel.front() == '/')
                rel.erase(rel.begin());
            return "./" + rel;
        }
        return abs;
    };

    setOrCreateText(target, "name", meta.title);
    setOrCreateText(target, "desc", meta.description);
    setOrCreateText(target, "developer", meta.developer);
    setOrCreateText(target, "publisher", meta.publisher);
    setOrCreateText(target, "genre", meta.genre);
    setOrCreateText(target, "releasedate", meta.releaseDate);
    setOrCreateText(target, "md5", meta.crc32);
    setOrCreateText(target, "image", relMedia(meta.boxArtPath));
    setOrCreateText(target, "thumbnail", relMedia(meta.logoPath));
    setOrCreateText(target, "video", relMedia(meta.videoPath));
    setOrCreateText(target, "manual", relMedia(meta.manualPath));

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    if (!FileSystem::writeFile(path, printer.CStr())) {
        SF_LOG_E("Meta", "Failed to write %s", path.c_str());
        return false;
    }
    return true;
}

bool GamelistXml::saveList(const std::string& romRoot, const std::vector<GameItem>& games)
{
    FileSystem::createDirectories(romRoot);

    tinyxml2::XMLDocument doc;
    auto* decl = doc.NewDeclaration(R"(xml version="1.0" encoding="UTF-8")");
    doc.InsertFirstChild(decl);
    auto* root = doc.NewElement("gameList");
    doc.InsertEndChild(root);

    auto relMedia = [&](const std::string& abs) -> std::string {
        if (abs.empty())
            return {};
        if (abs.rfind(romRoot, 0) == 0) {
            std::string rel = abs.substr(romRoot.size());
            if (!rel.empty() && rel.front() == '/')
                rel.erase(rel.begin());
            return "./" + rel;
        }
        return abs;
    };

    for (const auto& g : games) {
        auto* game = doc.NewElement("game");
        root->InsertEndChild(game);

        setOrCreateText(game, "path", g.path);
        setOrCreateText(game, "name", g.displayName);
        setOrCreateText(game, "desc", g.meta.description);
        setOrCreateText(game, "developer", g.meta.developer);
        setOrCreateText(game, "publisher", g.meta.publisher);
        setOrCreateText(game, "genre", g.meta.genre);
        setOrCreateText(game, "releasedate", g.meta.releaseDate);
        setOrCreateText(game, "image", relMedia(g.meta.boxArtPath));
        setOrCreateText(game, "thumbnail", relMedia(g.meta.logoPath));
        setOrCreateText(game, "video", relMedia(g.meta.videoPath));
        setOrCreateText(game, "manual", relMedia(g.meta.manualPath));
        setOrCreateText(game, "nxstationSystem", g.systemId);
    }

    tinyxml2::XMLPrinter printer;
    doc.Print(&printer);
    const std::string path = gamelistPath(romRoot);
    if (!FileSystem::writeFile(path, printer.CStr())) {
        SF_LOG_E("Meta", "Failed to write %s", path.c_str());
        return false;
    }
    SF_LOG_I("Meta", "Wrote %zu entries to %s", games.size(), path.c_str());
    return true;
}

} // namespace sf

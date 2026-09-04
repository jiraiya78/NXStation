#include "cloud/ZipArchive.hpp"
#include "util/FileSystem.hpp"
#include "util/Logger.hpp"

#ifdef __SWITCH__
#include <minizip/zip.h>
#include <minizip/unzip.h>
#endif

#include <ctime>
#include <fstream>
#include <vector>

namespace sf::cloud {

namespace {

#ifdef __SWITCH__
zip_fileinfo makeZipInfo()
{
    zip_fileinfo info{};
    const std::time_t now = std::time(nullptr);
    if (const std::tm* local = std::localtime(&now)) {
        info.tmz_date.tm_sec = local->tm_sec;
        info.tmz_date.tm_min = local->tm_min;
        info.tmz_date.tm_hour = local->tm_hour;
        info.tmz_date.tm_mday = local->tm_mday;
        info.tmz_date.tm_mon = local->tm_mon;
        info.tmz_date.tm_year = local->tm_year;
    }
    return info;
}

bool writeFileToZip(zipFile zip, const std::string& zipName, const std::string& sourcePath)
{
    std::ifstream in(sourcePath, std::ios::binary);
    if (!in)
        return false;

    const zip_fileinfo info = makeZipInfo();
    if (zipOpenNewFileInZip64(zip, zipName.c_str(), &info, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED,
                              Z_DEFAULT_COMPRESSION, 0) != ZIP_OK)
        return false;

    std::vector<char> buffer(64 * 1024);
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = in.gcount();
        if (got <= 0)
            break;
        if (zipWriteInFileInZip(zip, buffer.data(), static_cast<unsigned>(got)) != ZIP_OK) {
            zipCloseFileInZip(zip);
            return false;
        }
    }

    return zipCloseFileInZip(zip) == ZIP_OK;
}
#endif

} // namespace

bool ZipArchive::create(const std::string& zipPath, const std::vector<std::string>& files,
                        const std::vector<std::string>& zipNames)
{
    if (files.size() != zipNames.size())
        return false;

#ifndef __SWITCH__
    (void)zipPath;
    (void)files;
    (void)zipNames;
    SF_LOG_W("Cloud", "ZIP creation requires Switch build");
    return false;
#else
    FileSystem::createDirectories(FileSystem::parentPath(zipPath));

    zipFile zip = zipOpen64(zipPath.c_str(), APPEND_STATUS_CREATE);
    if (!zip) {
        SF_LOG_E("Cloud", "zipOpen64 failed: %s", zipPath.c_str());
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < files.size(); ++i) {
        if (!FileSystem::exists(files[i]))
            continue;
        if (!writeFileToZip(zip, zipNames[i], files[i])) {
            SF_LOG_W("Cloud", "Failed to zip %s", files[i].c_str());
            ok = false;
            break;
        }
    }

    zipClose(zip, nullptr);
    return ok;
#endif
}

bool ZipArchive::extractMerge(const std::string& zipPath, ZipTargetMapper mapper, ZipExtractProgress progress,
                              std::atomic<bool>* abort)
{
#ifndef __SWITCH__
    (void)zipPath;
    (void)mapper;
    (void)progress;
    (void)abort;
    SF_LOG_W("Cloud", "ZIP extract requires Switch build");
    return false;
#else
    if (!mapper)
        return false;

    unzFile zip = unzOpen64(zipPath.c_str());
    if (!zip) {
        SF_LOG_E("Cloud", "unzOpen64 failed: %s", zipPath.c_str());
        return false;
    }

    unz_global_info64 globalInfo {};
    if (unzGetGlobalInfo64(zip, &globalInfo) != UNZ_OK) {
        unzClose(zip);
        return false;
    }

    bool ok = true;
    for (uLong i = 0; i < globalInfo.number_entry; ++i) {
        if (abort && abort->load()) {
            ok = false;
            break;
        }

        char nameBuf[512] = {};
        unz_file_info64 info {};
        if (unzGetCurrentFileInfo64(zip, &info, nameBuf, sizeof(nameBuf) - 1, nullptr, 0, nullptr, 0) !=
            UNZ_OK) {
            ok = false;
            break;
        }

        std::string zipName(nameBuf);
        if (!zipName.empty() && zipName.back() == '/') {
            if (i + 1 < globalInfo.number_entry && unzGoToNextFile(zip) != UNZ_OK) {
                ok = false;
                break;
            }
            continue;
        }

        const auto dest = mapper(zipName);
        if (!dest) {
            if (progress)
                progress(zipName, {}, true, true);
            if (i + 1 < globalInfo.number_entry && unzGoToNextFile(zip) != UNZ_OK) {
                ok = false;
                break;
            }
            continue;
        }

        FileSystem::createDirectories(FileSystem::parentPath(*dest));

        if (unzOpenCurrentFile(zip) != UNZ_OK) {
            ok = false;
            if (progress)
                progress(zipName, *dest, false, false);
            if (i + 1 < globalInfo.number_entry)
                unzGoToNextFile(zip);
            continue;
        }

        std::ofstream out(*dest, std::ios::binary | std::ios::trunc);
        if (!out) {
            unzCloseCurrentFile(zip);
            ok = false;
            if (progress)
                progress(zipName, *dest, false, false);
            if (i + 1 < globalInfo.number_entry)
                unzGoToNextFile(zip);
            continue;
        }

        std::vector<char> buffer(64 * 1024);
        bool writeOk = true;
        while (true) {
            if (abort && abort->load()) {
                writeOk = false;
                ok = false;
                break;
            }
            const int read = unzReadCurrentFile(zip, buffer.data(), static_cast<unsigned>(buffer.size()));
            if (read < 0) {
                writeOk = false;
                ok = false;
                break;
            }
            if (read == 0)
                break;
            out.write(buffer.data(), read);
            if (!out) {
                writeOk = false;
                ok = false;
                break;
            }
        }

        out.close();
        unzCloseCurrentFile(zip);

        if (progress)
            progress(zipName, *dest, writeOk, false);

        if (i + 1 < globalInfo.number_entry && unzGoToNextFile(zip) != UNZ_OK) {
            ok = false;
            break;
        }
    }

    unzClose(zip);
    return ok;
#endif
}

} // namespace sf::cloud

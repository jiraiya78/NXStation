#include "media/ManualPages.hpp"

#include "ui/ManualViewerView.hpp"
#include "util/FileSystem.hpp"

#include <borealis.hpp>
#include <functional>



#include <cctype>



namespace sf::ManualPages {



namespace {



bool endsWithIgnoreCase(const std::string& value, const std::string& suffix)

{

    if (value.size() < suffix.size())

        return false;

    const size_t start = value.size() - suffix.size();

    for (size_t i = 0; i < suffix.size(); ++i) {

        if (std::tolower(static_cast<unsigned char>(value[start + i]))

            != std::tolower(static_cast<unsigned char>(suffix[i])))

            return false;

    }

    return true;

}



} // namespace



std::string pdfPath(const std::string& romRoot, const std::string& romStem)

{

    return FileSystem::join(FileSystem::join(romRoot, "manuals"), romStem + "-manual.pdf");

}



std::string resolvePdf(const std::string& romRoot, const std::string& romStem,

                       const GameMetadata& meta)

{

    if (!meta.manualPath.empty() && FileSystem::exists(meta.manualPath)

        && endsWithIgnoreCase(meta.manualPath, ".pdf"))

        return meta.manualPath;

    const std::string fallback = pdfPath(romRoot, romStem);

    if (FileSystem::exists(fallback))

        return fallback;

    return {};

}



bool hasViewableManual(const std::string& romRoot, const std::string& romStem,

                       const GameMetadata& meta)

{

    return !resolvePdf(romRoot, romStem, meta).empty();

}

bool hasManualEntry(const std::string& romRoot, const std::string& romStem,
                    const GameMetadata& meta)
{
    if (!meta.manualPath.empty())
        return true;
    return FileSystem::exists(pdfPath(romRoot, romStem));
}

bool tryOpenManual(const std::string& romRoot, const std::string& romStem,
                   const GameMetadata& meta, const std::string& displayTitle,
                   std::function<void()> onDismiss)
{
    const std::string pdf = resolvePdf(romRoot, romStem, meta);
    if (pdf.empty() || !FileSystem::exists(pdf)) {
        brls::Application::notify("Manual file not found");
        return false;
    }
    sf::ui::ManualViewerView::present(pdf, displayTitle, std::move(onDismiss));
    return true;
}



bool urlLooksLikePdf(const std::string& url)

{

    return endsWithIgnoreCase(url, ".pdf") || url.find("mediaManuelJeu.php") != std::string::npos;

}



} // namespace sf::ManualPages


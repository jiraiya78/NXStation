#include "ui/MetricIcons.hpp"

namespace sf::ui {

namespace {

std::string metricRes(const char* id)
{
    return std::string("img/metrics/") + id + ".png";
}

} // namespace

std::string personalityTagIconRes(const std::string& primaryTag)
{
    if (primaryTag == "The 16-Bit Purist")
        return metricRes("16bit_purist");
    if (primaryTag == "The 8-Bit Pioneer")
        return metricRes("8bit_pioneer");
    if (primaryTag == "The Polygon Crusader")
        return metricRes("polygon_crusader");
    if (primaryTag == "The Arcade Junkie")
        return metricRes("arcade_junkie");
    if (primaryTag == "The Serial Sampler")
        return metricRes("serial_sampler");
    if (primaryTag == "The Laser-Focused Completionist")
        return metricRes("completionist");
    if (primaryTag == "The JRPG Scholar")
        return metricRes("jrpg_scholar");
    if (primaryTag == "The Retro Renaissance Gamer")
        return metricRes("renaissance");
    return metricRes("empty");
}

std::string gamingStyleIconRes(const std::string& gamingStyle)
{
    if (gamingStyle.find("Micro-Burst") != std::string::npos)
        return metricRes("micro_burst");
    if (gamingStyle.find("Casual") != std::string::npos)
        return metricRes("casual");
    if (gamingStyle.find("Deep Dive") != std::string::npos)
        return metricRes("deep_dive");
    if (gamingStyle.find("Marathon") != std::string::npos)
        return metricRes("marathon");
    return metricRes("session_style");
}

std::string analyticsSectionIconRes(const std::string& sectionKey)
{
    if (sectionKey == "overview")
        return metricRes("overview");
    if (sectionKey == "timewarp")
        return metricRes("timewarp");
    if (sectionKey == "sources")
        return metricRes("sources");
    if (sectionKey == "session")
        return metricRes("session_style");
    if (sectionKey == "timeofday")
        return metricRes("time_of_day");
    if (sectionKey == "heatmap")
        return metricRes("heatmap");
    if (sectionKey == "backlog")
        return metricRes("backlog_dust");
    if (sectionKey == "note")
        return metricRes("note");
    if (sectionKey == "empty")
        return metricRes("empty");
    return metricRes("empty");
}

} // namespace sf::ui

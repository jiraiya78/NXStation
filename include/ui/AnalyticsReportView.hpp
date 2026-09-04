#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

namespace sf::ui {

class FocusedMenuDialog;

struct AnalyticsSection {
    std::string title;
    std::string body;
    /** romfs resource path (e.g. img/metrics/overview.png). */
    std::string iconRes;
};

struct AnalyticsReportSpec {
    std::string windowTitle;
    std::string heroIconRes;
    std::string heroTitle;
    std::string heroSubtitle;
    std::vector<AnalyticsSection> sections;
};

/** Modal analytics report floating over the main carousel (FocusedMenuDialog). */
class AnalyticsReportView : public brls::Box {
public:
    explicit AnalyticsReportView(AnalyticsReportSpec spec);
    ~AnalyticsReportView() override = default;

    static void present(AnalyticsReportSpec spec);

private:
    FocusedMenuDialog* dialog_ = nullptr;
    brls::ScrollingFrame* scroller_ = nullptr;
};

} // namespace sf::ui

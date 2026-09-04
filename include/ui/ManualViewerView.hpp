#pragma once



#include <borealis.hpp>

#include <functional>

#include <memory>

#include <string>



namespace sf {



class PdfManual;



} // namespace sf



namespace sf::ui {



class FocusedMenuDialog;



enum class ManualLayoutMode {

    /** Page 1 alone, remaining pages shown as two-page spreads. */

    CoverThenSpread,

    /** One page per view. */

    SinglePage,

};



/** Full-screen game-manual page viewer (opens over metadata dialog). */

class ManualViewerView : public brls::Box {

public:

    ManualViewerView(std::string pdfPath, std::string title);

    ~ManualViewerView() override;



    static void present(std::string pdfPath, std::string title,
                        std::function<void()> onDismiss = nullptr);

    /** Release GPU textures and close the PDF document. */
    void shutdown();



private:

    class ManualPageCanvas;



    void refreshPageLabel();

    void showSpread(size_t spreadIndex);

    void nextSpread();

    void prevSpread();

    void registerNavigationActions();

    void openLayoutMenu();

    size_t maxSpreadIndex() const;

    size_t pageCount() const;

    void getSpreadPages(size_t spreadIndex, int& leftPage, int& rightPage) const;



    std::unique_ptr<PdfManual> pdf_;

    std::string title_;

    ManualLayoutMode layoutMode_ = ManualLayoutMode::CoverThenSpread;

    size_t spreadIndex_ = 0;

    ManualPageCanvas* canvas_ = nullptr;

    brls::Label* pageLabel_ = nullptr;

    brls::Label* layoutHint_ = nullptr;

    FocusedMenuDialog* dialog_ = nullptr;

    bool shutdown_ = false;

};



} // namespace sf::ui


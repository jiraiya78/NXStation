#pragma once



#include "app/Models.hpp"

#include <functional>
#include <string>



namespace sf {



/** Game-manual storage under each ROM folder: manuals/{stem}-manual.pdf */

namespace ManualPages {



std::string pdfPath(const std::string& romRoot, const std::string& romStem);



/** Resolved local PDF path from metadata or default location. */

std::string resolvePdf(const std::string& romRoot, const std::string& romStem,

                       const GameMetadata& meta);



bool hasViewableManual(const std::string& romRoot, const std::string& romStem,

                       const GameMetadata& meta);

/** True when metadata or the default manuals path indicates a manual for this game. */
bool hasManualEntry(const std::string& romRoot, const std::string& romStem,
                    const GameMetadata& meta);

/** Open the manual viewer, or show a toast when the PDF is missing. Returns true if opened. */
bool tryOpenManual(const std::string& romRoot, const std::string& romStem,
                   const GameMetadata& meta, const std::string& displayTitle,
                   std::function<void()> onDismiss = nullptr);

bool urlLooksLikePdf(const std::string& url);



} // namespace ManualPages



} // namespace sf


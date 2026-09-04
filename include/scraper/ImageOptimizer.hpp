#pragma once

#include <string>

namespace sf {

/** Re-encode a scraped image in place (JPEG, resized) for smaller SD footprint. */
bool optimizeScrapedImage(const std::string& path, bool thumbnail);

} // namespace sf

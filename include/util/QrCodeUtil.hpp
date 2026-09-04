#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sf {

/** Encode text as a PNG QR code (for brls::Image::setImageFromMem). */
std::vector<uint8_t> encodeQrPng(const std::string& text, int moduleScale = 6, int quietBorder = 4);

} // namespace sf

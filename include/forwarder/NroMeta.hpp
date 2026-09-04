#pragma once

#include <switch.h>

#include <string>
#include <vector>

namespace sf {

/** Read embedded NACP from an .nro file. */
Result readNroNacp(const std::string& path, NacpStruct& nacp);

/** Read embedded JPEG icon bytes from an .nro file. */
std::vector<u8> readNroIcon(const std::string& path);

/** Quote an sdmc path for hbloader argv if it contains spaces. */
std::string nroArgvPath(const std::string& path);

} // namespace sf

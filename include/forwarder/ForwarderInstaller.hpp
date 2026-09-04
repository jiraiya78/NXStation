#pragma once

#include <functional>
#include <string>

namespace sf::ForwarderInstaller {

/** prod.keys on SD (required for on-device NCA generation). */
bool keysAvailable();

/** True when an NXStation main-menu forwarder title is registered for this NRO. */
bool isForwarderInstalled();

/**
 * Install an NXStation forwarder to the Switch main menu (NCA install).
 * @param onProgress Optional status callback for UI.
 */
bool installNxStation(std::string& errorOut, std::function<void(const std::string&)> onProgress = nullptr);

} // namespace sf::ForwarderInstaller

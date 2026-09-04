#pragma once

#include <string>

namespace sf {

/** Write NXStation return path for optional hbloader / forwarder integrations. */
void writeReturnChainMarker();

/** True if an older build swapped sdmc:/hbmenu.nro for NXStation. */
bool hbmenuWasReplaced();

/**
 * Undo the legacy hbmenu swap: restore sdmc:/hbmenu.nro from our backup and
 * clear the marker. Returns true when the original hbmenu was put back.
 */
bool restoreHbmenuBackup(std::string& errorOut);

} // namespace sf

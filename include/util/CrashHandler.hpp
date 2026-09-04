#pragma once



namespace sf {



/**

 * Writes a register dump and frame-pointer backtrace to

 * `sdmc:/switch/NXStation/log/crash.log` when the process faults.

 *

 * Addresses are reported both raw and relative to the module base; the relative value maps

 * directly onto `build_switch/NXStation.elf`:

 *   aarch64-none-elf-addr2line -Cfe build_switch/NXStation.elf <elf address>

 *

 * Delivery depends on the host process allowing user exception handlers. When it does not,

 * Atmosphère still writes a report to `sdmc:/atmosphere/crash_reports/`.

 */

class CrashHandler {

public:

    static void install();

};



} // namespace sf


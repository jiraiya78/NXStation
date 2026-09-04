#include "util/CrashHandler.hpp"



#if defined(__SWITCH__)



#include "util/FileSystem.hpp"

#include "util/Logger.hpp"

#include "util/Paths.hpp"



#include <cstdio>

#include <switch.h>



// Provided by the NRO linker script; the module is position independent, so subtracting

// its runtime address from a faulting PC yields the offset to feed to addr2line against

// build_switch/NXStation.elf.

extern "C" char _start[];



extern "C" {



alignas(16) u8 __nx_exception_stack[0x4000];

u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);



void __libnx_exception_handler(ThreadExceptionDump* ctx)

{

    // Buffered Info/Debug lines are the run-up to the fault; get them on disk first.

    sf::Logger::instance().flushNoLock();

    sf::FileSystem::createDirectories(sf::paths::LOG_DIR);

    FILE* f = std::fopen(sf::paths::CRASH_LOG_PATH, "a");

    if (!f)

        return;



    const u64 base = reinterpret_cast<u64>(_start);



    std::fprintf(f, "=== NXStation crash ===\n");

    std::fprintf(f, "error_desc = 0x%x\n", ctx->error_desc);

    std::fprintf(f, "module_base = 0x%016lx\n", base);

    std::fprintf(f, "pc  = 0x%016lx (elf 0x%lx)\n", ctx->pc.x, ctx->pc.x - base);

    std::fprintf(f, "lr  = 0x%016lx (elf 0x%lx)\n", ctx->lr.x, ctx->lr.x - base);

    std::fprintf(f, "fp  = 0x%016lx\n", ctx->fp.x);

    std::fprintf(f, "sp  = 0x%016lx\n", ctx->sp.x);

    std::fprintf(f, "far = 0x%016lx\n", ctx->far.x);

    std::fprintf(f, "esr = 0x%08x  pstate = 0x%08x\n", ctx->esr, ctx->pstate);



    for (int i = 0; i < 29; ++i)

        std::fprintf(f, "x%-2d = 0x%016lx\n", i, ctx->cpu_gprs[i].x);



    // Frame-pointer walk: each frame is { saved fp, saved lr }.

    std::fprintf(f, "-- backtrace --\n");

    u64 fp = ctx->fp.x;

    for (int depth = 0; depth < 24 && fp != 0 && (fp & 0xF) == 0; ++depth) {

        const u64* frame = reinterpret_cast<const u64*>(fp);

        const u64 nextFp = frame[0];

        const u64 lr = frame[1];

        if (lr == 0)

            break;

        std::fprintf(f, "  #%02d 0x%016lx (elf 0x%lx)\n", depth, lr, lr - base);

        if (nextFp <= fp)

            break;

        fp = nextFp;

    }



    std::fprintf(f, "=======================\n\n");

    std::fflush(f);

    std::fclose(f);

}



} // extern "C"



namespace sf {



void CrashHandler::install()

{

    // Nothing to do at runtime: defining __nx_exception_stack / __libnx_exception_handler

    // overrides the libnx weak symbols. This call exists so the translation unit is always

    // pulled in by the linker.

}



} // namespace sf



#else



namespace sf {

void CrashHandler::install() {}

} // namespace sf



#endif


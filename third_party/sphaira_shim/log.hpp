#pragma once

#define log_write(...) ((void)0)
#define log_write_arg(...) ((void)0)

inline bool log_file_init() { return true; }
inline bool log_nxlink_init() { return true; }
inline void log_file_exit() {}
inline void log_nxlink_exit() {}
inline bool log_is_init() { return false; }

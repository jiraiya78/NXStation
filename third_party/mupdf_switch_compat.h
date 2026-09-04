#pragma once

#include <time.h>

/* newlib on Switch lacks timegm(); PDF date parsing only needs a stable UTC helper. */
static inline time_t timegm(struct tm* tm)
{
    return mktime(tm);
}

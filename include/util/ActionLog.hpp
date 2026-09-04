#pragma once

#include "util/Logger.hpp"

/** Log a user-facing action (button press, menu pick, navigation). */
#define SF_LOG_ACTION(name) SF_LOG_I("Action", "%s", (name))

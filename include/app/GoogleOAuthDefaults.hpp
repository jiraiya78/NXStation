#pragma once

/**
 * Google OAuth client credentials for Cloud Save (Drive backup/restore).
 *
 * Release builds ship compiled-in credentials so users only link their Google
 * account. For public source checkouts, copy GoogleOAuthDefaults.local.hpp.example
 * to GoogleOAuthDefaults.local.hpp and fill in your OAuth Desktop client.
 * The local file is gitignored.
 */
#if defined(__has_include)
#if __has_include("GoogleOAuthDefaults.local.hpp")
#include "GoogleOAuthDefaults.local.hpp"
#endif
#endif

namespace sf::googleoauth {

#ifndef NXSTATION_GOOGLE_OAUTH_CLIENT_ID
inline constexpr const char* kClientId = "";
inline constexpr const char* kClientSecret = "";
#endif

inline bool credentialsConfigured()
{
    return kClientId[0] != '\0' && kClientSecret[0] != '\0';
}

} // namespace sf::googleoauth

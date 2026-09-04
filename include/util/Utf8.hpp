#pragma once

#include <cstdint>
#include <string>

namespace sf::Utf8 {

/** Append a Unicode code point as UTF-8. */
void appendCodePoint(std::string& out, uint32_t cp);

/** Decode a JSON \\uXXXX escape (optionally surrogate pair) from hex digits. */
std::string decodeJsonEscape(const char hex[4], char hex2[4], bool hasSecond);

/** First meaningful character for alphabet jump (ASCII letter/digit or UTF-8 lead byte bucket). */
char sortBucket(const std::string& text);

/** Case-insensitive substring search (ASCII letters; UTF-8 bytes compared literally). */
bool containsInsensitive(const std::string& haystack, const std::string& needle);

/** Accent/diacritic-insensitive search (e.g. "poke" matches "Pokémon"). */
bool containsFolded(const std::string& haystack, const std::string& needle);

/** Lowercase + strip accents for fuzzy matching. */
std::string foldForSearch(const std::string& text);

/** Case-insensitive prefix match on first character bucket. */
bool startsWithBucket(const std::string& text, char bucket);

/** True if @a s is valid UTF-8. */
bool isValidUtf8(const std::string& s);

/** Treat each byte as ISO-8859-1 and encode as UTF-8. */
std::string fromLatin1(const std::string& latin1);

/** Return UTF-8 text; if invalid UTF-8, assume Latin-1 source bytes. */
std::string ensureUtf8(const std::string& s);

/** Case-insensitive "less than" for sorting display titles. */
bool compareTitles(const std::string& a, const std::string& b);

} // namespace sf::Utf8

#include "util/Utf8.hpp"

#include <algorithm>
#include <cctype>

namespace sf::Utf8 {

void appendCodePoint(std::string& out, uint32_t cp)
{
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

namespace {

uint32_t hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return static_cast<uint32_t>(c - '0');
    if (c >= 'a' && c <= 'f')
        return static_cast<uint32_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return static_cast<uint32_t>(c - 'A' + 10);
    return 0;
}

uint32_t parseHex4(const char hex[4])
{
    return (hexValue(hex[0]) << 12) | (hexValue(hex[1]) << 8) | (hexValue(hex[2]) << 4)
           | hexValue(hex[3]);
}

} // namespace

std::string decodeJsonEscape(const char hex[4], char hex2[4], bool hasSecond)
{
    uint32_t cp = parseHex4(hex);
    if (cp >= 0xD800 && cp <= 0xDBFF && hasSecond) {
        const uint32_t low = parseHex4(hex2);
        if (low >= 0xDC00 && low <= 0xDFFF)
            cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
    }

    std::string out;
    appendCodePoint(out, cp);
    return out;
}

char sortBucket(const std::string& text)
{
    for (size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (std::isspace(c) || c == '*')
            continue;
        if (c < 0x80) {
            if (std::isdigit(c))
                return static_cast<char>(c);
            if (std::isalpha(c))
                return static_cast<char>(std::toupper(c));
            return '#';
        }
        // UTF-8 lead byte — bucket by first code unit (good enough for é etc.)
        return static_cast<char>(std::toupper(static_cast<char>(c)));
    }
    return '#';
}

namespace {

uint32_t readCodePoint(const std::string& s, size_t& i)
{
    if (i >= s.size())
        return 0;

    const unsigned char c0 = static_cast<unsigned char>(s[i]);
    if (c0 <= 0x7F) {
        ++i;
        return c0;
    }

    if ((c0 & 0xE0) == 0xC0 && i + 1 < s.size()) {
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        if ((c1 & 0xC0) == 0x80) {
            const uint32_t cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
            i += 2;
            return cp;
        }
    }

    if ((c0 & 0xF0) == 0xE0 && i + 2 < s.size()) {
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) {
            const uint32_t cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            i += 3;
            return cp;
        }
    }

    if ((c0 & 0xF8) == 0xF0 && i + 3 < s.size()) {
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
        const unsigned char c3 = static_cast<unsigned char>(s[i + 3]);
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
            const uint32_t cp =
                ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            i += 4;
            return cp;
        }
    }

    ++i;
    return c0;
}

bool isCombiningMark(uint32_t cp)
{
    return (cp >= 0x300 && cp <= 0x36F) || (cp >= 0x1AB0 && cp <= 0x1AFF)
           || (cp >= 0x1DC0 && cp <= 0x1DFF) || (cp >= 0x20D0 && cp <= 0x20FF)
           || (cp >= 0xFE20 && cp <= 0xFE2F);
}

uint32_t foldCodePoint(uint32_t cp)
{
    switch (cp) {
        case 0x00C0:
        case 0x00C1:
        case 0x00C2:
        case 0x00C3:
        case 0x00C4:
        case 0x00C5:
        case 0x00C6:
        case 0x00E0:
        case 0x00E1:
        case 0x00E2:
        case 0x00E3:
        case 0x00E4:
        case 0x00E5:
            return 'a';
        case 0x00C7:
        case 0x00E7:
            return 'c';
        case 0x00C8:
        case 0x00C9:
        case 0x00CA:
        case 0x00CB:
        case 0x00E8:
        case 0x00E9:
        case 0x00EA:
        case 0x00EB:
            return 'e';
        case 0x00CC:
        case 0x00CD:
        case 0x00CE:
        case 0x00CF:
        case 0x00EC:
        case 0x00ED:
        case 0x00EE:
        case 0x00EF:
            return 'i';
        case 0x00D1:
        case 0x00F1:
            return 'n';
        case 0x00D2:
        case 0x00D3:
        case 0x00D4:
        case 0x00D5:
        case 0x00D6:
        case 0x00F2:
        case 0x00F3:
        case 0x00F4:
        case 0x00F5:
        case 0x00F6:
            return 'o';
        case 0x00D9:
        case 0x00DA:
        case 0x00DB:
        case 0x00DC:
        case 0x00F9:
        case 0x00FA:
        case 0x00FB:
        case 0x00FC:
            return 'u';
        case 0x00DD:
        case 0x00FD:
        case 0x00FF:
            return 'y';
        default:
            break;
    }

    if (cp < 0x80)
        return static_cast<uint32_t>(std::tolower(static_cast<unsigned char>(cp)));
    return cp;
}

} // namespace

std::string foldForSearch(const std::string& text)
{
    std::string out;
    for (size_t i = 0; i < text.size();) {
        const uint32_t cp = readCodePoint(text, i);
        if (cp == 0 || isCombiningMark(cp))
            continue;
        const uint32_t folded = foldCodePoint(cp);
        if (folded < 0x80)
            out.push_back(static_cast<char>(folded));
        else
            appendCodePoint(out, folded);
    }
    return out;
}

bool containsFolded(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
        return true;

    const std::string foldedHay = foldForSearch(haystack);
    const std::string foldedNeedle = foldForSearch(needle);
    if (foldedHay.size() < foldedNeedle.size())
        return false;

    for (size_t i = 0; i + foldedNeedle.size() <= foldedHay.size(); ++i) {
        if (foldedHay.compare(i, foldedNeedle.size(), foldedNeedle) == 0)
            return true;
    }
    return false;
}

bool containsInsensitive(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
        return true;
    if (haystack.size() < needle.size())
        return false;

    auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };

    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            const unsigned char a = static_cast<unsigned char>(haystack[i + j]);
            const unsigned char b = static_cast<unsigned char>(needle[j]);
            if (a >= 0x80 || b >= 0x80) {
                if (haystack[i + j] != needle[j]) {
                    match = false;
                    break;
                }
            } else if (lower(a) != lower(b)) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

bool startsWithBucket(const std::string& text, char bucket)
{
    return sortBucket(text) == bucket;
}

bool isValidUtf8(const std::string& s)
{
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c <= 0x7F) {
            ++i;
            continue;
        }
        size_t need = 0;
        if ((c & 0xE0) == 0xC0)
            need = 1;
        else if ((c & 0xF0) == 0xE0)
            need = 2;
        else if ((c & 0xF8) == 0xF0)
            need = 3;
        else
            return false;
        if (i + need >= s.size())
            return false;
        for (size_t j = 1; j <= need; ++j) {
            const unsigned char cc = static_cast<unsigned char>(s[i + j]);
            if ((cc & 0xC0) != 0x80)
                return false;
        }
        i += need + 1;
    }
    return true;
}

std::string fromLatin1(const std::string& latin1)
{
    std::string out;
    for (unsigned char c : latin1) {
        if (c < 0x80)
            out.push_back(static_cast<char>(c));
        else
            appendCodePoint(out, static_cast<uint32_t>(c));
    }
    return out;
}

std::string ensureUtf8(const std::string& s)
{
    if (s.empty() || isValidUtf8(s))
        return s;
    return fromLatin1(s);
}

bool compareTitles(const std::string& a, const std::string& b)
{
    size_t ia = 0;
    size_t ib = 0;

    while (ia < a.size() || ib < b.size()) {
        uint32_t cpa = 0;
        uint32_t cpb = 0;

        while (ia < a.size()) {
            cpa = readCodePoint(a, ia);
            if (cpa == 0 || isCombiningMark(cpa))
                continue;
            break;
        }

        while (ib < b.size()) {
            cpb = readCodePoint(b, ib);
            if (cpb == 0 || isCombiningMark(cpb))
                continue;
            break;
        }

        if (cpa == 0 && cpb == 0)
            return false;
        if (cpa == 0)
            return true;
        if (cpb == 0)
            return false;

        const uint32_t fa = foldCodePoint(cpa);
        const uint32_t fb = foldCodePoint(cpb);
        if (fa != fb)
            return fa < fb;
    }

    return false;
}

} // namespace sf::Utf8

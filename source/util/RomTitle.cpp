#include "util/RomTitle.hpp"
#include "util/FileSystem.hpp"

#include <algorithm>
#include <cctype>

namespace sf::RomTitle {

namespace {

std::string trim(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

void removeBracketTags(std::string& s)
{
    for (;;) {
        size_t start = s.find('[');
        if (start == std::string::npos)
            break;
        size_t end = s.find(']', start);
        if (end == std::string::npos)
            break;
        s.erase(start, end - start + 1);
    }
}

bool isRegionToken(const std::string& token)
{
    static const char* regions[] = {
        "U", "USA", "US", "E", "Europe", "EU", "J", "Japan", "W", "World",
        "G", "Germany", "F", "France", "A", "Australia", "B", "Brazil",
        "S", "Spain", "I", "Italy", "K", "Korea", "En", "Eng", "Beta",
        "Proto", "Demo", "Sample", "Unl", "PD", "Hack", "Translated",
    };
    for (const char* r : regions) {
        if (token == r)
            return true;
    }
    if (token.size() >= 3 && token.compare(0, 3, "Rev") == 0)
        return true;
    if (!token.empty() && token[0] == 'V' && token.size() <= 4)
        return true;
    return false;
}

void removeRegionParens(std::string& s)
{
    for (;;) {
        size_t open = s.rfind('(');
        if (open == std::string::npos)
            break;
        size_t close = s.find(')', open);
        if (close == std::string::npos)
            break;
        std::string inner = s.substr(open + 1, close - open - 1);
        bool remove = isRegionToken(inner);
        if (!remove) {
            size_t comma = inner.find(',');
            if (comma != std::string::npos) {
                std::string first = trim(inner.substr(0, comma));
                remove = isRegionToken(first);
            }
        }
        if (!remove)
            break;
        s.erase(open, close - open + 1);
    }
}

std::string moveLeadingArticle(std::string s)
{
    if (s.size() > 5) {
        std::string tail = s.substr(s.size() - 5);
        if (tail == ", The" || tail == ", the")
            return "The " + s.substr(0, s.size() - 5);
    }
    return s;
}

std::string collapseSpaces(std::string s)
{
    std::string out;
    out.reserve(s.size());
    bool space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!space) {
                out.push_back(' ');
                space = true;
            }
        } else {
            out.push_back(c);
            space = false;
        }
    }
    return out;
}

} // namespace

std::string fromStem(const std::string& stemIn)
{
    std::string stem = stemIn;
    removeBracketTags(stem);
    removeRegionParens(stem);
    stem = moveLeadingArticle(std::move(stem));
    stem = collapseSpaces(std::move(stem));
    return trim(std::move(stem));
}

std::string fromPath(const std::string& path)
{
    return fromStem(FileSystem::stemOf(path));
}

} // namespace sf::RomTitle

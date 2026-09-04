#include "util/Json.hpp"
#include "util/Utf8.hpp"

#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>

namespace sf {
namespace {

class Parser {
public:
    explicit Parser(const std::string& t) : text_(t) {}

    Json parse()
    {
        skip();
        return parseValue();
    }

private:
    const std::string& text_;
    size_t i_ = 0;

    void skip()
    {
        while (i_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[i_])))
            ++i_;
    }

    char peek() const { return i_ < text_.size() ? text_[i_] : '\0'; }
    char getc() { return i_ < text_.size() ? text_[i_++] : '\0'; }

    Json parseValue()
    {
        skip();
        char c = peek();
        if (c == '{')
            return parseObject();
        if (c == '[')
            return parseArray();
        if (c == '"')
            return parseString();
        if (c == 't' || c == 'f')
            return parseBool();
        if (c == 'n')
            return parseNull();
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)))
            return parseNumber();
        throw std::runtime_error("JSON parse error near index " + std::to_string(i_));
    }

    Json parseObject()
    {
        getc();
        Json obj = Json::object();
        skip();
        if (peek() == '}') {
            getc();
            return obj;
        }
        while (true) {
            skip();
            std::string key = parseString().get<std::string>();
            skip();
            if (getc() != ':')
                throw std::runtime_error("Expected ':'");
            skip();
            obj[key] = parseValue();
            skip();
            char c = getc();
            if (c == '}')
                break;
            if (c != ',')
                throw std::runtime_error("Expected ',' or '}'");
        }
        return obj;
    }

    Json parseArray()
    {
        getc();
        Json arr = Json::array();
        skip();
        if (peek() == ']') {
            getc();
            return arr;
        }
        while (true) {
            arr.push_back(parseValue());
            skip();
            char c = getc();
            if (c == ']')
                break;
            if (c != ',')
                throw std::runtime_error("Expected ',' or ']'");
        }
        return arr;
    }

    Json parseString()
    {
        if (getc() != '"')
            throw std::runtime_error("Expected string");
        std::string out;
        while (i_ < text_.size()) {
            char c = getc();
            if (c == '"')
                break;
            if (c == '\\') {
                char e = getc();
                switch (e) {
                    case '"':
                    case '\\':
                    case '/':
                        out.push_back(e);
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case 'u': {
                        char hex[4];
                        for (int k = 0; k < 4; ++k)
                            hex[k] = getc();
                        char hex2[4] = {'0', '0', '0', '0'};
                        bool pair = false;
                        auto hexVal = [](char h) -> uint32_t {
                            if (h >= '0' && h <= '9')
                                return h - '0';
                            if (h >= 'a' && h <= 'f')
                                return h - 'a' + 10;
                            if (h >= 'A' && h <= 'F')
                                return h - 'A' + 10;
                            return 0;
                        };
                        const uint32_t cp = (hexVal(hex[0]) << 12) | (hexVal(hex[1]) << 8)
                                            | (hexVal(hex[2]) << 4) | hexVal(hex[3]);
                        if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 6 <= text_.size()
                            && text_[i_] == '\\' && text_[i_ + 1] == 'u') {
                            i_ += 2;
                            for (int k = 0; k < 4; ++k)
                                hex2[k] = getc();
                            pair = true;
                        }
                        out += Utf8::decodeJsonEscape(hex, hex2, pair);
                        break;
                    }
                    default:
                        out.push_back(e);
                        break;
                }
            } else {
                out.push_back(c);
            }
        }
        return Json(std::move(out));
    }

    Json parseBool()
    {
        if (text_.compare(i_, 4, "true") == 0) {
            i_ += 4;
            return Json(true);
        }
        if (text_.compare(i_, 5, "false") == 0) {
            i_ += 5;
            return Json(false);
        }
        throw std::runtime_error("Invalid bool");
    }

    Json parseNull()
    {
        if (text_.compare(i_, 4, "null") == 0) {
            i_ += 4;
            return Json(nullptr);
        }
        throw std::runtime_error("Invalid null");
    }

    Json parseNumber()
    {
        size_t start = i_;
        if (peek() == '-')
            getc();
        while (std::isdigit(static_cast<unsigned char>(peek())))
            getc();
        if (peek() == '.') {
            getc();
            while (std::isdigit(static_cast<unsigned char>(peek())))
                getc();
        }
        if (peek() == 'e' || peek() == 'E') {
            getc();
            if (peek() == '+' || peek() == '-')
                getc();
            while (std::isdigit(static_cast<unsigned char>(peek())))
                getc();
        }
        return Json(std::stod(text_.substr(start, i_ - start)));
    }
};

void dumpEscaped(std::ostringstream& out, const std::string& s)
{
    out << '"';
    for (char c : s) {
        switch (c) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                out << c;
                break;
        }
    }
    out << '"';
}

void dumpValue(const Json& j, std::ostringstream& out, int indent, int level)
{
    auto nl = [&]() {
        if (indent < 0)
            return;
        out << '\n';
        for (int i = 0; i < indent * level; ++i)
            out << ' ';
    };

    switch (j.type()) {
        case Json::Type::Null:
            out << "null";
            break;
        case Json::Type::Bool:
            out << (j.get<bool>() ? "true" : "false");
            break;
        case Json::Type::Number: {
            // Default ostringstream precision is 6 digits, which turns unix timestamps
            // into values like 1.7545e+09 and collapses every play within ~hours to one second.
            const double v = j.get<double>();
            if (std::isfinite(v) && std::floor(v) == v
                && v >= static_cast<double>(std::numeric_limits<long long>::min())
                && v <= static_cast<double>(std::numeric_limits<long long>::max())) {
                out << static_cast<long long>(v);
            } else {
                std::ostringstream num;
                num.precision(17);
                num << v;
                out << num.str();
            }
            break;
        }
        case Json::Type::String:
            dumpEscaped(out, j.get<std::string>());
            break;
        case Json::Type::Array: {
            out << '[';
            for (size_t i = 0; i < j.size(); ++i) {
                if (i)
                    out << ',';
                if (indent >= 0) {
                    nl();
                    for (int k = 0; k < indent; ++k)
                        out << ' ';
                }
                dumpValue(j[i], out, indent, level + 1);
            }
            if (j.size() && indent >= 0)
                nl();
            out << ']';
            break;
        }
        case Json::Type::Object: {
            out << '{';
            bool first = true;
            for (const auto& [k, v] : j.items()) {
                if (!first)
                    out << ',';
                first = false;
                if (indent >= 0) {
                    nl();
                    for (int x = 0; x < indent; ++x)
                        out << ' ';
                }
                dumpEscaped(out, k);
                out << (indent >= 0 ? ": " : ":");
                dumpValue(v, out, indent, level + 1);
            }
            if (!first && indent >= 0)
                nl();
            out << '}';
            break;
        }
    }
}

} // namespace

Json Json::parse(const std::string& text)
{
    return Parser(text).parse();
}

std::string Json::dump(int indent) const
{
    std::ostringstream out;
    dumpValue(*this, out, indent, 0);
    return out.str();
}

} // namespace sf

#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace sf {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : type_(Type::Bool), bool_(b) {}
    Json(int n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Json(unsigned n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Json(long n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Json(unsigned long n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Json(long long n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Json(unsigned long long n) : type_(Type::Number), number_(static_cast<double>(n)) {}
    Json(double n) : type_(Type::Number), number_(n) {}
    Json(float n) : type_(Type::Number), number_(n) {}
    Json(const char* s) : type_(Type::String), string_(s ? s : "") {}
    Json(std::string s) : type_(Type::String), string_(std::move(s)) {}

    static Json array()
    {
        Json j;
        j.type_ = Type::Array;
        return j;
    }
    static Json object()
    {
        Json j;
        j.type_ = Type::Object;
        return j;
    }

    static Json parse(const std::string& text);

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }
    bool is_string() const { return type_ == Type::String; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_boolean() const { return type_ == Type::Bool; }

    bool contains(const std::string& key) const
    {
        return type_ == Type::Object && object_.count(key) > 0;
    }

    Json& operator[](const std::string& key)
    {
        type_ = Type::Object;
        return object_[key];
    }

    const Json& operator[](const std::string& key) const
    {
        static const Json null;
        auto it = object_.find(key);
        return it == object_.end() ? null : it->second;
    }

    Json& operator[](size_t i)
    {
        type_ = Type::Array;
        if (array_.size() <= i)
            array_.resize(i + 1);
        return array_[i];
    }

    const Json& operator[](size_t i) const { return array_.at(i); }

    size_t size() const
    {
        if (type_ == Type::Array)
            return array_.size();
        if (type_ == Type::Object)
            return object_.size();
        return 0;
    }

    std::vector<Json>::iterator begin() { return array_.begin(); }
    std::vector<Json>::iterator end() { return array_.end(); }
    std::vector<Json>::const_iterator begin() const { return array_.begin(); }
    std::vector<Json>::const_iterator end() const { return array_.end(); }

    const std::map<std::string, Json>& items() const { return object_; }

    void push_back(Json v)
    {
        type_ = Type::Array;
        array_.push_back(std::move(v));
    }

    template <typename T>
    T get() const;

    template <typename T>
    T value(const std::string& key, T fallback) const
    {
        if (!contains(key))
            return fallback;
        try {
            return (*this)[key].template get<T>();
        } catch (...) {
            return fallback;
        }
    }

    std::string dump(int indent = -1) const;

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0;
    std::string string_;
    std::vector<Json> array_;
    std::map<std::string, Json> object_;
};

template <>
inline bool Json::get<bool>() const
{
    if (type_ == Type::Bool)
        return bool_;
    if (type_ == Type::Number)
        return number_ != 0;
    throw std::runtime_error("not bool");
}

template <>
inline int Json::get<int>() const
{
    if (type_ != Type::Number)
        throw std::runtime_error("not number");
    return static_cast<int>(number_);
}

template <>
inline float Json::get<float>() const
{
    if (type_ != Type::Number)
        throw std::runtime_error("not number");
    return static_cast<float>(number_);
}

template <>
inline double Json::get<double>() const
{
    if (type_ != Type::Number)
        throw std::runtime_error("not number");
    return number_;
}

template <>
inline size_t Json::get<size_t>() const
{
    if (type_ != Type::Number)
        throw std::runtime_error("not number");
    return static_cast<size_t>(number_);
}

template <>
inline std::string Json::get<std::string>() const
{
    if (type_ == Type::String)
        return string_;
    if (type_ == Type::Number)
        return std::to_string(number_);
    if (type_ == Type::Bool)
        return bool_ ? "true" : "false";
    return {};
}

} // namespace sf

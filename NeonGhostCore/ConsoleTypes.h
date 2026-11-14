#pragma once
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace ng::console {


    template<typename T>
    std::string ToString(const std::vector<T>& values)
    {
        std::ostringstream stream;

        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0)
                stream << ",";

            stream << ToString(values[i]);
        }

        return stream.str();
    }

    template<typename T, std::size_t N>
    std::string ToString(const std::array<T, N>& values)
    {
        std::ostringstream stream;

        for (std::size_t i = 0; i < N; ++i)
        {
            if (i > 0)
                stream << ",";

            stream << ToString(values[i]);
        }

        return stream.str();
    }

    template<typename T>
    bool ParseValue(const std::string& text, std::vector<T>& values)
    {
        std::vector<T> result;
        std::istringstream stream(text);

        std::string token;

        while (std::getline(stream, token, ','))
        {
            T value{};

            if (!ParseValue(token, value))
                return false;

            result.push_back(value);
        }

        values = std::move(result);
        return true;
    }

    template<typename T, std::size_t N>
    bool ParseValue(const std::string& text, std::array<T, N>& values)
    {
        std::istringstream stream(text);
        std::string token;

        std::array<T, N> result{};

        for (std::size_t i = 0; i < N; ++i)
        {
            if (!std::getline(stream, token, ','))
                return false;

            if (!ParseValue(token, result[i]))
                return false;
        }

        // Keine zusätzlichen Elemente zulassen.
        if (std::getline(stream, token, ','))
            return false;

        values = result;
        return true;
    }



template<class T> std::string ToString(const T& value) {
    std::ostringstream s; s << std::boolalpha;
    if constexpr (std::is_floating_point_v<T>) s << std::setprecision(std::numeric_limits<T>::max_digits10);
    s << value; return s.str();
}

template<class T> bool ParseValue(const std::string& text, T& value) {
    if constexpr (std::is_same_v<T, std::string>) { value = text; return true; }
    else if constexpr (std::is_same_v<T, bool>) {
        if (text == "true" || text == "1" || text == "on") { value = true; return true; }
        if (text == "false" || text == "0" || text == "off") { value = false; return true; }
        return false;
    } else {
        std::istringstream s(text); s >> std::boolalpha >> value;
        if (s.fail()) return false; s >> std::ws; return s.eof();
    }
}

inline std::vector<std::string> Tokenize(const std::string& line) {
    std::vector<std::string> out; std::string token;
    bool quoted = false, escaped = false, started = false;
    for (char ch : line) {
        if (escaped) { token += ch; escaped = false; started = true; continue; }
        if (ch == '\\') { escaped = true; started = true; continue; }
        if (ch == '"') { quoted = !quoted; started = true; continue; }
        if (std::isspace(static_cast<unsigned char>(ch)) && !quoted) {
            if (started) { out.push_back(token); token.clear(); started = false; }
            continue;
        }
        token += ch; started = true;
    }
    if (escaped) token += '\\';
    if (started) out.push_back(token);
    return out;
}
}

#include "libjson/serializer.hpp"

#include "libjson/json_value.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace libjson {
namespace {

void validate_utf8(std::string_view value) {
    int           remaining = 0;
    std::uint32_t code_point = 0;
    std::uint32_t minimum = 0;

    for (char c : value) {
        const auto byte = static_cast<unsigned char>(c);
        if (remaining != 0) {
            if ((byte & 0xC0) != 0x80) {
                throw std::invalid_argument(
                    "invalid UTF-8 continuation byte in string");
            }
            code_point = (code_point << 6) | (byte & 0x3F);
            --remaining;
            if (remaining == 0
                && (code_point < minimum || code_point > 0x10FFFF
                    || (code_point >= 0xD800 && code_point <= 0xDFFF))) {
                throw std::invalid_argument(
                    "invalid UTF-8 code point in string");
            }
            continue;
        }

        if (byte <= 0x7F) {
            continue;
        }
        if (byte >= 0xC2 && byte <= 0xDF) {
            remaining = 1;
            code_point = byte & 0x1F;
            minimum = 0x80;
        } else if (byte >= 0xE0 && byte <= 0xEF) {
            remaining = 2;
            code_point = byte & 0x0F;
            minimum = 0x800;
        } else if (byte >= 0xF0 && byte <= 0xF4) {
            remaining = 3;
            code_point = byte & 0x07;
            minimum = 0x10000;
        } else {
            throw std::invalid_argument("invalid UTF-8 leading byte in string");
        }
    }
    if (remaining != 0) {
        throw std::invalid_argument("incomplete UTF-8 sequence in string");
    }
}

void serialize_string(std::string_view value, std::string& res) {
    validate_utf8(value);
    constexpr std::string_view hex {"0123456789abcdef"};

    res.push_back('"');
    for (char c : value) {
        switch (c) {
        case '"': res += "\\\""; break;
        case '\\': res += "\\\\"; break;
        case '\b': res += "\\b"; break;
        case '\f': res += "\\f"; break;
        case '\n': res += "\\n"; break;
        case '\r': res += "\\r"; break;
        case '\t': res += "\\t"; break;
        default:
            if (const auto byte = static_cast<unsigned char>(c); byte < 0x20) {
                res += "\\u00";
                res.push_back(hex[byte >> 4]);
                res.push_back(hex[byte & 0x0F]);
            } else {
                res.push_back(c);
            }
        }
    }
    res.push_back('"');
}

void serialize_value(const json_value& json_value, std::string& res) {
    std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            res += "null";
        } else if constexpr (std::is_same_v<T, bool>) {
            res += value ? "true" : "false";
        } else if constexpr (std::is_same_v<T, std::string>) {
            serialize_string(value, res);
        } else if constexpr (std::is_same_v<T, json_value::array>) {
            res.push_back('[');

            bool first_element {true};
            for (const auto& element : value) {
                if (!first_element) {
                    res.push_back(',');
                }

                first_element = false;

                serialize_value(element, res);
            }

            res.push_back(']');
        } else if constexpr (std::is_same_v<T, json_value::object>) {
            res.push_back('{');

            bool first_member {true};
            for (const auto& [k, v] : value) {
                if (!first_member) {
                    res.push_back(',');
                }

                first_member = false;

                serialize_string(k, res);
                res.push_back(':');
                serialize_value(v, res);
            }

            res.push_back('}');
        } else if constexpr (std::is_same_v<T, std::uint64_t>
                             || std::is_same_v<T, std::int64_t>) {
            res += std::to_string(value);
        }
    }, json_value.data);
}

} // namespace

std::string serialize(const json_value& value) {
    std::string res {};
    serialize_value(value, res);
    return res;
}

} // namespace libjson

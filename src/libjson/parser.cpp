#include "libjson/parser.hpp"

#include "libjson/json_value.hpp"
#include "libjson/parse_error.hpp"

#include <cassert>
#include <cstddef>
#include <string_view>
#include <utility>
#include <variant>

namespace libjson {

namespace {

[[nodiscard]] constexpr auto error(std::string_view what,
                                   std::size_t      where) noexcept {
    return std::unexpected(parse_error {.what = what, .where = where});
}

[[nodiscard]] constexpr int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

void append_utf8(std::string& output, std::uint32_t code_point) {
    if (code_point <= 0x7F) {
        output += static_cast<char>(code_point);
    } else if (code_point <= 0x7FF) {
        output += static_cast<char>(0xC0 | (code_point >> 6));
        output += static_cast<char>(0x80 | (code_point & 0x3F));
    } else if (code_point <= 0xFFFF) {
        output += static_cast<char>(0xE0 | (code_point >> 12));
        output += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (code_point & 0x3F));
    } else {
        output += static_cast<char>(0xF0 | (code_point >> 18));
        output += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
        output += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (code_point & 0x3F));
    }
}

} // namespace

parser::parse_result<json_value> parser::parse() {
    const auto& parsed_value {parse_value()};

    if (!parsed_value) {
        return std::unexpected(parsed_value.error());
    }

    skip_whitespace();

    if (!at_end()) {
        return error("unexpected characters after JSON value", m_pos);
    }

    return parsed_value;
}

bool parser::at_end() noexcept {
    return m_pos >= m_raw.size();
}

char parser::peek() noexcept {
    assert(!at_end());
    return m_raw[m_pos];
}

char parser::advance() noexcept {
    assert(!at_end());
    return m_raw[m_pos++];
}

void parser::consume(char c) noexcept {
    assert(peek() == c);
    advance();
}

void parser::skip_whitespace() noexcept {
    while (!at_end()) {
        if (char c {peek()}; c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return;
        }

        advance();
    }
}

parser::parse_result<std::uint16_t> parser::parse_hex_code_unit() {
    std::uint16_t value = 0;

    for (int i = 0; i < 4; ++i) {
        if (at_end()) {
            return error("incomplete Unicode escape", m_pos);
        }

        const std::size_t digit_pos = m_pos;
        const int         digit = hex_value(advance());
        if (digit < 0) {
            return error("invalid hexadecimal digit in Unicode escape",
                         digit_pos);
        }

        value = static_cast<std::uint16_t>((value << 4) | digit);
    }

    return value;
}

parser::parse_result<std::uint32_t> parser::parse_unicode_escape() {
    auto first_result = parse_hex_code_unit();
    if (!first_result) {
        return std::unexpected(first_result.error());
    }

    const std::uint16_t first = *first_result;
    if (first >= 0xDC00 && first <= 0xDFFF) {
        return error("unexpected low surrogate", m_pos - 4);
    }

    if (first < 0xD800 || first > 0xDBFF) {
        return first;
    }

    // A high surrogate must be followed by a second Unicode escape.
    if (at_end() || advance() != '\\') {
        return error("high surrogate must be followed by low surrogate", m_pos);
    }
    if (at_end() || advance() != 'u') {
        return error("high surrogate must be followed by low surrogate", m_pos);
    }

    auto second_result = parse_hex_code_unit();
    if (!second_result) {
        return std::unexpected(second_result.error());
    }

    const std::uint16_t second = *second_result;
    if (second < 0xDC00 || second > 0xDFFF) {
        return error("expected low surrogate", m_pos - 4);
    }

    return 0x10000 + ((static_cast<std::uint32_t>(first) - 0xD800) << 10)
        + (static_cast<std::uint32_t>(second) - 0xDC00);
}

parser::parse_result<void> parser::parse_escape(std::string& output) {
    if (at_end()) {
        return error("incomplete escape sequence", m_pos);
    }

    switch (advance()) {
    case '"': output += '"'; break;
    case '\\': output += '\\'; break;
    case '/': output += '/'; break;
    case 'b': output += '\b'; break;
    case 'f': output += '\f'; break;
    case 'n': output += '\n'; break;
    case 'r': output += '\r'; break;
    case 't': output += '\t'; break;
    case 'u': {
        auto result = parse_unicode_escape();
        if (!result) {
            return std::unexpected(result.error());
        }
        append_utf8(output, *result);
        break;
    }
    default: return error("invalid escape sequence", m_pos - 1);
    }

    return {};
}

parser::parse_result<json_value> parser::parse_value() {
    skip_whitespace();

    switch (peek()) {
    case 'n': return parse_null();
    case 't':
    case 'f': return parse_bool();
    case '\"': {
        auto parsed_str {parse_string()};

        if (!parsed_str) {
            return std::unexpected(parsed_str.error());
        }

        return json_value {.data = std::move(*parsed_str)};
    }
    // case '[':
    //     return parse_array();
    // case '{':
    //     return parse_object();
    default:
        // if (char c {peek()}; c == '-' || c >= '0' || c <= 9) {
        //     return parse_number();
        // }

        return error("unexpected characters", m_pos);
    }
}

parser::parse_result<json_value> parser::parse_null() noexcept {
    assert(peek() == 'n');
    constexpr std::string_view token {"null"};

    for (char c : token) {
        if (at_end() || c != advance()) {
            return error("expected null", m_pos - 1);
        }
    }

    return json_value {.data = std::monostate()};
}

parser::parse_result<json_value> parser::parse_bool() noexcept {
    assert(peek() == 't' || peek() == 'f');

    bool                   is_true {peek() == 't'};
    const std::string_view token {is_true ? "true" : "false"};

    for (char c : token) {
        if (at_end() || c != advance()) {
            return error(is_true ? "expected true" : "expected false",
                         m_pos - 1);
        }
    }

    return json_value {.data = is_true};
}

parser::parse_result<std::string> parser::parse_string() {
    consume('"');

    std::string parsed_str {};

    while (!at_end()) {
        char c {advance()};

        if (static_cast<unsigned char>(c) <= 0x1F) {
            return error("unescaped control character in string", m_pos - 1);
        }

        if (c == '"') {
            return parsed_str;
        }

        if (c == '\\') {
            auto result = parse_escape(parsed_str);
            if (!result) {
                return std::unexpected(result.error());
            }
            continue;
        }

        parsed_str.push_back(c);
    }

    return error("unterminated string", m_pos);
}

} // namespace libjson

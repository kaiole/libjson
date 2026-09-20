#include "libjson/parser.hpp"

#include "libjson/json_value.hpp"
#include "libjson/parse_error.hpp"

#include <cassert>
#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>
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
    auto parsed_value {parse_value()};

    if (!parsed_value) {
        return std::unexpected(parsed_value.error());
    }

    skip_whitespace();
    if (!at_end()) {
        return error("expected end of input after JSON value", m_pos);
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
            return error(
                "incomplete Unicode escape: expected four hexadecimal digits",
                m_pos);
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
        return error("invalid Unicode escape: low surrogate without preceding "
                     "high surrogate",
                     m_pos - 4);
    }

    if (first < 0xD800 || first > 0xDBFF) {
        return first;
    }

    // A high surrogate must be followed by a second Unicode escape.
    if (at_end() || peek() != '\\') {
        return error("invalid Unicode escape: expected low surrogate escape "
                     "after high surrogate",
                     m_pos);
    }
    consume('\\');
    if (at_end() || peek() != 'u') {
        return error("invalid Unicode escape: expected low surrogate escape "
                     "after high surrogate",
                     m_pos);
    }
    advance();

    auto second_result = parse_hex_code_unit();
    if (!second_result) {
        return std::unexpected(second_result.error());
    }

    const std::uint16_t second = *second_result;
    if (second < 0xDC00 || second > 0xDFFF) {
        return error("invalid Unicode escape: expected low surrogate after "
                     "high surrogate",
                     m_pos - 4);
    }

    return 0x10000 + ((static_cast<std::uint32_t>(first) - 0xD800) << 10)
        + (static_cast<std::uint32_t>(second) - 0xDC00);
}

parser::parse_result<void> parser::parse_escape(std::string& output) {
    if (at_end()) {
        return error("incomplete string escape: expected escape character "
                     "after backslash",
                     m_pos);
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
    default:
        return error("invalid string escape: expected one of '\"', '\\', '/', "
                     "'b', 'f', 'n', 'r', 't', or 'u'",
                     m_pos - 1);
    }

    return {};
}

parser::parse_result<json_value> parser::parse_value() {
    skip_whitespace();
    if (at_end()) {
        return error("expected JSON value before end of input", m_pos);
    }

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
    case '[': return parse_array();
    case '{': return parse_object();
    default:
        if (char c {peek()}; c == '-' || (c >= '0' && c <= '9')) {
            return parse_number();
        }

        return error("expected JSON value", m_pos);
    }
}

parser::parse_result<json_value> parser::parse_null() noexcept {
    assert(peek() == 'n');
    constexpr std::string_view token {"null"};

    for (char c : token) {
        if (at_end() || peek() != c) {
            return error("invalid literal: expected 'null'", m_pos);
        }
        advance();
    }

    return json_value {.data = std::monostate()};
}

parser::parse_result<json_value> parser::parse_bool() noexcept {
    assert(peek() == 't' || peek() == 'f');

    bool                   is_true {peek() == 't'};
    const std::string_view token {is_true ? "true" : "false"};

    for (char c : token) {
        if (at_end() || peek() != c) {
            return error(is_true ? "invalid literal: expected 'true'"
                                 : "invalid literal: expected 'false'",
                         m_pos);
        }
        advance();
    }

    return json_value {.data = is_true};
}

parser::parse_result<void> parser::parse_utf8(unsigned char lead,
                                              std::string&  output) {
    const std::size_t start = m_pos - 1;
    int               remaining = 0;
    std::uint32_t     code_point = 0;
    std::uint32_t     minimum = 0;
    if (lead >= 0xC2 && lead <= 0xDF) {
        remaining = 1;
        code_point = lead & 0x1F;
        minimum = 0x80;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        remaining = 2;
        code_point = lead & 0x0F;
        minimum = 0x800;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        remaining = 3;
        code_point = lead & 0x07;
        minimum = 0x10000;
    } else {
        return error("invalid UTF-8 leading byte in string", start);
    }

    for (int i = 0; i < remaining; ++i) {
        if (at_end()) {
            return error("incomplete UTF-8 sequence in string", start);
        }
        const auto next = static_cast<unsigned char>(peek());
        if ((next & 0xC0) != 0x80) {
            return error("invalid UTF-8 continuation byte in string", m_pos);
        }
        advance();
        code_point = (code_point << 6) | (next & 0x3F);
    }
    if (code_point < minimum || code_point > 0x10FFFF
        || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
        return error("invalid UTF-8 sequence in string: overlong encoding or "
                     "invalid Unicode scalar value",
                     start);
    }
    output.append(m_raw.substr(start, m_pos - start));
    return {};
}

parser::parse_result<std::string> parser::parse_string() {
    consume('"');

    std::string parsed_str {};

    while (!at_end()) {
        char c {advance()};

        if (static_cast<unsigned char>(c) <= 0x1F) {
            return error("invalid string: control characters must be escaped",
                         m_pos - 1);
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

        const auto lead = static_cast<unsigned char>(c);
        if (lead >= 0x80) {
            auto result = parse_utf8(lead, parsed_str);
            if (!result) {
                return std::unexpected(result.error());
            }
            continue;
        }

        parsed_str.push_back(c);
    }

    return error("unterminated string: expected closing quote", m_pos);
}

parser::parse_result<json_value> parser::parse_array() {
    consume('[');
    skip_whitespace();
    if (at_end()) {
        return error("unterminated array", m_pos);
    }

    json_value::array arr {};
    if (peek() == ']') {
        consume(']');
        return json_value {.data = std::move(arr)};
    }

    while (true) {
        auto parsed_value {parse_value()};
        if (!parsed_value) {
            return std::unexpected(parsed_value.error());
        }

        arr.push_back(std::move(*parsed_value));
        skip_whitespace();
        if (at_end()) {
            return error("unterminated array", m_pos);
        }

        if (peek() == ']') {
            consume(']');
            return json_value {.data = std::move(arr)};
        }

        if (peek() != ',') {
            return error("expected ',' or ']' after array element", m_pos);
        }

        consume(',');
    }
}

parser::parse_result<json_value> parser::parse_object() {
    consume('{');
    skip_whitespace();
    if (at_end()) {
        return error("unterminated object", m_pos);
    }

    json_value::object obj {};
    if (peek() == '}') {
        consume('}');
        return json_value {.data = std::move(obj)};
    }

    while (true) {
        skip_whitespace();
        if (at_end()) {
            return error("unterminated object", m_pos);
        }

        if (peek() != '"') {
            return error("expected double-quoted object key", m_pos);
        }

        auto parsed_key {parse_string()};
        if (!parsed_key) {
            return std::unexpected(parsed_key.error());
        }

        skip_whitespace();
        if (at_end()) {
            return error("unterminated object", m_pos);
        }

        if (peek() != ':') {
            return error("expected ':' after object key", m_pos);
        }

        consume(':');
        skip_whitespace();
        if (at_end()) {
            return error("unterminated object", m_pos);
        }

        auto parsed_value {parse_value()};
        if (!parsed_value) {
            return std::unexpected(parsed_value.error());
        }

        obj.insert_or_assign(std::move(*parsed_key), std::move(*parsed_value));

        skip_whitespace();
        if (at_end()) {
            return error("unterminated object", m_pos);
        }

        if (peek() == '}') {
            consume('}');
            return json_value {.data = std::move(obj)};
        }

        if (peek() != ',') {
            return error("expected ',' or '}' after object member", m_pos);
        }

        consume(',');
    }
}

parser::parse_result<json_value> parser::parse_number() {
    const std::size_t start = m_pos;
    const bool        negative = peek() == '-';
    if (negative) {
        consume('-');
    }

    if (at_end() || peek() < '0' || peek() > '9') {
        return error("invalid integer: expected digit", m_pos);
    }

    if (const bool leading_zero {advance() == '0'};
        leading_zero && !at_end() && peek() >= '0' && peek() <= '9') {
        return error("invalid integer: leading zeros are not allowed", m_pos);
    }

    while (!at_end() && peek() >= '0' && peek() <= '9') {
        advance();
    }

    if (!at_end() && (peek() == '.' || peek() == 'e' || peek() == 'E')) {
        return error(
            "unsupported number: expected integer without fraction or exponent",
            m_pos);
    }

    const char* first = m_raw.substr(start).data();
    const char* last = m_raw.substr(m_pos).data();
    if (negative) {
        std::int64_t value = 0;
        const auto   result = std::from_chars(first, last, value);
        if (result.ec != std::errc {}) {
            return error("integer out of range for int64_t", start);
        }

        return json_value {.data = value};
    }

    std::uint64_t value = 0;
    const auto    result = std::from_chars(first, last, value);
    if (result.ec != std::errc {}) {
        return error("integer out of range for uint64_t", start);
    }

    return json_value {.data = value};
}

} // namespace libjson

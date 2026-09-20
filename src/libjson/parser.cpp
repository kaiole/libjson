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

void parser::skip_whitespace() noexcept {
    while (!at_end()) {
        if (char c {peek()}; c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return;
        }

        advance();
    }
}

parser::parse_result<json_value> parser::parse_value() {
    skip_whitespace();

    switch (peek()) {
    case 'n':
        return parse_null();
    case 't':
    case 'f':
        return parse_bool();
    // case '\"': {
    //     auto parsed_str {parse_string()};
    //
    //     if (!parsed_str) {
    //         return std::unexpected(parsed_str.error());
    //     }
    //
    //     return json_value {.data = std::move(*parsed_str)};
    // }
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

} // namespace libjson

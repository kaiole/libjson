#pragma once

#include "libjson/json_value.hpp"
#include "libjson/parse_error.hpp"

#include <cstddef>
#include <expected>
#include <string_view>

namespace libjson {

class parser {
public:
    explicit parser(std::string_view raw_json) : m_raw(raw_json) {};

    template <typename T>
    using parse_result = std::expected<T, parse_error>;

    parse_result<json_value> parse();

private:
    [[nodiscard]] bool at_end() noexcept;
    [[nodiscard]] char peek() noexcept;
    char               advance() noexcept;
    void               consume() noexcept;
    void               skip_whitespace() noexcept;

    [[nodiscard]] parse_result<json_value> parse_value();

    [[nodiscard]] parse_result<json_value>  parse_null() noexcept;
    [[nodiscard]] parse_result<json_value>  parse_bool() noexcept;
    [[nodiscard]] parse_result<std::string> parse_string();
    [[nodiscard]] parse_result<json_value>  parse_array();
    [[nodiscard]] parse_result<json_value>  parse_object();
    [[nodiscard]] parse_result<json_value>  parse_number();

    std::string_view m_raw;
    std::size_t      m_pos = 0;
};

} // namespace libjson

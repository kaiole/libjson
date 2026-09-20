#pragma once

#include "libjson/json_value.hpp"
#include "libjson/parse_error.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
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
    void               consume(char c) noexcept;
    void               skip_whitespace() noexcept;

    [[nodiscard]] parse_result<std::uint16_t> parse_hex_code_unit();
    [[nodiscard]] parse_result<std::uint32_t> parse_unicode_escape();
    [[nodiscard]] parse_result<void>          parse_escape(std::string& output);
    [[nodiscard]] parse_result<void>          parse_utf8(unsigned char lead,
                                                         std::string&  output);

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

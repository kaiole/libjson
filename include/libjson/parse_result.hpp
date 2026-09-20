#pragma once

#include "libjson/json_value.hpp"
#include "libjson/parse_error.hpp"

#include <expected>

namespace libjson {

using parse_result = std::expected<libjson::json_value, parse_error>;

} // namespace libjson

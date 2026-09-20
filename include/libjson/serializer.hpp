#pragma once

#include "libjson/json_value.hpp"

namespace libjson {

std::string serialize(const json_value& json_value);

} // namespace libjson

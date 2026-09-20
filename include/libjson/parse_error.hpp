#pragma once

#include <cstddef>
#include <string_view>

namespace libjson {

struct parse_error {
    std::string_view what;
    std::size_t      where;
};

} // namespace libjson

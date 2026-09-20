#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace libjson {

struct json_value {
    using array = std::vector<json_value>;
    using object = std::unordered_map<std::string, json_value>;
    using value = std::variant<std::monostate,
                               bool,
                               std::string,
                               array,
                               object,
                               std::uint64_t,
                               std::int64_t>;
    value data;
};

} // namespace libjson

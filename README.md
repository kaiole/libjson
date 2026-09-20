# libjson

A C++23 JSON parser and serializer.

Supports null, booleans, strings, arrays, objects, and 64-bit integers. Strings
support JSON escapes and UTF-8 validation.

Limitations:

- No fractional or exponent-form numbers.
- Integers must fit in `int64_t` (negative) or `uint64_t` (nonnegative).
- Object key order is not preserved. Duplicate keys keep the last value.

## Build and test

Requires CMake 3.20+ and a C++23 compiler with `std::expected` support.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

Link against the `libjson` CMake target.

```cpp
#include <libjson/parser.hpp>
#include <libjson/serializer.hpp>

#include <iostream>

int main() {
    auto result = libjson::parser(R"({"name":"hello","count":42})").parse();
    if (!result) {
        std::cerr << result.error().what << " at byte " << result.error().where << '\n';
        return 1;
    }

    std::cout << libjson::serialize(*result) << '\n';
}
```

`json_value::data` is a `std::variant`. Arrays use `std::vector`; objects use
`std::unordered_map`. Serialization throws `std::invalid_argument` for invalid
UTF-8 strings.

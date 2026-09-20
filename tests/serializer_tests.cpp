#include "libjson/parser.hpp"
#include "libjson/serializer.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {

using libjson::json_value;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void expect(const json_value& value, std::string_view expected) {
    require(libjson::serialize(value) == expected,
            "incorrect serialized output");
}

json_value round_trip(const json_value& value) {
    const auto output = libjson::serialize(value);
    auto       parsed = libjson::parser(output).parse();
    require(parsed.has_value(), "serialized output rejected by parser");
    return std::move(*parsed);
}

void literals() {
    expect(json_value {.data = std::monostate {}}, "null");
    expect(json_value {.data = true}, "true");
    expect(json_value {.data = false}, "false");
}

void integers() {
    expect(json_value {.data = std::uint64_t {0}}, "0");
    expect(json_value {.data = std::int64_t {0}}, "0");
    expect(json_value {.data = std::uint64_t {42}}, "42");
    expect(json_value {.data = std::int64_t {42}}, "42");
    expect(json_value {.data = std::int64_t {-42}}, "-42");
    expect(json_value {.data = std::numeric_limits<std::int64_t>::min()},
           "-9223372036854775808");
    expect(json_value {.data = std::numeric_limits<std::int64_t>::max()},
           "9223372036854775807");
    expect(json_value {.data = std::numeric_limits<std::uint64_t>::max()},
           "18446744073709551615");
}

void strings() {
    expect(json_value {.data = std::string {}}, R"("")");
    expect(json_value {.data = std::string {"hello world"}},
           R"("hello world")");
    expect(json_value {.data = std::string {"\"\\/\b\f\n\r\t"}},
           R"("\"\\/\b\f\n\r\t")");
    expect(json_value {.data = std::string("a\0b", 3)}, R"("a\u0000b")");
    // A literal backslash followed by 'n' must not become a newline escape.
    expect(json_value {.data = std::string {"\\n"}}, R"("\\n")");
    expect(json_value {.data = std::string {"[]{}:,"}}, R"("[]{}:,")");
    expect(json_value {.data = std::string {"\x7f"}}, "\"\x7f\"");
}

void controls() {
    constexpr std::string_view escapes[] {
        "\\u0000", "\\u0001", "\\u0002", "\\u0003", "\\u0004", "\\u0005",
        "\\u0006", "\\u0007", "\\b",     "\\t",     "\\n",     "\\u000b",
        "\\f",     "\\r",     "\\u000e", "\\u000f", "\\u0010", "\\u0011",
        "\\u0012", "\\u0013", "\\u0014", "\\u0015", "\\u0016", "\\u0017",
        "\\u0018", "\\u0019", "\\u001a", "\\u001b", "\\u001c", "\\u001d",
        "\\u001e", "\\u001f"
    };
    for (std::size_t byte = 0; byte < 32; ++byte) {
        const json_value value {
            .data = std::string(1, static_cast<char>(byte))
        };
        expect(value, "\"" + std::string(escapes[byte]) + "\"");
        require(std::get<std::string>(round_trip(value).data)
                    == std::get<std::string>(value.data),
                "control byte round trip failed");
    }
}

void utf8() {
    for (auto bytes :
         {"\xc2\x80",
          "\xdf\xbf",
          "\xe0\xa0\x80",
          "\xed\x9f\xbf",
          "\xee\x80\x80",
          "\xef\xbf\xbf",
          "\xf0\x90\x80\x80",
          "\xf0\x9f\x98\x80",
          "\xf4\x8f\xbf\xbf"}) {
        const std::string text = std::string("prefix ") + bytes + " suffix";
        const json_value  value {.data = text};
        expect(value, "\"" + text + "\"");
        require(std::get<std::string>(round_trip(value).data) == text,
                "UTF-8 round trip failed");
    }
}

void expect_invalid_utf8(const json_value& value) {
    try {
        static_cast<void>(libjson::serialize(value));
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid UTF-8 was accepted");
}

void invalid_utf8() {
    for (auto bytes :
         {"\x80",
          "\xbf",
          "\xc0\x80",
          "\xc1\xbf",
          "\xc2",
          "\xc2\x20",
          "\xe0\x80\x80",
          "\xe0\x9f\xbf",
          "\xed\xa0\x80",
          "\xed\xbf\xbf",
          "\xe2\x82",
          "\xf0\x80\x80\x80",
          "\xf0\x8f\xbf\xbf",
          "\xf4\x90\x80\x80",
          "\xf5\x80\x80\x80",
          "\xff"}) {
        const json_value invalid {.data = std::string(bytes)};
        expect_invalid_utf8(invalid);
        expect_invalid_utf8(json_value {.data = json_value::array {invalid}});
        expect_invalid_utf8(
            json_value {.data = json_value::object {{"key", invalid}}});
        expect_invalid_utf8(
            json_value {.data = json_value::object {{bytes, json_value {}}}});
    }
    expect_invalid_utf8(json_value {.data = std::string("\xc2\0", 2)});
}

void arrays() {
    expect(json_value {.data = json_value::array {}}, "[]");
    expect(json_value {.data = json_value::array {json_value {.data = true}}},
           "[true]");
    const json_value value {
        .data = json_value::array {
            json_value {},
            json_value {.data = true},
            json_value {.data = false},
            json_value {.data = std::string {"text\n"}},
            json_value {.data = std::int64_t {-42}},
            json_value {.data = std::uint64_t {42}},
            json_value {.data = json_value::array {json_value {.data = true}}},
            json_value {.data = json_value::array {}},
            json_value {.data = json_value::object {}}
        }
    };
    expect(value, R"([null,true,false,"text\n",-42,42,[true],[],{}])");
    require(std::get<json_value::array>(value.data).size() == 9,
            "input array modified");
    // Repeated calls must not retain any prior output.
    expect(value, R"([null,true,false,"text\n",-42,42,[true],[],{}])");
}

void objects() {
    expect(json_value {.data = json_value::object {}}, "{}");
    expect(json_value {.data = json_value::object {{"", json_value {}}}},
           R"({"":null})");
    expect(
        json_value {
            .data = json_value::object {{"\"\\\n", json_value {.data = true}}}
        },
        R"({"\"\\\n":true})");
    expect(
        json_value {
            .data = json_value::object {{std::string("a\0b", 3), json_value {}}}
        },
        R"({"a\u0000b":null})");
    expect(
        json_value {.data = json_value::object {{"\xc3\xa9", json_value {}}}},
        "{\"\xc3\xa9\":null}");

    const json_value value {
        .data = json_value::object {
            {"a", json_value {.data = true}},
            {"b", json_value {.data = false}}
        }
    };
    const auto output = libjson::serialize(value);
    require(output == R"({"a":true,"b":false})"
                || output == R"({"b":false,"a":true})",
            "incorrect object members or separators");
    const json_value nested {
        .data = json_value::object {
            {"outer",
             json_value {
                 .data = json_value::array {json_value {
                     .data = json_value::object {
                         {"inner", json_value {.data = true}}
                     }
                 }}
             }}
        }
    };
    expect(nested, R"({"outer":[{"inner":true}]})");
}

void round_trips() {
    const json_value original {
        .data = json_value::object {
            {"values",
             json_value {
                 .data = json_value::array {
                     json_value {},
                     json_value {.data = true},
                     json_value {
                         .data = std::numeric_limits<std::int64_t>::min()
                     },
                     json_value {
                         .data = std::numeric_limits<std::uint64_t>::max()
                     },
                     json_value {.data = std::string("x\0\n", 3)}
                 }
             }}
        }
    };
    const auto  parsed = round_trip(original);
    const auto& object = std::get<json_value::object>(parsed.data);
    require(object.size() == 1, "incorrect round-trip object size");
    const auto& array = std::get<json_value::array>(object.at("values").data);
    require(array.size() == 5, "incorrect round-trip array size");
    require(std::holds_alternative<std::monostate>(array.at(0).data),
            "null changed");
    require(std::get<bool>(array.at(1).data), "boolean changed");
    require(std::get<std::int64_t>(array.at(2).data)
                == std::numeric_limits<std::int64_t>::min(),
            "signed integer changed");
    require(std::get<std::uint64_t>(array.at(3).data)
                == std::numeric_limits<std::uint64_t>::max(),
            "unsigned integer changed");
    require(std::get<std::string>(array.at(4).data) == std::string("x\0\n", 3),
            "string changed");
    // JSON does not preserve the signed variant alternative for nonnegative
    // integers.
    require(std::get<std::uint64_t>(
                round_trip(json_value {.data = std::int64_t {42}}).data)
                == 42,
            "nonnegative signed integer changed numerically");
}

} // namespace

int main(int argc, char* argv[]) {
    const std::pair<std::string_view, void (*)()> tests[] {
        {"literals", literals},
        {"integers", integers},
        {"strings", strings},
        {"controls", controls},
        {"utf8", utf8},
        {"invalid_utf8", invalid_utf8},
        {"arrays", arrays},
        {"objects", objects},
        {"round_trips", round_trips}
    };
    bool found = false;
    int  failures = 0;
    for (const auto& [name, test] : tests) {
        if (argc > 1 && name != argv[1]) {
            continue;
        }
        found = true;
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& exception) {
            ++failures;
            std::cerr << "FAIL " << name << ": " << exception.what() << '\n';
        }
    }
    return found && failures == 0 ? 0 : 1;
}

#include "libjson/parser.hpp"

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

json_value parse(std::string_view input) {
    auto result = libjson::parser(input).parse();
    if (!result) {
        throw std::runtime_error("unexpected rejection of " + std::string(input)
                                 + " at " + std::to_string(result.error().where)
                                 + ": " + std::string(result.error().what));
    }
    return std::move(*result);
}

template <typename T>
void expect_value(std::string_view input, const T& expected) {
    auto result = parse(input);
    const auto* value = std::get_if<T>(&result.data);
    require(value != nullptr, "incorrect value type");
    require(*value == expected, "incorrect parsed value");
}

void reject(std::string_view input) {
    auto result = libjson::parser(input).parse();
    require(!result, "unexpected acceptance of " + std::string(input));
    require(!result.error().what.empty(), "missing error message");
    require(result.error().where <= input.size(), "error offset outside input");
}

void literals() {
    expect_value("null", std::monostate {});
    expect_value("true", true);
    expect_value("false", false);
    expect_value(" \t\r\ntrue \t\r\n", true);
    for (auto input : {"", " ", "\t\r\n", "n", "nu", "nul", "Null", "NULL",
                       "t", "tru", "True", "f", "fals", "FALSE", "nulx",
                       "trux", "falsx", "undefined", "true false", "nullx",
                       "true,", "false]", "\vtrue", "true\f"}) {
        reject(input);
    }
    reject(std::string("true\0", 5));
}

void integers() {
    expect_value("0", std::uint64_t {0});
    expect_value("9", std::uint64_t {9});
    expect_value("-0", std::int64_t {0});
    expect_value("-9", std::int64_t {-9});
    expect_value("10", std::uint64_t {10});
    expect_value("1234567890", std::uint64_t {1234567890});
    expect_value("-42", std::int64_t {-42});
    expect_value("9223372036854775807", std::uint64_t {9223372036854775807ULL});
    expect_value("9223372036854775808", std::uint64_t {9223372036854775808ULL});
    expect_value("18446744073709551615", std::numeric_limits<std::uint64_t>::max());
    expect_value("-9223372036854775808", std::numeric_limits<std::int64_t>::min());
    expect_value(" \n42\t", std::uint64_t {42});
}

void invalid_integers() {
    for (auto input : {"-", "- ", "--1", "+1", "01", "00", "-01", "-00",
                       "0x10", "1a", "1 2", "1.0", "0.1", "-0.1", "1.",
                       ".1", "1e2", "1E+2", "1e-2", "1e", "NaN", "Infinity",
                       "18446744073709551616", "-9223372036854775809",
                       "999999999999999999999999999999999999999999999999"}) {
        reject(input);
    }
}

void strings() {
    expect_value(R"("")", std::string {});
    expect_value(R"("hello world")", std::string {"hello world"});
    expect_value(R"("\"\\\/\b\f\n\r\t")", std::string {"\"\\/\b\f\n\r\t"});
    expect_value(R"("a\u0000b")", std::string("a\0b", 3));
    expect_value(R"("\u007f\u0080\u07ff\u0800\uffff")",
                 std::string {"\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xef\xbf\xbf"});
    expect_value(R"("\u0041\u00e9\u20AC")", std::string {"A\xc3\xa9\xe2\x82\xac"});
    expect_value(R"("\uD800\uDC00")", std::string {"\xf0\x90\x80\x80"});
    expect_value(R"("\ud83d\ude00")", std::string {"\xf0\x9f\x98\x80"});
    expect_value(R"("\uDBFF\uDFFF")", std::string {"\xf4\x8f\xbf\xbf"});
}

void invalid_strings() {
    for (auto input : {"\"", "\"abc", "\"abc\\", R"("\x")", R"("\v")",
                       R"("\u")", R"("\u123")", R"("\u12x4")", R"("\uDC00")",
                       R"("\uDFFF")", R"("\uD800")", R"("\uD800x")",
                       R"("\uD800\n")", R"("\uD800\u0041")", R"("\uD800\uD800")",
                       R"("\uD800\uDC0")", "\"\\u12", "\"\\uD800\\", "'abc'"}) {
        reject(input);
    }
    for (int byte = 0; byte <= 0x1F; ++byte) {
        reject(std::string("\"") + static_cast<char>(byte) + "\"");
    }
}

void utf8() {
    for (auto bytes : {"\xc2\x80", "\xdf\xbf", "\xe0\xa0\x80", "\xed\x9f\xbf",
                       "\xee\x80\x80", "\xef\xbf\xbf", "\xf0\x90\x80\x80",
                       "\xf4\x8f\xbf\xbf"}) {
        expect_value(std::string("\"") + bytes + "\"", std::string(bytes));
    }
    for (auto bytes : {"\x80", "\xbf", "\xc0\x80", "\xc1\xbf", "\xc2",
                       "\xc2\x20", "\xe0\x80\x80", "\xe0\x9f\xbf",
                       "\xed\xa0\x80", "\xed\xbf\xbf", "\xe2\x82",
                       "\xf0\x80\x80\x80", "\xf0\x8f\xbf\xbf",
                       "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xff"}) {
        reject(std::string("\"") + bytes + "\"");
        reject(std::string("\"") + bytes);
    }
}

void containers() {
    require(std::get<json_value::array>(parse("[ \n ]").data).empty(), "nonempty array");
    require(std::get<json_value::object>(parse("{ \t }").data).empty(), "nonempty object");
    auto value = parse(R"([null,true,false,"text",0,-1,[],{}])");
    const auto& array = std::get<json_value::array>(value.data);
    require(array.size() == 8, "incorrect array size");
    require(std::holds_alternative<std::monostate>(array.at(0).data), "incorrect null element");
    require(std::get<bool>(array.at(1).data), "incorrect true element");
    require(!std::get<bool>(array.at(2).data), "incorrect false element");
    require(std::get<std::string>(array.at(3).data) == "text", "incorrect string element");
    require(std::get<std::uint64_t>(array.at(4).data) == 0, "incorrect unsigned element");
    require(std::get<std::int64_t>(array.at(5).data) == -1, "incorrect signed element");
    require(std::get<json_value::array>(array.at(6).data).empty(), "incorrect nested array");
    require(std::get<json_value::object>(array.at(7).data).empty(), "incorrect nested object");

    value = parse(R"( { "a" : [true, {"b": "value"}], "": null, "\u0063": false } )");
    const auto& object = std::get<json_value::object>(value.data);
    require(object.size() == 3, "object members lost");
    const auto& nested = std::get<json_value::array>(object.at("a").data);
    require(nested.size() == 2, "incorrect nested array size");
    require(std::get<bool>(nested.at(0).data), "incorrect nested boolean");
    const auto& inner = std::get<json_value::object>(nested.at(1).data);
    require(std::get<std::string>(inner.at("b").data) == "value", "incorrect nested member");
    require(std::holds_alternative<std::monostate>(object.at("").data), "empty key lost");
    require(!std::get<bool>(object.at("c").data), "escaped key not decoded");

    value = parse(R"({"a":true,"\u0061":false})");
    const auto& duplicates = std::get<json_value::object>(value.data);
    require(duplicates.size() == 1 && !std::get<bool>(duplicates.at("a").data),
            "duplicate keys must retain last value");
}

void invalid_containers() {
    for (auto input : {"[", "[ ", "[true", "[true,", "[true, ", "[true,]",
                       "[,true]", "[true false]", "[true,,false]", "[}",
                       "{", "{ ", "{\"a\"", "{\"a\":", "{\"a\": ",
                       "{\"a\":true", "{\"a\":true,", R"({"a":true,})",
                       R"({a:true})", R"({"a" true})", R"({"a":})",
                       R"({"a":true "b":false})", R"({"a":true,,"b":false})",
                       "{]", "[]{}", "{}null", "[1.5]", R"({"a":1e2})"}) {
        reject(input);
    }
}

void error_offsets() {
    const std::pair<std::string_view, std::size_t> cases[] {
        {"", 0}, {"  ", 2}, {"nul", 3}, {"nux", 2}, {"true x", 5},
        {"[true false]", 6}, {R"({"a" true})", 5}, {"01", 1}, {"-", 1},
        {R"("\x")", 2}, {R"("\u12x4")", 5}, {"\"abc", 4}
    };
    for (const auto& [input, offset] : cases) {
        auto result = libjson::parser(input).parse();
        require(!result, "expected parse failure");
        require(result.error().where == offset, "incorrect error offset for " + std::string(input));
    }
}

} // namespace

int main(int argc, char* argv[]) {
    const std::pair<std::string_view, void (*)()> tests[] {
        {"literals", literals}, {"integers", integers},
        {"invalid_integers", invalid_integers}, {"strings", strings},
        {"invalid_strings", invalid_strings}, {"utf8", utf8},
        {"containers", containers}, {"invalid_containers", invalid_containers},
        {"error_offsets", error_offsets}
    };
    bool found = false;
    int failures = 0;
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

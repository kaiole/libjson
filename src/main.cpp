#include "libjson/parser.hpp"

#include <fstream>
#include <print>
#include <string>

int main() {
    std::ifstream file("./test.json");
    std::string   json {};
    std::string   line {};

    while (std::getline(file, line)) {
        json += line;
    }

    std::println("JSON file: \n{}\n", json);

    libjson::parser parser {json};

    const auto& parsed_value {parser.parse()};

    if (!parsed_value) {
        std::println("failed: {} at {}",
                     parsed_value.error().what,
                     parsed_value.error().where);
    }

    return 0;
}

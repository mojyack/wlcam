#pragma once
#include <iostream>
#include <print>

#include "util/charconv.hpp"

namespace stdio {
inline auto read_line(const std::optional<std::string_view> prompt = std::nullopt) -> std::string {
    if(prompt) {
        std::print("{}", *prompt);
        std::flush(std::cout);
    }
    auto line = std::string();
    std::getline(std::cin, line);
    return line;
}

template <class T>
inline auto read_stdin(const std::optional<std::string_view> prompt = std::nullopt) -> T {
    while(true) {
        const auto line = read_line(prompt);
        if(const auto o = from_chars<T>(line)) {
            return o.value();
        } else {
            std::println("invalid input");
            continue;
        }
    }
}

} // namespace stdio


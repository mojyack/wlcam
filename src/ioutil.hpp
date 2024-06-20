#pragma once
#include "util/charconv.hpp"
#include "util/print.hpp"

namespace stdio {
inline auto read_line(const std::optional<std::string_view> prompt = std::nullopt) -> std::string {
    if(prompt) {
        std::cout << *prompt;
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
            print("invalid input");
            continue;
        }
    }
}

} // namespace stdio


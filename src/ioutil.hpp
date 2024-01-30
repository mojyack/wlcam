#pragma once
#include <charconv>
#include <iostream>
#include <optional>

namespace stdio {
template <class... Args>
auto print(Args... args) -> void {
    (std::cout << ... << args);
}

template <class... Args>
auto println(Args... args) -> void {
    (std::cout << ... << args) << std::endl;
}

inline auto read_line(const std::optional<std::string_view> prompt = std::nullopt) -> std::string {
    if(prompt) {
        print(*prompt);
    }
    auto line = std::string();
    std::getline(std::cin, line);
    return line;
}

template <class T>
constexpr auto false_v = false;

template <class T>
auto from_chars(const std::string_view str) -> std::optional<T> {
    // libc++15 has not support to std::from_chars<double>
    if constexpr(std::is_same_v<T, double>) {
        try {
            return std::stod(std::string(str));
        } catch(const std::invalid_argument&) {
            return std::nullopt;
        }
    } else {
        auto r = T();
        if(auto [ptr, ec] = std::from_chars(std::begin(str), std::end(str), r); ec == std::errc{}) {
            return r;
        } else {
            return std::nullopt;
        }
    }
}

template <class T>
inline auto read_stdin(const std::optional<std::string_view> prompt = std::nullopt) -> T {
    while(true) {
        const auto line = read_line(prompt);
        if(const auto o = from_chars<T>(line)) {
            return o.value();
        } else {
            println("invalid input");
            continue;
        }
    }
}

} // namespace stdio


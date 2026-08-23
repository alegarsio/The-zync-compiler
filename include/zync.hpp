#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <tuple>
#include <utility>

#define Package(name) namespace name
#define fn inline auto

template <typename... Args>
inline void print(Args&&... args) {
    (std::cout << ... << std::forward<Args>(args));
}

template <typename... Args>
inline void println(Args&&... args) {
    (std::cout << ... << std::forward<Args>(args)) << std::endl;
}
#pragma once

#include "fmt_config.hpp"
#ifdef DPB_HAS_FMT
#include <fmt/format.h>
#include <fmt/ostream.h>
#else
#include <iostream>
#include <format>
#endif

namespace dpb {

template<typename... Args>
void print(Args&&... args) {
#ifdef DPB_HAS_FMT
    fmt::print(std::forward<Args>(args)...);
#else
    if constexpr (sizeof...(Args) == 0) {
        return;
    } else {
        ((std::cout << std::forward<Args>(args)), ...);
    }
#endif
}

template<typename Fmt, typename... Args>
auto format(Fmt&& fmt_str, Args&&... args) {
#ifdef DPB_HAS_FMT
    return fmt::format(std::forward<Fmt>(fmt_str), std::forward<Args>(args)...);
#else
    return std::vformat(fmt_str, std::make_format_args(args...));
#endif
}

}

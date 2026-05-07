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

inline void print(std::string_view fmt_str) {
#ifdef DPB_HAS_FMT
    fmt::vprint(fmt_str, {});
#else
    std::cout << fmt_str;
#endif
}

template<typename Arg, typename... Args>
void print(std::string_view fmt_str, Arg&& arg, Args&&... args) {
#ifdef DPB_HAS_FMT
    fmt::vprint(fmt_str, fmt::make_format_args(arg, args...));
#else
    std::cout << std::vformat(fmt_str, std::make_format_args(arg, args...));
#endif
}

inline auto format(std::string_view fmt_str, auto&&... args) {
#ifdef DPB_HAS_FMT
    return fmt::vformat(fmt_str, fmt::make_format_args(args...));
#else
    return std::vformat(fmt_str, std::make_format_args(args...));
#endif
}

} // namespace dpb

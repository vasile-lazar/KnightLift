#include <iostream>
#include <string_view>

#include "types.hpp"

namespace {
    constexpr std::string_view kEngineName = "KnightLift";
    constexpr std::string_view kVersion = "0.1.0";
}  // namespace

int main() {
    using namespace KnightLift;

    std::cout << kEngineName << ' ' << kVersion << '\n';
    std::cout << std::hex << square_bb(Square::E4) << '\n';
    return 0;
}
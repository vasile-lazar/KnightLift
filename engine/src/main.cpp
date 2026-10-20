#include <iostream>
#include <string_view>

namespace {
    constexpr std::string_view kEngineName = "KnightLift";
    constexpr std::string_view kVersion = "0.1.0";
}  // namespace

int main() {
    std::cout << kEngineName << ' ' << kVersion << '\n';
    return 0;
}
#include <iostream>
#include <string_view>

#include "bitboard.hpp"

namespace {
    constexpr std::string_view kEngineName = "KnightLift";
    constexpr std::string_view kVersion = "0.1.0";
}  // namespace

int main() {
    using namespace KnightLift;

    std::cout << kEngineName << ' ' << kVersion << '\n';
    std::cout << bitboard_to_string(square_bb(Square::E4)) << '\n';

    Bitboard cross = file_bb(File::D) | rank_bb(Rank::R4);
    std::cout << bitboard_to_string(cross);
    std::cout << "popcount: " << popcount(cross) << '\n';

    std::cout << "squares:";
    while (cross) {
        std::cout << ' ' << to_index(pop_lsb(cross));
    }
    std::cout << '\n';
    return 0;
}
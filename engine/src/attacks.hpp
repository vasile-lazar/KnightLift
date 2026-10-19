#pragma once

#include <array>
#include <cstddef>

#include "bitboard.hpp"
#include "types.hpp"

namespace KnightLift {

namespace detail {

struct Offset {
    int file;
    int rank;
};

template <std::size_t N>
constexpr Bitboard leaper_attacks(Square s, const std::array<Offset, N>& offsets) {
    const int file = static_cast<int>(to_index(file_of(s)));
    const int rank = static_cast<int>(to_index(rank_of(s)));

    Bitboard result = 0;
    for (const Offset& o : offsets) {
        const int f = file + o.file;
        const int r = rank + o.rank;
        if (f >= 0 && f < 8 && r >= 0 && r < 8) {
            result |= square_bb(make_square(static_cast<File>(f), static_cast<Rank>(r)));
        }
    }
    return result;
}

template <std::size_t N>
constexpr std::array<Bitboard, kNumSquares> make_leaper_table(
    const std::array<Offset, N>& offsets) {
    std::array<Bitboard, kNumSquares> table{};
    for (std::size_t i = 0; i < kNumSquares; ++i) {
        table[i] = leaper_attacks(static_cast<Square>(i), offsets);
    }
    return table;
}

constexpr std::array<Offset, 8> kKnightOffsets = {{
    {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}
}};

constexpr std::array<Offset, 8> kKingOffsets = {{
    {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}
}};

constexpr std::array<Offset, 2> kWhitePawnOffsets = {{{-1, 1}, {1, 1}}};
constexpr std::array<Offset, 2> kBlackPawnOffsets = {{{-1, -1}, {1, -1}}};

constexpr int total_attacks(const std::array<Bitboard, kNumSquares>& table) {
    int total = 0;
    for (const Bitboard bb : table) {
        total += popcount(bb);
    }
    return total;
}

}  // namespace detail

inline constexpr std::array<Bitboard, kNumSquares> kKnightAttacks =
    detail::make_leaper_table(detail::kKnightOffsets);

inline constexpr std::array<Bitboard, kNumSquares> kKingAttacks =
    detail::make_leaper_table(detail::kKingOffsets);

inline constexpr std::array<std::array<Bitboard, kNumSquares>, kNumColors> kPawnAttacks = {{
    detail::make_leaper_table(detail::kWhitePawnOffsets),
    detail::make_leaper_table(detail::kBlackPawnOffsets),
}};

constexpr Bitboard knight_attacks(Square s) {
    return kKnightAttacks[to_index(s)];
}

constexpr Bitboard king_attacks(Square s) {
    return kKingAttacks[to_index(s)];
}

// Squares attacked by a pawn of color c standing on s.
constexpr Bitboard pawn_attacks(Color c, Square s) {
    return kPawnAttacks[to_index(c)][to_index(s)];
}

// ---- Compile-time sanity checks -------------------------------------------

static_assert(knight_attacks(Square::A1) == (square_bb(Square::B3) | square_bb(Square::C2)));
static_assert(popcount(knight_attacks(Square::E4)) == 8);
static_assert(popcount(knight_attacks(Square::H8)) == 2);

static_assert(popcount(king_attacks(Square::A1)) == 3);
static_assert(popcount(king_attacks(Square::E1)) == 5);
static_assert(popcount(king_attacks(Square::E4)) == 8);

static_assert(pawn_attacks(Color::White, Square::E4) ==
              (square_bb(Square::D5) | square_bb(Square::F5)));
static_assert(pawn_attacks(Color::White, Square::A2) == square_bb(Square::B3));
static_assert(pawn_attacks(Color::Black, Square::H7) == square_bb(Square::G6));
static_assert(pawn_attacks(Color::White, Square::E8) == 0);

// Totals over all 64 squares: knight 336, king 420, pawn 98 per color.
static_assert(detail::total_attacks(kKnightAttacks) == 336);
static_assert(detail::total_attacks(kKingAttacks) == 420);
static_assert(detail::total_attacks(kPawnAttacks[0]) == 98);
static_assert(detail::total_attacks(kPawnAttacks[1]) == 98);

}  // namespace KnightLift
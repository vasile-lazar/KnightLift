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

        template<std::size_t N>
        constexpr Bitboard leaper_attacks(Square s, const std::array<Offset, N> &offsets) {
            const int file = static_cast<int>(to_index(file_of(s)));
            const int rank = static_cast<int>(to_index(rank_of(s)));

            Bitboard result = 0;
            for (const Offset &o: offsets) {
                const int f = file + o.file;
                const int r = rank + o.rank;
                if (f >= 0 && f < 8 && r >= 0 && r < 8) {
                    result |= square_bb(make_square(static_cast<File>(f), static_cast<Rank>(r)));
                }
            }
            return result;
        }

        template<std::size_t N>
        constexpr std::array<Bitboard, kNumSquares> make_leaper_table(
            const std::array<Offset, N> &offsets) {
            std::array<Bitboard, kNumSquares> table{};
            for (std::size_t i = 0; i < kNumSquares; ++i) {
                table[i] = leaper_attacks(static_cast<Square>(i), offsets);
            }
            return table;
        }

        constexpr std::array<Offset, 8> kKnightOffsets = {
            {
                {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}
            }
        };

        constexpr std::array<Offset, 8> kKingOffsets = {
            {
                {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}
            }
        };

        constexpr std::array<Offset, 2> kWhitePawnOffsets = {{{-1, 1}, {1, 1}}};
        constexpr std::array<Offset, 2> kBlackPawnOffsets = {{{-1, -1}, {1, -1}}};

        constexpr int total_attacks(const std::array<Bitboard, kNumSquares> &table) {
            int total = 0;
            for (const Bitboard bb: table) {
                total += popcount(bb);
            }
            return total;
        }
    } // namespace detail

    inline constexpr std::array<Bitboard, kNumSquares> kKnightAttacks =
            detail::make_leaper_table(detail::kKnightOffsets);

    inline constexpr std::array<Bitboard, kNumSquares> kKingAttacks =
            detail::make_leaper_table(detail::kKingOffsets);

    inline constexpr std::array<std::array<Bitboard, kNumSquares>, kNumColors> kPawnAttacks = {
        {
            detail::make_leaper_table(detail::kWhitePawnOffsets),
            detail::make_leaper_table(detail::kBlackPawnOffsets),
        }
    };

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

    // ---- Sliding pieces: reference implementation -----------------------------

    namespace detail {
        constexpr std::array<Offset, 4> kBishopDirections = {{{1, 1}, {1, -1}, {-1, -1}, {-1, 1}}};
        constexpr std::array<Offset, 4> kRookDirections = {{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};

        constexpr Bitboard sliding_attacks(Square s, Bitboard occupied,
                                           const std::array<Offset, 4> &directions) {
            const int file = static_cast<int>(to_index(file_of(s)));
            const int rank = static_cast<int>(to_index(rank_of(s)));

            Bitboard result = 0;
            for (const Offset &d: directions) {
                int f = file + d.file;
                int r = rank + d.rank;
                while (f >= 0 && f < 8 && r >= 0 && r < 8) {
                    const Bitboard bb =
                            square_bb(make_square(static_cast<File>(f), static_cast<Rank>(r)));
                    result |= bb;
                    if (occupied & bb) break;
                    f += d.file;
                    r += d.rank;
                }
            }
            return result;
        }

        constexpr int total_empty_board_attacks(Bitboard (*fn)(Square, Bitboard)) {
            int total = 0;
            for (std::size_t i = 0; i < kNumSquares; ++i) {
                total += popcount(fn(static_cast<Square>(i), 0));
            }
            return total;
        }
    } // namespace detail

    constexpr Bitboard bishop_attacks_reference(Square s, Bitboard occupied) {
        return detail::sliding_attacks(s, occupied, detail::kBishopDirections);
    }

    constexpr Bitboard rook_attacks_reference(Square s, Bitboard occupied) {
        return detail::sliding_attacks(s, occupied, detail::kRookDirections);
    }

    constexpr Bitboard queen_attacks_reference(Square s, Bitboard occupied) {
        return bishop_attacks_reference(s, occupied) | rook_attacks_reference(s, occupied);
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

    // Sliders: reference implementation.
    static_assert(rook_attacks_reference(Square::A1, 0) ==
                  ((file_bb(File::A) | rank_bb(Rank::R1)) & ~square_bb(Square::A1)));
    static_assert(popcount(rook_attacks_reference(Square::D4, 0)) == 14);
    static_assert(popcount(rook_attacks_reference(
                      Square::D4, square_bb(Square::D6) | square_bb(Square::B4))) == 11);
    static_assert((rook_attacks_reference(Square::D4, square_bb(Square::D6)) &
                   square_bb(Square::D6)) != 0); // the blocker itself is included
    static_assert((rook_attacks_reference(Square::D4, square_bb(Square::D6)) &
                   square_bb(Square::D7)) == 0); // squares behind it are not

    static_assert(popcount(bishop_attacks_reference(Square::C1, 0)) == 7);
    static_assert(popcount(bishop_attacks_reference(Square::E4, 0)) == 13);
    static_assert(popcount(bishop_attacks_reference(
                      Square::E4, square_bb(Square::F5) | square_bb(Square::C2))) == 10);

    static_assert(popcount(queen_attacks_reference(Square::D4, 0)) == 27);

    // Totals over all 64 squares on an empty board: rook 896, bishop 560.
    static_assert(detail::total_empty_board_attacks(&rook_attacks_reference) == 896);
    static_assert(detail::total_empty_board_attacks(&bishop_attacks_reference) == 560);
} // namespace KnightLift

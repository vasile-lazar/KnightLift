#pragma once

#include <array>
#include <cstddef>

#if defined(__BMI2__)
#include <immintrin.h>
#endif

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

    // ---- Sliding pieces: PEXT bitboards ---------------------------------------

#if defined(__BMI2__)
    inline constexpr bool kHasHardwarePext = true;
#else
    inline constexpr bool kHasHardwarePext = false;
#endif

    namespace detail {

        // Portable software versions of the BMI2 instructions PEXT and PDEP.
        constexpr Bitboard pext_software(Bitboard value, Bitboard mask) {
            Bitboard result = 0;
            for (Bitboard bit = 1; mask != 0; bit <<= 1) {
                if (value & mask & -mask) result |= bit;
                mask &= mask - 1;
            }
            return result;
        }

        constexpr Bitboard pdep_software(Bitboard value, Bitboard mask) {
            Bitboard result = 0;
            for (Bitboard bit = 1; mask != 0; bit <<= 1) {
                if (value & bit) result |= mask & -mask;
                mask &= mask - 1;
            }
            return result;
        }

        struct SliderEntry {
            Bitboard mask = 0;
            const Bitboard* attacks = nullptr;
        };

        extern std::array<SliderEntry, kNumSquares> gBishopEntries;
        extern std::array<SliderEntry, kNumSquares> gRookEntries;

    }  // namespace detail

    inline Bitboard pext(Bitboard value, Bitboard mask) {
#if defined(__BMI2__)
        return _pext_u64(value, mask);
#else
        return detail::pext_software(value, mask);
#endif
    }

    // Must be called once before bishop_attacks / rook_attacks / queen_attacks.
    void init_attacks();

    inline Bitboard bishop_attacks(Square s, Bitboard occupied) {
        const detail::SliderEntry& e = detail::gBishopEntries[to_index(s)];
        return e.attacks[pext(occupied, e.mask)];
    }

    inline Bitboard rook_attacks(Square s, Bitboard occupied) {
        const detail::SliderEntry& e = detail::gRookEntries[to_index(s)];
        return e.attacks[pext(occupied, e.mask)];
    }

    inline Bitboard queen_attacks(Square s, Bitboard occupied) {
        return bishop_attacks(s, occupied) | rook_attacks(s, occupied);
    }

    // Attacks of any non-pawn piece.
    inline Bitboard piece_attacks(PieceType pt, Square s, Bitboard occupied) {
        switch (pt) {
            case PieceType::Knight: return knight_attacks(s);
            case PieceType::Bishop: return bishop_attacks(s, occupied);
            case PieceType::Rook: return rook_attacks(s, occupied);
            case PieceType::Queen: return queen_attacks(s, occupied);
            case PieceType::King: return king_attacks(s);
            default: return 0;
        }
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

    // Software PEXT / PDEP.
    static_assert(detail::pext_software(0b101101, 0b111000) == 0b101);
    static_assert(detail::pdep_software(0b101, 0b111000) == 0b101000);
    static_assert(detail::pdep_software(0b1101, 0xF0F0) == 0xD0);
    static_assert(detail::pext_software(detail::pdep_software(0b1101, 0xF0F0), 0xF0F0) == 0b1101);

} // namespace KnightLift

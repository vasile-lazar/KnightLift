#pragma once

#include <bit>
#include <string>

#include "types.hpp"

namespace KnightLift {

    constexpr Bitboard kFileABB = 0x0101010101010101ULL;
    constexpr Bitboard kRank1BB = 0x00000000000000FFULL;

    constexpr Bitboard file_bb(File f) {
        return kFileABB << to_index(f);
    }

    constexpr Bitboard rank_bb(Rank r) {
        return kRank1BB << (8 * to_index(r));
    }

    constexpr int popcount(Bitboard bb) {
        return std::popcount(bb);
    }

    constexpr bool more_than_one(Bitboard bb) {
        return (bb & (bb - 1)) != 0;
    }

    // Precondition: bb != 0.
    constexpr Square lsb(Bitboard bb) {
        return static_cast<Square>(std::countr_zero(bb));
    }

    // Precondition: bb != 0. Returns the lowest set square and removes it from bb.
    constexpr Square pop_lsb(Bitboard& bb) {
        const Square s = lsb(bb);
        bb &= bb - 1;
        return s;
    }

    std::string bitboard_to_string(Bitboard bb);

    // ---- Compile-time sanity checks -------------------------------------------

    static_assert(popcount(0xFFULL) == 8);
    static_assert(lsb(square_bb(Square::E4)) == Square::E4);
    static_assert(file_bb(File::H) == 0x8080808080808080ULL);
    static_assert(rank_bb(Rank::R8) == 0xFF00000000000000ULL);
    static_assert(!more_than_one(square_bb(Square::A1)));
    static_assert(more_than_one(square_bb(Square::A1) | square_bb(Square::H8)));

    constexpr bool pop_lsb_works() {
        Bitboard bb = square_bb(Square::C3) | square_bb(Square::F6);
        return pop_lsb(bb) == Square::C3 && pop_lsb(bb) == Square::F6 && bb == 0;
    }
    static_assert(pop_lsb_works());

}  // namespace KnightLift
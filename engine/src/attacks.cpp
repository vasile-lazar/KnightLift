#include "attacks.hpp"

#include <cassert>
#include <cstddef>

namespace KnightLift {

    namespace detail {
        std::array<SliderEntry, kNumSquares> gBishopEntries;
        std::array<SliderEntry, kNumSquares> gRookEntries;
    } // namespace detail

    namespace {
        constexpr std::size_t kBishopTableSize = 5248;
        constexpr std::size_t kRookTableSize = 102400;

        std::array<Bitboard, kBishopTableSize> gBishopTable;
        std::array<Bitboard, kRookTableSize> gRookTable;

        Bitboard relevant_mask(Square s, bool rook) {
            const Bitboard allEdges = file_bb(File::A) | file_bb(File::H) |
                                      rank_bb(Rank::R1) | rank_bb(Rank::R8);
            if (!rook) {
                return bishop_attacks_reference(s, 0) & ~allEdges;
            }

            const Bitboard edges =
                    ((rank_bb(Rank::R1) | rank_bb(Rank::R8)) & ~rank_bb(rank_of(s))) |
                    ((file_bb(File::A) | file_bb(File::H)) & ~file_bb(file_of(s)));
            return rook_attacks_reference(s, 0) & ~edges;
        }

        // Fills one table for all 64 squares, starting at `next`.
        // Returns the position just past the last entry written.
        Bitboard *init_slider_table(std::array<detail::SliderEntry, kNumSquares> &entries,
                                    Bitboard *next,
                                    Bitboard (*reference)(Square, Bitboard),
                                    bool rook) {
            for (std::size_t i = 0; i < kNumSquares; ++i) {
                const Square s = static_cast<Square>(i);
                const Bitboard mask = relevant_mask(s, rook);
                const std::size_t size = std::size_t{1} << popcount(mask);

                entries[i].mask = mask;
                entries[i].attacks = next;

                for (std::size_t index = 0; index < size; ++index) {
                    const Bitboard occupied = detail::pdep_software(static_cast<Bitboard>(index), mask);
                    next[index] = reference(s, occupied);
                }
                next += size;
            }
            return next;
        }
    } // namespace

    void init_attacks() {
        [[maybe_unused]] const Bitboard *bishopEnd =
                init_slider_table(detail::gBishopEntries, gBishopTable.data(),
                                  bishop_attacks_reference, false);
        assert(bishopEnd == gBishopTable.data() + gBishopTable.size());

        [[maybe_unused]] const Bitboard *rookEnd =
                init_slider_table(detail::gRookEntries, gRookTable.data(),
                                  rook_attacks_reference, true);
        assert(rookEnd == gRookTable.data() + gRookTable.size());
    }
} // namespace KnightLift

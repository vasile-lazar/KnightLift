#pragma once

#include <array>
#include <cstddef>

#include "types.hpp"

namespace KnightLift::EvalTables {
    using Table = std::array<int, kNumSquares>;

    // Tables are written from White's view, rank 8 first.
    inline constexpr std::array<int, kNumPieceTypes> kMgValue = {60, 278, 328, 378, 905, 0};
    inline constexpr std::array<int, kNumPieceTypes> kEgValue = {82, 301, 319, 546, 899, 0};
    inline constexpr std::array<int, kNumPieceTypes> kPhaseWeight = {0, 1, 1, 2, 4, 0};
    inline constexpr int kMaxPhase = 24;

    // ---- King safety (5d) ---------------------------------------------------

    inline constexpr int kShieldNear = 24;
    inline constexpr int kShieldFar = 17;
    inline constexpr int kAttackCap = 250;
    // Indexed by PieceType; pawn and king do not attack the king zone for this term.
    inline constexpr std::array<int, kNumPieceTypes> kAttackWeight = {0, 2, 2, 3, 5, 0};

    inline constexpr std::array<Table, kNumPieceTypes> kMgPst = {
        {
            // Pawn
            {
                0, 0, 0, 0, 0, 0, 0, 0,
                89, 82, 92, 119, 102, 71, -14, -34,
                17, 32, 38, 64, 78, 107, 99, 55,
                -14, -8, 5, 15, 32, 35, 25, 18,
                -26, -25, -7, 9, 12, 13, 8, -2,
                -32, -25, -16, -13, 1, -10, 9, -7,
                -26, -24, -20, -25, -12, -1, 13, -11,
                0, 0, 0, 0, 0, 0, 0, 0
            },
            // Knight
            {
                -142, -71, -38, -51, 56, -123, -11, -107,
                -22, -36, 24, 54, 56, 72, -1, 48,
                -14, 28, 23, 49, 99, 110, 58, 32,
                1, -8, 2, 46, 7, 47, 3, 36,
                -26, -13, -3, -5, 5, 13, 21, -10,
                -49, -30, -22, -11, 5, -14, -9, -31,
                -60, -50, -25, -13, -10, -11, -25, -26,
                -63, -39, -36, -23, -21, -24, -35, -46
            },
            // Bishop
            {
                -44, -47, -53, -96, -64, -97, -9, -55,
                -25, -15, -10, -18, 6, -7, -22, 0,
                -6, 14, -2, 26, 30, 87, 46, 14,
                -10, -13, 4, 25, 10, 15, -10, -22,
                -21, -10, -17, 14, 9, -20, -18, -1,
                -7, -5, -5, -6, -7, -7, -6, 3,
                0, 3, 6, -16, -6, 0, 15, 2,
                -10, 20, -10, -18, -13, -19, 3, -22
            },
            // Rook
            {
                56, 61, 59, 67, 75, 54, 80, 81,
                -9, -27, 11, 44, 35, 83, 25, 89,
                -19, -1, -3, 20, 54, 76, 94, 58,
                -41, -33, -27, -11, -8, -1, 24, 9,
                -64, -58, -50, -35, -36, -47, -7, -25,
                -60, -43, -48, -47, -31, -37, 5, -16,
                -68, -60, -40, -37, -34, -29, -10, -44,
                -53, -38, -27, -19, -14, -27, -5, -42
            },
            // Queen
            {
                -91, -13, 10, 25, 51, 70, 64, 21,
                -37, -65, -48, -42, -41, 36, -2, 87,
                -31, -34, -38, -16, 23, 73, 71, 58,
                -31, -45, -41, -28, -31, -17, -20, 9,
                -17, -40, -32, -24, -27, -28, -8, 7,
                -16, -11, -23, -24, -20, -14, -1, 13,
                -19, -12, -6, 3, 3, 8, 12, 33,
                -10, -2, 4, 7, 18, -11, -13, -17
            },
            // King
            {
                -37, 36, 31, -4, -40, -20, 16, 19,
                65, 38, 13, -4, 5, 9, -10, -21,
                -15, 60, 22, -25, -17, 17, 42, -27,
                -24, 3, -44, -75, -93, -58, -44, -74,
                -77, -38, -66, -94, -116, -101, -85, -120,
                -35, -20, -55, -81, -94, -87, -70, -86,
                34, -6, -2, -45, -44, -36, 20, 15,
                -12, 44, 43, -54, 23, -37, 37, 28
            },
        }
    };

    inline constexpr std::array<Table, kNumPieceTypes> kEgPst = {
        {
            {
                0, 0, 0, 0, 0, 0, 0, 0,
                119, 115, 111, 83, 77, 70, 99, 109,
                78, 69, 54, 21, 17, 30, 39, 47,
                58, 45, 32, 9, 16, 24, 38, 36,
                35, 35, 17, 10, 13, 21, 24, 21,
                28, 27, 20, 22, 26, 27, 20, 16,
                39, 37, 34, 16, 43, 40, 27, 21,
                0, 0, 0, 0, 0, 0, 0, 0
            },
            // Knight
            {
                -65, 5, 14, 14, -19, -2, -34, -91,
                -19, 8, 7, 14, -4, -17, -8, -42,
                0, 2, 26, 17, -9, 8, 1, -9,
                3, 17, 35, 36, 39, 30, 30, -7,
                -5, 11, 27, 34, 34, 22, 7, -6,
                -27, -5, 2, 20, 14, -3, -8, -16,
                -33, -11, -15, -14, -14, -18, -21, -28,
                -76, -52, -31, -24, -18, -29, -51, -74
            },
            // Bishop
            {
                38, 19, 30, 29, 22, 21, 9, -1,
                4, 9, 11, 16, 7, 14, 8, -19,
                13, 9, 10, -5, -2, -5, 2, 24,
                5, 22, 1, 6, 3, 0, 16, 15,
                3, 4, 23, 6, -1, 10, 3, -23,
                0, 10, 8, 17, 20, 7, -8, -4,
                -4, -15, -5, -2, -2, -11, -5, -32,
                -23, -18, -13, -7, -16, 0, -30, -19
            },
            // Rook
            {
                -13, 1, 7, 8, 5, 7, -5, -13,
                21, 33, 33, 19, 17, -2, 13, -15,
                30, 27, 32, 21, 5, 8, -6, -5,
                33, 34, 38, 25, 20, 19, 8, 8,
                29, 23, 26, 15, 15, 22, -2, -2,
                5, 1, 7, 0, -5, -2, -26, -25,
                -2, -7, -5, -6, -11, -18, -35, -19,
                0, -9, -1, -8, -14, -9, -28, -20
            },
            // Queen
            {
                48, 25, 20, 18, 18, 4, -3, 18,
                22, 58, 82, 81, 107, 52, 65, 13,
                15, 49, 72, 72, 63, 61, 31, 32,
                18, 49, 62, 62, 85, 82, 91, 58,
                -13, 27, 26, 40, 39, 46, 19, 17,
                -35, -24, 11, -1, 7, 11, -15, -44,
                -37, -41, -38, -51, -46, -63, -99, -90,
                -71, -85, -71, -51, -94, -90, -84, -51
            },
            // King
            {
                -43, 18, 15, -8, 17, 19, 41, -56,
                -25, 31, 40, 19, 19, 40, 59, 1,
                17, 28, 30, 28, 22, 39, 41, 23,
                10, 25, 44, 45, 45, 40, 36, 7,
                -5, 19, 33, 46, 43, 32, 18, 6,
                -14, -4, 7, 19, 23, 17, 5, -1,
                -18, -5, -11, -1, 1, 0, -16, -30,
                -19, -43, -32, -16, -45, -23, -47, -70
            },
        }
    };

    using PieceTable = std::array<std::array<int, kNumSquares>, kNumPieces>;

    // Signed material + PST per (piece, square): + for White, - for Black.
    constexpr PieceTable make_piece_table(const std::array<int, kNumPieceTypes> &value,
                                          const std::array<Table, kNumPieceTypes> &pst) {
        PieceTable t{};
        for (std::size_t p = 0; p < kNumPieces; ++p) {
            const std::size_t type = p % kNumPieceTypes;
            const bool white = p < kNumPieceTypes;
            for (std::size_t s = 0; s < kNumSquares; ++s) {
                const int v = value[type] + pst[type][white ? (s ^ 56) : s];
                t[p][s] = white ? v : -v;
            }
        }
        return t;
    }

    inline constexpr PieceTable kPieceMg = make_piece_table(kMgValue, kMgPst);
    inline constexpr PieceTable kPieceEg = make_piece_table(kEgValue, kEgPst);

    static_assert(kPieceMg[to_index(Piece::WhitePawn)][to_index(Square::E4)] ==
                  -kPieceMg[to_index(Piece::BlackPawn)][to_index(Square::E5)]);
} // namespace KnightLift::EvalTables

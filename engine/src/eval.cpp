#include <algorithm>
#include <array>
#include <cstddef>

#include "eval.hpp"
#include "attacks.hpp"
#include "eval_tables.hpp"

namespace KnightLift {
    namespace {

        // ---- Pawn structure (5b) ------------------------------------------------

        constexpr int kDoubledMg = -10, kDoubledEg = -20;
        constexpr int kIsolatedMg = -10, kIsolatedEg = -15;
        // Indexed by rank from the pawn owner's point of view (0 = own back rank).
        constexpr std::array<int, 8> kPassedMg = {0, 5, 5, 10, 20, 35, 60, 0};
        constexpr std::array<int, 8> kPassedEg = {0, 10, 15, 25, 45, 80, 120, 0};

        constexpr Bitboard adjacent_files(File f) {
            const Bitboard b = file_bb(f);
            return ((b & ~file_bb(File::A)) >> 1) | ((b & ~file_bb(File::H)) << 1);
        }

        constexpr std::array<Bitboard, 8> make_adjacent_table() {
            std::array<Bitboard, 8> t{};
            for (std::size_t f = 0; f < 8; ++f) t[f] = adjacent_files(static_cast<File>(f));
            return t;
        }

        constexpr std::array<Bitboard, 8> kAdjacentFiles = make_adjacent_table();

        using MaskTable = std::array<std::array<Bitboard, kNumSquares>, kNumColors>;

        // kPassedMask[c][sq]: own file + adjacent files, ranks strictly ahead of sq for colour c.
        constexpr MaskTable make_passed_masks() {
            MaskTable t{};
            for (std::size_t s = 0; s < kNumSquares; ++s) {
                const Square sq = static_cast<Square>(s);
                const File f = file_of(sq);
                const std::size_t r = to_index(rank_of(sq));
                const Bitboard files = file_bb(f) | adjacent_files(f);
                const Bitboard ahead = (r == 7) ? Bitboard{0} : (~Bitboard{0} << (8 * (r + 1)));
                const Bitboard behind = (Bitboard{1} << (8 * r)) - 1;
                t[to_index(Color::White)][s] = files & ahead;
                t[to_index(Color::Black)][s] = files & behind;
            }
            return t;
        }

        constexpr MaskTable kPassedMask = make_passed_masks();

        static_assert(make_passed_masks()[to_index(Color::White)][to_index(Square::E4)] == 0x3838383800000000ULL);
        static_assert(make_passed_masks()[to_index(Color::Black)][to_index(Square::E4)] == 0x0000000000383838ULL);

        void pawn_structure(const Position &pos, Color c, int &mg, int &eg) {
            const Bitboard own = pos.pieces(c, PieceType::Pawn);
            const Bitboard enemy = pos.pieces(~c, PieceType::Pawn);

            for (std::size_t f = 0; f < 8; ++f) {
                const int n = popcount(own & file_bb(static_cast<File>(f)));
                if (n > 1) {
                    mg += (n - 1) * kDoubledMg;
                    eg += (n - 1) * kDoubledEg;
                }
            }

            Bitboard bb = own;
            while (bb) {
                const Square s = pop_lsb(bb);
                const std::size_t i = to_index(s);
                if (!(own & kAdjacentFiles[to_index(file_of(s))])) {
                    mg += kIsolatedMg;
                    eg += kIsolatedEg;
                }
                if (!(enemy & kPassedMask[to_index(c)][i])) {
                    const std::size_t r = to_index(rank_of(s));
                    const std::size_t rel = (c == Color::White) ? r : 7 - r;
                    mg += kPassedMg[rel];
                    eg += kPassedEg[rel];
                }
            }
        }

                // ---- Mobility (5c) ------------------------------------------------------

        struct MobilityParams {
            int base;
            int mg;
            int eg;
        };

        // Indexed by PieceType; pawn and king unused.
        constexpr std::array<MobilityParams, kNumPieceTypes> kMobility = {{
            {0, 0, 0},    // Pawn
            {4, 4, 4},    // Knight
            {6, 5, 5},    // Bishop
            {6, 2, 4},    // Rook
            {13, 1, 2},   // Queen
            {0, 0, 0},    // King
        }};

        constexpr Bitboard pawn_attacks_bb(Color c, Bitboard pawns) {
            if (c == Color::White) {
                return ((pawns & ~file_bb(File::A)) << 7) | ((pawns & ~file_bb(File::H)) << 9);
            }
            return ((pawns & ~file_bb(File::A)) >> 9) | ((pawns & ~file_bb(File::H)) >> 7);
        }

        static_assert(pawn_attacks_bb(Color::White, square_bb(Square::E4)) ==
                      (square_bb(Square::D5) | square_bb(Square::F5)));
        static_assert(pawn_attacks_bb(Color::White, square_bb(Square::A2)) == square_bb(Square::B3));
        static_assert(pawn_attacks_bb(Color::Black, square_bb(Square::H7)) == square_bb(Square::G6));
        static_assert(pawn_attacks_bb(Color::Black, square_bb(Square::A7)) == square_bb(Square::B6));

        void mobility(const Position& pos, Color c, int& mg, int& eg) {
            const Bitboard occ = pos.occupied();
            const Bitboard area = ~(pos.pieces(c) | pawn_attacks_bb(~c, pos.pieces(~c, PieceType::Pawn)));

            for (std::size_t t = to_index(PieceType::Knight); t <= to_index(PieceType::Queen); ++t) {
                const PieceType pt = static_cast<PieceType>(t);
                Bitboard bb = pos.pieces(c, pt);
                while (bb) {
                    const Square s = pop_lsb(bb);
                    const int n = popcount(piece_attacks(pt, s, occ) & area);
                    mg += kMobility[t].mg * (n - kMobility[t].base);
                    eg += kMobility[t].eg * (n - kMobility[t].base);
                }
            }
        }
    } // namespace

    int king_safety(const Position& pos, Color c) {
        const Square ks = pos.king_square(c);
        int score = 0;

        // Pawn shield: three files centred on the king (file clamped to B..G).
        const int f = std::clamp(static_cast<int>(to_index(file_of(ks))), 1, 6);
        const Bitboard files = file_bb(static_cast<File>(f - 1)) | file_bb(static_cast<File>(f)) |
                               file_bb(static_cast<File>(f + 1));
        const Bitboard pawns = pos.pieces(c, PieceType::Pawn) & files;
        const int r = static_cast<int>(to_index(rank_of(ks)));
        const int dir = (c == Color::White) ? 1 : -1;

        const int r1 = r + dir;
        if (r1 >= 0 && r1 <= 7) {
            score += EvalTables::kShieldNear * popcount(pawns & rank_bb(static_cast<Rank>(r1)));
        }
        const int r2 = r + 2 * dir;
        if (r2 >= 0 && r2 <= 7) {
            score += EvalTables::kShieldFar * popcount(pawns & rank_bb(static_cast<Rank>(r2)));
        }

        // Attackers of the king zone.
        const Bitboard zone = king_attacks(ks) | square_bb(ks);
        const Bitboard occ = pos.occupied();
        int attackers = 0;
        int units = 0;
        for (std::size_t t = to_index(PieceType::Knight); t <= to_index(PieceType::Queen); ++t) {
            const PieceType pt = static_cast<PieceType>(t);
            Bitboard bb = pos.pieces(~c, pt);
            while (bb) {
                const Square s = pop_lsb(bb);
                if (piece_attacks(pt, s, occ) & zone) {
                    ++attackers;
                    units += EvalTables::kAttackWeight[t];
                }
            }
        }
        if (attackers >= 2) {
            score -= std::min(units * units / 2, EvalTables::kAttackCap);
        }
        return score;
    }

    int evaluate(const Position &pos) {
        std::array<int, kNumColors> mg{};
        std::array<int, kNumColors> eg{};

        for (std::size_t c = 0; c < kNumColors; ++c) {
            const Color color = static_cast<Color>(c);
            pawn_structure(pos, color, mg[c], eg[c]);
            mobility(pos, color, mg[c], eg[c]);
            mg[c] += king_safety(pos, color);
        }

        const int maxPhase = EvalTables::kMaxPhase;
        const int phase = std::min(pos.phase(), maxPhase);
        const int mgScore = pos.psq_mg() + mg[0] - mg[1];
        const int egScore = pos.psq_eg() + eg[0] - eg[1];
        const int score = (mgScore * phase + egScore * (maxPhase - phase)) / maxPhase;
        return pos.side_to_move() == Color::White ? score : -score;
    }
} // namespace KnightLift

#include "eval.hpp"

#include <array>

namespace KnightLift {

    namespace {

        constexpr std::array<int, kNumPieceTypes> kPieceValue = {100, 320, 330, 500, 900, 0};

    }  // namespace

    int evaluate(const Position& pos) {
        int score = 0;
        for (std::size_t t = 0; t < kNumPieceTypes; ++t) {
            const PieceType pt = static_cast<PieceType>(t);
            score += kPieceValue[t] * (popcount(pos.pieces(Color::White, pt)) -
                                       popcount(pos.pieces(Color::Black, pt)));
        }
        return pos.side_to_move() == Color::White ? score : -score;
    }

}  // namespace KnightLift
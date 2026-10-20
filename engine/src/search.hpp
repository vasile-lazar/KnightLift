#pragma once

#include <cstdint>

#include "move.hpp"
#include "position.hpp"

namespace KnightLift {

    constexpr int kInfinity = 32001;
    constexpr int kMateScore = 32000;
    constexpr int kMateBound = kMateScore - 1000;  // |score| above this encodes a mate

    struct SearchResult {
        Move best = Move::none();
        int score = 0;
        std::uint64_t nodes = 0;
    };

    // Fixed-depth search. Returns Move::none() as best move if there are no legal moves.
    // useAlphaBeta = false runs plain minimax, kept as a reference and for measurements.
    SearchResult search(Position& pos, int depth, bool useAlphaBeta = true);

}  // namespace KnightLift
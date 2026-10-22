#pragma once

#include "position.hpp"

namespace KnightLift {

    // Static evaluation in centipawns, from the side to move's point of view.
    int evaluate(const Position& pos);

    // Middlegame-only king safety term for color c (positive = safe). Exposed for tests.
    int king_safety(const Position& pos, Color c);

}  // namespace KnightLift
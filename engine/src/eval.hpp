#pragma once

#include "position.hpp"

namespace KnightLift {

    // Static evaluation in centipawns, from the side to move's point of view.
    int evaluate(const Position& pos);

    // Middlegame-only king safety term for color c (positive = safe). Exposed for tests.
    int king_safety(const Position& pos, Color c);

    // Straightforward unoptimised evaluation, kept to test evaluate() against.
    int evaluate_reference(const Position& pos);
}  // namespace KnightLift
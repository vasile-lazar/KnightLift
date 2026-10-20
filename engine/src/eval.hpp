#pragma once

#include "position.hpp"

namespace KnightLift {

    // Static evaluation in centipawns, from the side to move's point of view.
    int evaluate(const Position& pos);

}  // namespace KnightLift
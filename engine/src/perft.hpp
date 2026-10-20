#pragma once

#include <cstdint>

#include "position.hpp"

namespace KnightLift {

    std::uint64_t perft(Position& pos, int depth);

    // Prints "move: nodes" for every legal root move and the total; returns the total.
    std::uint64_t perft_divide(Position& pos, int depth);

}  // namespace KnightLift
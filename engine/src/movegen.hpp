#pragma once

#include "move.hpp"
#include "position.hpp"

namespace KnightLift {

    // Appends pseudo-legal moves for the side to move. Castling is fully legal.
    void generate_pseudo_legal(const Position& pos, MoveList& list);

}  // namespace KnightLift
#pragma once

#include "move.hpp"
#include "position.hpp"

namespace KnightLift {

    // Appends pseudo-legal moves for the side to move. Castling is fully legal.
    void generate_pseudo_legal(const Position& pos, MoveList& list);
    // Captures (including en passant) and all promotions, in the same relative order as
    // generate_pseudo_legal. No quiet moves, no castling.
    void generate_pseudo_legal_captures(const Position& pos, MoveList& list);
    // Legal moves only. Makes and unmakes moves internally, so pos is restored.
    void generate_legal(Position& pos, MoveList& list);
}  // namespace KnightLift
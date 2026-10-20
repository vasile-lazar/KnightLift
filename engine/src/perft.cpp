#include "perft.hpp"

#include <ostream>

#include "movegen.hpp"

namespace KnightLift {

    std::uint64_t perft(Position& pos, int depth) {
        if (depth == 0) return 1;

        MoveList list;
        generate_pseudo_legal(pos, list);

        std::uint64_t nodes = 0;
        for (const Move m : list) {
            pos.make_move(m);
            if (!pos.king_left_in_check()) {
                nodes += (depth == 1) ? 1 : perft(pos, depth - 1);
            }
            pos.unmake_move(m);
        }
        return nodes;
    }

    std::uint64_t perft_divide(Position& pos, int depth, std::ostream& out) {
        MoveList list;
        generate_pseudo_legal(pos, list);

        std::uint64_t total = 0;
        for (const Move m : list) {
            pos.make_move(m);
            if (!pos.king_left_in_check()) {
                const std::uint64_t n = (depth <= 1) ? 1 : perft(pos, depth - 1);
                out << move_to_uci(m) << ": " << n << '\n';
                total += n;
            }
            pos.unmake_move(m);
        }
        return total;
    }

}  // namespace KnightLift
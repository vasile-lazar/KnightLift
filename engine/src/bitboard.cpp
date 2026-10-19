#include "bitboard.hpp"

namespace KnightLift {

    std::string bitboard_to_string(Bitboard bb) {
        std::string out;

        for (int r = 7; r >= 0; --r) {
            out += static_cast<char>('1' + r);
            out += ' ';
            for (int f = 0; f < 8; ++f) {
                const Square s = make_square(static_cast<File>(f), static_cast<Rank>(r));
                out += (bb & square_bb(s)) ? 'X' : '.';
                out += ' ';
            }
            out += '\n';
        }
        out += "  a b c d e f g h\n";
        return out;
    }

}  // namespace KnightLift
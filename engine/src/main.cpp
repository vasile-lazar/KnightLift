#include <iostream>
#include <string_view>

#include "position.hpp"

namespace {
    constexpr std::string_view kEngineName = "KnightLift";
    constexpr std::string_view kVersion = "0.1.0";

    constexpr std::string_view kRoundTripFens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    };

    constexpr std::string_view kBadFens[] = {
        "",
        "8/8/8/8/8/8/8 w - - 0 1",
        "rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",
        "8/8/8/8/8/8/8/8 w - - 0 1",
    };
}  // namespace

int main() {
    using namespace KnightLift;

    std::cout << kEngineName << ' ' << kVersion << '\n';

    Position pos;

    for (const std::string_view fen : kRoundTripFens) {
        const bool ok = pos.set_from_fen(fen) && pos.to_fen() == fen;
        std::cout << (ok ? "OK       " : "FAIL     ") << fen << '\n';
    }

    for (const std::string_view fen : kBadFens) {
        const bool rejected = !pos.set_from_fen(fen);
        std::cout << (rejected ? "REJECTED " : "ACCEPTED ") << '"' << fen << "\"\n";
    }

    pos.set_from_fen(kStartFen);
    std::cout << '\n' << pos.to_string();
    std::cout << "occupied: " << popcount(pos.occupied()) << '\n';
    std::cout << bitboard_to_string(pos.pieces(Color::White, PieceType::Pawn));
    return 0;
}
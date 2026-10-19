#include <chrono>
#include <iostream>
#include <string_view>

#include "attacks.hpp"
#include "position.hpp"
#include "move.hpp"

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
    std::uint64_t next_random(std::uint64_t& state) {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return state * 2685821657736338717ULL;
    }

    bool verify_sliders() {
        using namespace KnightLift;

        std::uint64_t state = 0x9E3779B97F4A7C15ULL;
        for (std::size_t i = 0; i < kNumSquares; ++i) {
            const Square s = static_cast<Square>(i);
            for (int n = 0; n < 10000; ++n) {
                const Bitboard occupied = next_random(state) & next_random(state);
                if (rook_attacks(s, occupied) != rook_attacks_reference(s, occupied) ||
                    bishop_attacks(s, occupied) != bishop_attacks_reference(s, occupied)) {
                    return false;
                    }
            }
        }
        return true;
    }

}  // namespace

int main() {
    using namespace KnightLift;

    std::cout << kEngineName << ' ' << kVersion << '\n';

    std::cout << "pext: " << (kHasHardwarePext ? "hardware (BMI2)" : "software fallback") << '\n';

    const auto start = std::chrono::steady_clock::now();
    init_attacks();
    const auto stop = std::chrono::steady_clock::now();
    std::cout << "init_attacks: "
              << std::chrono::duration<double, std::milli>(stop - start).count() << " ms\n";

    std::cout << "sliders vs reference: " << (verify_sliders() ? "OK" : "MISMATCH") << '\n';

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

    const Move sampleMoves[] = {
        Move(Square::E2, Square::E4, MoveFlag::DoublePush),
        Move(Square::G1, Square::F3, MoveFlag::Quiet),
        Move(Square::E4, Square::D5, MoveFlag::Capture),
        Move(Square::E1, Square::G1, MoveFlag::KingCastle),
        Move(Square::E7, Square::E8, MoveFlag::PromoQueen),
        Move(Square::B7, Square::A8, MoveFlag::PromoCaptureKnight),
        Move::none(),
    };

    MoveList list;
    for (const Move m : sampleMoves) {
        list.push(m);
    }

    for (const Move m : list) {
        std::cout << move_to_uci(m) << (m.is_capture() ? " capture" : "")
                  << (m.is_promotion() ? " promotion" : "") << '\n';
    }
    std::cout << "moves: " << list.size() << ", sizeof(Move) = " << sizeof(Move) << '\n';

    std::cout << "\nknight g1:\n" << bitboard_to_string(knight_attacks(Square::G1));
    std::cout << "king h1:\n" << bitboard_to_string(king_attacks(Square::H1));
    std::cout << "white pawn d4:\n" << bitboard_to_string(pawn_attacks(Color::White, Square::D4));

    const Bitboard rookBlockers = square_bb(Square::D6) | square_bb(Square::B4);
    std::cout << "\nrook d4, blockers d6 b4:\n"
              << bitboard_to_string(rook_attacks_reference(Square::D4, rookBlockers));

    const Bitboard bishopBlockers = square_bb(Square::F5) | square_bb(Square::C2);
    std::cout << "bishop e4, blockers f5 c2:\n"
              << bitboard_to_string(bishop_attacks_reference(Square::E4, bishopBlockers));

    return 0;
}
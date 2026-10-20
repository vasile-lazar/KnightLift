#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

#include "../engine/src/attacks.hpp"
#include "../engine/src/movegen.hpp"
#include "../engine/src/perft.hpp"
#include "../engine/src/position.hpp"

using namespace KnightLift;

namespace {

int gFailures = 0;

void check(bool condition, const std::string& what) {
    if (!condition) {
        ++gFailures;
        std::cout << "FAIL: " << what << '\n';
    }
}

// ---- FEN --------------------------------------------------------------------

void test_fen() {
    constexpr std::string_view good[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    };
    constexpr std::string_view bad[] = {
        "",
        "8/8/8/8/8/8/8 w - - 0 1",
        "rnbqkbnr/pppppppp/9/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1",
        "8/8/8/8/8/8/8/8 w - - 0 1",
        "4k3/8/8/8/8/8/8/P3K3 w - - 0 1",
    };

    Position pos;
    for (const std::string_view fen : good) {
        check(pos.set_from_fen(fen) && pos.to_fen() == fen, "fen round trip: " + std::string(fen));
    }
    for (const std::string_view fen : bad) {
        check(!pos.set_from_fen(fen), "fen rejected: " + std::string(fen));
    }
}

// ---- Sliders vs reference ---------------------------------------------------

std::uint64_t next_random(std::uint64_t& state) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 2685821657736338717ULL;
}

void test_sliders() {
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    bool ok = true;
    for (std::size_t i = 0; i < kNumSquares; ++i) {
        const Square s = static_cast<Square>(i);
        for (int n = 0; n < 10000; ++n) {
            const Bitboard occupied = next_random(state) & next_random(state);
            ok = ok && rook_attacks(s, occupied) == rook_attacks_reference(s, occupied) &&
                 bishop_attacks(s, occupied) == bishop_attacks_reference(s, occupied);
        }
    }
    check(ok, "sliders match reference");
}

// ---- Pseudo-legal move counts -----------------------------------------------

void test_move_counts() {
    struct Case {
        std::string_view fen;
        std::size_t moves;
    };
    constexpr Case cases[] = {
        {kStartFen, 20},
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 26},
        {"8/P7/8/8/8/8/8/k6K w - - 0 1", 7},
        {"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", 7},
    };

    Position pos;
    for (const Case& c : cases) {
        pos.set_from_fen(c.fen);
        MoveList list;
        generate_pseudo_legal(pos, list);
        check(list.size() == c.moves, "move count: " + std::string(c.fen));
    }
}

// ---- Make / unmake ----------------------------------------------------------

bool roundtrip(Position& pos, int depth) {
    if (depth == 0) return true;

    const std::string before = pos.to_fen();
    MoveList list;
    generate_pseudo_legal(pos, list);
    for (const Move m : list) {
        pos.make_move(m);
        const bool ok = pos.consistent() && roundtrip(pos, depth - 1);
        pos.unmake_move(m);
        if (!ok || !pos.consistent() || pos.to_fen() != before) return false;
    }
    return true;
}

void test_make_unmake() {
    constexpr std::string_view fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "8/P7/8/8/8/8/8/k6K w - - 0 1",
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
    };

    Position pos;
    for (const std::string_view fen : fens) {
        pos.set_from_fen(fen);
        check(roundtrip(pos, 3), "make/unmake round trip: " + std::string(fen));
    }
}

// ---- Perft ------------------------------------------------------------------

struct PerftCase {
    std::string_view name;
    std::string_view fen;
    int maxDepth;
    std::uint64_t expected[7];  // expected[d] is the node count at depth d
};

constexpr PerftCase kPerftCases[] = {
    {"startpos", kStartFen, 5, {0, 20, 400, 8902, 197281, 4865609, 0}},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4,
     {0, 48, 2039, 97862, 4085603, 0, 0}},
    {"position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5,
     {0, 14, 191, 2812, 43238, 674624, 0}},
    {"position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4,
     {0, 6, 264, 9467, 422333, 0, 0}},
    {"position 4 mirrored", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4,
     {0, 6, 264, 9467, 422333, 0, 0}},
    {"position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4,
     {0, 44, 1486, 62379, 2103487, 0, 0}},
    {"position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4,
     {0, 46, 2079, 89890, 3894594, 0, 0}},
};

void test_perft() {
    Position pos;
    for (const PerftCase& c : kPerftCases) {
        pos.set_from_fen(c.fen);
        for (int d = 1; d <= c.maxDepth; ++d) {
            const auto start = std::chrono::steady_clock::now();
            const std::uint64_t nodes = perft(pos, d);
            const auto stop = std::chrono::steady_clock::now();

            check(nodes == c.expected[d], std::string("perft ") + std::string(c.name) +
                                              " depth " + std::to_string(d) + ": got " +
                                              std::to_string(nodes) + ", expected " +
                                              std::to_string(c.expected[d]));
            if (d == c.maxDepth) {
                const double seconds = std::chrono::duration<double>(stop - start).count();
                std::cout << c.name << " depth " << d << ": " << nodes << " nodes, "
                          << seconds * 1000.0 << " ms, " << nodes / seconds / 1e6 << " Mnps\n";
            }
        }
    }
}

}  // namespace

int main() {
    init_attacks();

    test_fen();
    test_sliders();
    test_move_counts();
    test_make_unmake();
    test_perft();

    std::cout << (gFailures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << '\n';
    return gFailures == 0 ? 0 : 1;
}
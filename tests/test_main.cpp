#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <vector>

#include "../engine/src/eval.hpp"
#include "tt.hpp"
#include "../engine/src/attacks.hpp"
#include "../engine/src/movegen.hpp"
#include "../engine/src/perft.hpp"
#include "../engine/src/position.hpp"
#include "../engine/src/uci.hpp"
#include "../engine/src/search.hpp"
using namespace KnightLift;

namespace {
    int gFailures = 0;

    void check(bool condition, const std::string &what) {
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
        for (const std::string_view fen: good) {
            check(pos.set_from_fen(fen) && pos.to_fen() == fen, "fen round trip: " + std::string(fen));
        }
        for (const std::string_view fen: bad) {
            check(!pos.set_from_fen(fen), "fen rejected: " + std::string(fen));
        }
    }

    // ---- Sliders vs reference ---------------------------------------------------

    std::uint64_t next_random(std::uint64_t &state) {
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
        for (const Case &c: cases) {
            pos.set_from_fen(c.fen);
            MoveList list;
            generate_pseudo_legal(pos, list);
            check(list.size() == c.moves, "move count: " + std::string(c.fen));
        }
    }

    // ---- Make / unmake ----------------------------------------------------------

    bool roundtrip(Position &pos, int depth) {
        if (depth == 0) return true;

        const std::string before = pos.to_fen();
        MoveList list;
        generate_pseudo_legal(pos, list);
        for (const Move m: list) {
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
        for (const std::string_view fen: fens) {
            pos.set_from_fen(fen);
            check(roundtrip(pos, 3), "make/unmake round trip: " + std::string(fen));
        }
    }

    // ---- Perft ------------------------------------------------------------------

    struct PerftCase {
        std::string_view name;
        std::string_view fen;
        int maxDepth;
        std::uint64_t expected[7]; // expected[d] is the node count at depth d
    };

    constexpr PerftCase kPerftCases[] = {
        {"startpos", kStartFen, 5, {0, 20, 400, 8902, 197281, 4865609, 0}},
        {
            "kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4,
            {0, 48, 2039, 97862, 4085603, 0, 0}
        },
        {
            "position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5,
            {0, 14, 191, 2812, 43238, 674624, 0}
        },
        {
            "position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4,
            {0, 6, 264, 9467, 422333, 0, 0}
        },
        {
            "position 4 mirrored", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4,
            {0, 6, 264, 9467, 422333, 0, 0}
        },
        {
            "position 5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4,
            {0, 44, 1486, 62379, 2103487, 0, 0}
        },
        {
            "position 6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4,
            {0, 46, 2079, 89890, 3894594, 0, 0}
        },
    };

    void test_perft() {
        Position pos;
        for (const PerftCase &c: kPerftCases) {
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

    // ---- Zobrist ----------------------------------------------------------------

    bool play(Position &pos, const std::string &uci) {
        MoveList list;
        generate_legal(pos, list);
        for (const Move m: list) {
            if (move_to_uci(m) == uci) {
                pos.make_move(m);
                return true;
            }
        }
        return false;
    }

    bool key_walk(Position &pos, int depth) {
        if (depth == 0) return true;

        const Key before = pos.key();
        MoveList list;
        generate_legal(pos, list);
        for (const Move m: list) {
            pos.make_move(m);
            const bool ok = pos.key() == pos.compute_key() && key_walk(pos, depth - 1);
            pos.unmake_move(m);
            if (!ok || pos.key() != before) return false;
        }
        return true;
    }

    void test_zobrist() {
        Position pos;

        // Incremental key == from-scratch key at every node, and unmake restores it.
        for (const PerftCase &c: kPerftCases) {
            pos.set_from_fen(c.fen);
            check(key_walk(pos, 3), "zobrist walk: " + std::string(c.name));
        }

        // Transposition: same position reached by two move orders.
        Position a, b, start;
        start.set_from_fen(kStartFen);
        a.set_from_fen(kStartFen);
        b.set_from_fen(kStartFen);
        bool ok = true;
        for (const char *m: {"g1f3", "g8f6", "b1c3", "b8c6"}) ok = play(a, m) && ok;
        for (const char *m: {"b1c3", "b8c6", "g1f3", "g8f6"}) ok = play(b, m) && ok;
        check(ok, "zobrist transposition: moves are legal");
        check(a.key() == b.key(), "zobrist transposition: equal keys");
        check(a.key() != start.key(), "zobrist transposition: differs from start");

        // En passant: hashed only when a pawn can capture.
        // 1.e4 (no black pawn on d4/f4): ep square must not change the key.
        a.set_from_fen(kStartFen);
        play(a, "e2e4");
        b.set_from_fen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
        check(a.key() == b.key(), "zobrist ep: no capturing pawn, key ignores ep");
        check(a.ep_square() != Square::None, "zobrist ep: ep square still stored");

        // Black pawn on d4 can capture on e3: ep must change the key.
        a.set_from_fen("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1");
        b.set_from_fen("4k3/8/8/8/3pP3/8/8/4K3 b - - 0 1");
        check(a.key() != b.key(), "zobrist ep: capturing pawn, key includes ep");

        // Side to move and castling rights change the key.
        a.set_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        b.set_from_fen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1");
        check(a.key() != b.key(), "zobrist: side to move");
        b.set_from_fen("r3k2r/8/8/8/8/8/8/R3K2R w Kkq - 0 1");
        check(a.key() != b.key(), "zobrist: castling rights");

        // Null move: key stays consistent and unmake restores everything.
        pos.set_from_fen("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1"); // black pawn can capture ep
        const Key beforeNull = pos.key();
        const std::string fenBeforeNull = pos.to_fen();
        pos.make_null_move();
        check(pos.consistent() && pos.key() == pos.compute_key(), "null move: key consistent");
        pos.unmake_null_move();
        check(pos.consistent() && pos.key() == beforeNull && pos.to_fen() == fenBeforeNull,
              "null move: unmake restores position and key");
    }

    std::string run_uci(const std::string &script) {
        std::istringstream in(script);
        std::ostringstream out;
        uci_loop(in, out);
        return out.str();
    }

    void test_uci() {
        const auto has = [](const std::string &text, const std::string &part) {
            return text.find(part) != std::string::npos;
        };

        const std::string handshake = run_uci("uci\nisready\nquit\n");
        check(has(handshake, "uciok") && has(handshake, "readyok"), "uci handshake");

        // After 1.e4 e5 White has 29 legal moves.
        const std::string perft1 = run_uci("position startpos moves e2e4 e7e5\ngo perft 1\n");
        check(has(perft1, "Nodes searched: 29"), "uci position + go perft");

        // Stalemate: no legal move.
        const std::string stalemate = run_uci("position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1\ngo\n");
        check(has(stalemate, "bestmove 0000"), "uci stalemate bestmove");

        const std::string normal = run_uci("position startpos\ngo depth 3\n");
        check(has(normal, "bestmove ") && !has(normal, "bestmove 0000"), "uci bestmove from startpos");

        const std::string mate = run_uci("position fen 6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1\ngo depth 4\n");
        check(has(mate, "bestmove a1a8") && has(mate, "score mate 1"), "uci finds mate in 1");

        const std::string hash = run_uci(
            "uci\nsetoption name Hash value 16\nucinewgame\nposition startpos\ngo depth 4\n");
        check(has(hash, "option name Hash") && has(hash, "bestmove ") && has(hash, "hashfull"),
              "uci Hash option, ucinewgame, hashfull");

        const std::string bench = run_uci("bench 3\n");
        check(has(bench, "Total nodes:") && has(bench, "Nodes/second:"), "uci bench");
    }

    void test_search_iterative() {
        Position pos;
        pos.set_from_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        const std::string before = pos.to_fen();

        SearchLimits limits;
        limits.hardMs = limits.optimumMs = 100;
        std::atomic<bool> stop{false};

        const auto start = std::chrono::steady_clock::now();
        const SearchResult r = search_iterative(pos, limits, stop, nullptr);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        check(!r.best.is_none(), "iterative search returns a move");
        check(ms < 1000, "iterative search respects its time limit");
        check(pos.to_fen() == before && pos.consistent(), "aborted search restores the position");

        pos.set_from_fen("6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1");
        const SearchResult m = search_iterative(pos, SearchLimits{}, stop, nullptr);
        check(move_to_uci(m.best) == "a1a8" && m.score == kMateScore - 1,
              "iterative search finds mate in 1");
    }

    void test_search() {
        Position pos;

        pos.set_from_fen("6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1");
        SearchResult r = search(pos, 2);
        check(move_to_uci(r.best) == "a1a8" && r.score == kMateScore - 1, "search: back-rank mate in 1");

        pos.set_from_fen("4k3/8/8/3q4/8/8/8/3QK3 w - - 0 1");
        r = search(pos, 3);
        check(move_to_uci(r.best) == "d1d5", "search: wins a free queen");

        // Quiescence: a depth-1 search must not grab a defended pawn with the queen.
        pos.set_from_fen("4k3/8/2p5/3p4/8/8/8/3QK3 w - - 0 1");
        r = search(pos, 1);
        check(move_to_uci(r.best) != "d1d5", "quiescence avoids a poisoned pawn");
    }

    void test_alphabeta() {
        constexpr std::string_view fens[] = {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            "6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1",
        };

        Position pos;
        for (const std::string_view fen: fens) {
            for (int depth = 1; depth <= 3; ++depth) {
                pos.set_from_fen(fen);
                const SearchResult full = search(pos, depth, false);
                pos.set_from_fen(fen);
                const SearchResult pruned = search(pos, depth, true);

                const std::string what = std::string(fen) + " depth " + std::to_string(depth);
                check(pruned.score == full.score && pruned.best == full.best,
                      "alpha-beta equals minimax: " + what);
                check(pruned.nodes <= full.nodes, "alpha-beta never searches more: " + what);

                if (depth == 3) {
                    std::cout << "depth 3 nodes minimax " << full.nodes << ", alpha-beta "
                            << pruned.nodes << " (" << static_cast<double>(full.nodes) /
                            static_cast<double>(pruned.nodes)
                            << "x fewer)\n";
                }
            }
        }
    }

    // ---- Eval -------------------------------------------------------------------

    std::string mirror_fen(const std::string &fen) {
        std::vector<std::string> f;
        std::istringstream in(fen);
        for (std::string t; in >> t;) f.push_back(t);

        // Board: reverse rank order, swap piece colours.
        std::vector<std::string> ranks;
        std::istringstream bs(f[0]);
        for (std::string r; std::getline(bs, r, '/');) ranks.push_back(r);
        std::reverse(ranks.begin(), ranks.end());
        std::string board;
        for (std::size_t i = 0; i < ranks.size(); ++i) {
            if (i) board += '/';
            for (const char ch: ranks[i]) {
                if (std::isupper(static_cast<unsigned char>(ch))) board += static_cast<char>(std::tolower(ch));
                else if (std::islower(static_cast<unsigned char>(ch))) board += static_cast<char>(std::toupper(ch));
                else board += ch;
            }
        }

        const std::string side = f[1] == "w" ? "b" : "w";

        // Castling: swap case, keep canonical KQkq order.
        std::string castling;
        for (const char ch: std::string("KQkq")) {
            const char orig = std::isupper(static_cast<unsigned char>(ch)) ? static_cast<char>(std::tolower(ch))
                                                                           : static_cast<char>(std::toupper(ch));
            if (f[2].find(orig) != std::string::npos) castling += ch;
        }
        if (castling.empty()) castling = "-";

        // En passant: flip the rank (3 <-> 6).
        std::string ep = f[3];
        if (ep != "-") ep[1] = ep[1] == '3' ? '6' : '3';

        return board + " " + side + " " + castling + " " + ep + " " + f[4] + " " + f[5];
    }

    void test_eval() {
        Position a, b;

        // Start position is balanced.
        a.set_from_fen(kStartFen);
        check(evaluate(a) == 0, "eval: startpos == 0");

        // Colour symmetry over every perft position: a mirrored position must score the same
        // from the side to move's point of view. Catches a wrong `sq ^ 56` or a sign error.
        for (const PerftCase &c: kPerftCases) {
            const std::string mirrored = mirror_fen(std::string(c.fen));
            const bool ok = a.set_from_fen(c.fen) && b.set_from_fen(mirrored);
            check(ok, "eval: mirrored FEN parses: " + mirrored);
            check(ok && evaluate(a) == evaluate(b), "eval: colour symmetry: " + std::string(c.name));
        }

        // Same board, other side to move: score flips sign (eval is side-to-move POV).
        const std::string board = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R";
        a.set_from_fen(board + " w KQkq - 0 1");
        b.set_from_fen(board + " b KQkq - 0 1");
        check(evaluate(a) == -evaluate(b), "eval: side-to-move sign");

        // Material sanity: an extra queen is worth clearly more than a minor piece.
        a.set_from_fen("4k3/8/8/8/8/8/8/3QK3 w - - 0 1");
        check(evaluate(a) > 800, "eval: extra queen");

        // Tapering: in a pawn endgame an advanced passed pawn must score higher than a home one.
        a.set_from_fen("4k3/8/8/8/8/8/P7/4K3 w - - 0 1");
        b.set_from_fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
        check(evaluate(b) > evaluate(a), "eval: endgame pawn advancement");

        // Phase clamp: far more than 24 phase points (promotions) must not break anything.
        a.set_from_fen("QQQQkQQQ/QQQQQQQQ/8/8/8/8/8/4K3 w - - 0 1");
        check(std::abs(evaluate(a)) < 30000, "eval: phase clamp, no overflow");

        // Isolated pawns: a2+c2 vs a2+b2 have identical PST sums, so the difference is exactly
        // two isolated penalties (both setups have the same passed bonuses).
        a.set_from_fen("4k3/8/8/8/8/8/P1P5/4K3 w - - 0 1");
        b.set_from_fen("4k3/8/8/8/8/8/PP6/4K3 w - - 0 1");
        check(evaluate(b) - evaluate(a) == 30, "eval: isolated pawn penalty");

        // Doubled pawns (plus the isolation that comes with them) cost clearly more than a
        // pawn pair that is neither.
        a.set_from_fen("4k3/8/8/8/8/P7/P7/4K3 w - - 0 1");
        b.set_from_fen("4k3/8/8/8/8/1P6/P7/4K3 w - - 0 1");
        check(evaluate(b) > evaluate(a) + 40, "eval: doubled pawn penalty");

        // Passed pawn: e6 is blocked from passing by a black d7 pawn, but passed against a7.
        a.set_from_fen("4k3/3p4/4P3/8/8/8/8/4K3 w - - 0 1");
        b.set_from_fen("4k3/p7/4P3/8/8/8/8/4K3 w - - 0 1");
        check(evaluate(b) > evaluate(a) + 50, "eval: passed pawn bonus");

        // Mobility: boxed-in bishop scores clearly lower than a free one.
        a.set_from_fen("4k3/8/8/8/8/8/1P1P4/2B1K3 w - - 0 1");
        b.set_from_fen("4k3/8/8/8/8/8/P5P1/2B1K3 w - - 0 1");
        check(evaluate(b) > evaluate(a) + 25, "eval: bishop mobility");
    }

    void test_tt() {
        TranspositionTable tt(1);
        TTEntry e;
        const Key k = 0x123456789ABCDEF0ULL;

        check(!tt.probe(k, e), "tt: empty table misses");

        tt.store(k, Move::none(), 42, 5, Bound::Exact);
        check(tt.probe(k, e) && e.score == 42 && e.depth == 5 && e.bound() == Bound::Exact,
              "tt: store/probe round trip");

        // Same slot (same high bits), different low bits: must not be reported as a hit.
        check(!tt.probe(k ^ 1ULL, e), "tt: verification key rejects a different position");

        // A store without a move keeps the previously stored move.
        Position pos;
        pos.set_from_fen(kStartFen);
        MoveList list;
        generate_legal(pos, list);
        Move first = Move::none();
        for (const Move m: list) {
            first = m;
            break;
        }
        tt.store(k, first, 10, 6, Bound::Lower);
        tt.store(k, Move::none(), 11, 7, Bound::Upper);
        check(tt.probe(k, e) && e.move == first, "tt: move kept when stored without a move");

        tt.clear();
        check(!tt.probe(k, e), "tt: clear empties the table");
    }
} // namespace

int main() {
    init_attacks();

    test_fen();
    test_sliders();
    test_move_counts();
    test_make_unmake();
    test_perft();
    test_zobrist();
    test_eval();
    test_uci();
    test_search();
    test_alphabeta();
    test_search_iterative();
    test_tt();

    std::cout << (gFailures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED") << '\n';
    return gFailures == 0 ? 0 : 1;
}

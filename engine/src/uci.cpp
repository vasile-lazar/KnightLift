#include "uci.hpp"

#include <istream>
#include <ostream>
#include <chrono>
#include <sstream>
#include <string>

#include "movegen.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "version.hpp"
#include "search.hpp"

namespace KnightLift {

namespace {
    std::string score_string(int score) {
        if (score > kMateBound) return "mate " + std::to_string((kMateScore - score + 1) / 2);
        if (score < -kMateBound) return "mate -" + std::to_string((kMateScore + score + 1) / 2);
        return "cp " + std::to_string(score);
    }

class UciEngine {
public:
    UciEngine(std::istream& in, std::ostream& out) : in_(in), out_(out) {
        pos_.set_from_fen(kStartFen);
    }

    void run() {
        std::string line;
        while (std::getline(in_, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            std::istringstream args(line);
            std::string command;
            args >> command;

            if (command == "quit") {
                break;
            } else if (command == "uci") {
                out_ << "id name " << kEngineName << ' ' << kVersion << '\n'
                     << "id author " << kAuthor << '\n'
                     << "uciok" << std::endl;
            } else if (command == "isready") {
                out_ << "readyok" << std::endl;
            } else if (command == "ucinewgame") {
                pos_.set_from_fen(kStartFen);
            } else if (command == "position") {
                handle_position(args);
            } else if (command == "go") {
                handle_go(args);
            } else if (command == "d") {
                out_ << pos_.to_string() << std::endl;
            }
            // Anything else (setoption, debug, unknown) is ignored for now.
        }
    }

private:
    Move parse_move(std::string_view text) {
        MoveList legal;
        generate_legal(pos_, legal);
        for (const Move m : legal) {
            if (move_to_uci(m) == text) return m;
        }
        return Move::none();
    }

    void handle_position(std::istringstream& args) {
        std::string token;
        args >> token;

        Position next;
        if (token == "startpos") {
            next.set_from_fen(kStartFen);
        } else if (token == "fen") {
            std::string fen;
            while (args >> token && token != "moves") {
                if (!fen.empty()) fen += ' ';
                fen += token;
            }
            if (!next.set_from_fen(fen)) return;  // keep the old position
        } else {
            return;
        }
        pos_ = next;

        while (args >> token) {
            if (token == "moves") continue;
            const Move m = parse_move(token);
            if (m.is_none()) break;  // illegal or malformed move: stop here
            pos_.make_move(m);
        }
    }

        void handle_go(std::istringstream& args) {
        int depth = 4;  // fixed depth until time management exists
        bool alphaBeta = true;
        std::string token;
        while (args >> token) {
            if (token == "perft") {
                int n = 1;
                args >> n;
                const std::uint64_t total = perft_divide(pos_, n, out_);
                out_ << "\nNodes searched: " << total << std::endl;
                return;
            }
            if (token == "depth") args >> depth;
            if (token == "minimax") alphaBeta = false;
        }
        run_search(depth, alphaBeta);
    }

        void run_search(int depth, bool alphaBeta) {
        const auto start = std::chrono::steady_clock::now();
        const SearchResult r = search(pos_, depth, alphaBeta);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        if (r.best.is_none()) {
            out_ << "bestmove 0000" << std::endl;
            return;
        }

        out_ << "info depth " << depth << " score " << score_string(r.score)
             << " nodes " << r.nodes << " time " << ms;
        if (ms > 0) out_ << " nps " << r.nodes * 1000 / static_cast<std::uint64_t>(ms);
        out_ << '\n' << "bestmove " << move_to_uci(r.best) << std::endl;
    }

    std::istream& in_;
    std::ostream& out_;
    Position pos_;
};

}  // namespace

void uci_loop(std::istream& in, std::ostream& out) {
    UciEngine engine(in, out);
    engine.run();
}

}  // namespace KnightLift
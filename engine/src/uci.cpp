#include "uci.hpp"

#include <istream>
#include <ostream>
#include <random>
#include <sstream>
#include <string>

#include "movegen.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "version.hpp"

namespace KnightLift {

namespace {

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
        std::string token;
        args >> token;

        if (token == "perft") {
            int depth = 1;
            args >> depth;
            const std::uint64_t total = perft_divide(pos_, depth, out_);
            out_ << "\nNodes searched: " << total << std::endl;
            return;
        }

        // Placeholder until the search exists: play a random legal move.
        MoveList legal;
        generate_legal(pos_, legal);
        if (legal.empty()) {
            out_ << "bestmove 0000" << std::endl;
            return;
        }
        std::uniform_int_distribution<std::size_t> pick(0, legal.size() - 1);
        out_ << "bestmove " << move_to_uci(legal[pick(rng_)]) << std::endl;
    }

    std::istream& in_;
    std::ostream& out_;
    Position pos_;
    std::mt19937 rng_{std::random_device{}()};
};

}  // namespace

void uci_loop(std::istream& in, std::ostream& out) {
    UciEngine engine(in, out);
    engine.run();
}

}  // namespace KnightLift
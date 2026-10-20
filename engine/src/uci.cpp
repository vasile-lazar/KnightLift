#include "uci.hpp"

#include <algorithm>
#include <atomic>
#include <istream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>

#include "movegen.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "search.hpp"
#include "version.hpp"

namespace KnightLift
{
    namespace
    {
        struct GoParams
        {
            int depth = -1;
            std::int64_t movetime = -1;
            std::int64_t wtime = -1;
            std::int64_t btime = -1;
            std::int64_t winc = 0;
            std::int64_t binc = 0;
            int movestogo = 0;
            bool infinite = false;
        };

        std::string score_string(int score)
        {
            if (score > kMateBound) return "mate " + std::to_string((kMateScore - score + 1) / 2);
            if (score < -kMateBound) return "mate -" + std::to_string((kMateScore + score + 1) / 2);
            return "cp " + std::to_string(score);
        }

        SearchLimits make_limits(const GoParams& go, Color side)
        {
            constexpr std::int64_t kMarginMs = 30;
            SearchLimits limits;

            const std::int64_t myTime = (side == Color::White) ? go.wtime : go.btime;
            const std::int64_t myInc = (side == Color::White) ? go.winc : go.binc;

            if (go.depth > 0) limits.maxDepth = std::min(go.depth, kMaxDepth);

            if (go.movetime >= 0)
            {
                limits.optimumMs = limits.hardMs = std::max<std::int64_t>(1, go.movetime - kMarginMs);
            }
            else if (!go.infinite && myTime >= 0)
            {
                const int movesToGo = go.movestogo > 0 ? std::min(go.movestogo, 50) : 30;
                limits.optimumMs = std::max<std::int64_t>(1, myTime / movesToGo + myInc * 3 / 4);
                limits.hardMs =
                    std::max<std::int64_t>(1, std::min(limits.optimumMs * 3, myTime - kMarginMs));
            }
            else if (!go.infinite && go.depth < 0)
            {
                limits.maxDepth = 6; // plain "go" with no limits
            }
            return limits;
        }

        class UciEngine
        {
        public:
            UciEngine(std::istream& in, std::ostream& out) : in_(in), out_(out)
            {
                pos_.set_from_fen(kStartFen);
            }

            ~UciEngine() { finish_search(true); }

            void run()
            {
                std::string line;
                bool quit = false;
                while (!quit && std::getline(in_, line))
                {
                    if (!line.empty() && line.back() == '\r') line.pop_back();

                    std::istringstream args(line);
                    std::string command;
                    args >> command;

                    if (command == "quit")
                    {
                        quit = true;
                    }
                    else if (command == "uci")
                    {
                        std::ostringstream text;
                        text << "id name " << kEngineName << ' ' << kVersion << '\n'
                            << "id author " << kAuthor << "\nuciok";
                        send(text.str());
                    }
                    else if (command == "isready")
                    {
                        send("readyok");
                    }
                    else if (command == "stop")
                    {
                        finish_search(true);
                    }
                    else if (command == "ucinewgame")
                    {
                        finish_search(true);
                        pos_.set_from_fen(kStartFen);
                    }
                    else if (command == "position")
                    {
                        finish_search(true);
                        handle_position(args);
                    }
                    else if (command == "go")
                    {
                        handle_go(args);
                    }
                    else if (command == "d")
                    {
                        finish_search(true);
                        send(pos_.to_string());
                    }
                    // Anything else (setoption, debug, unknown) is ignored for now.
                }

                // End of input: abort on "quit" or an infinite search, otherwise let it finish.
                finish_search(quit || infinite_);
            }

        private:
            void send(const std::string& text)
            {
                std::lock_guard<std::mutex> lock(outMutex_);
                out_ << text << std::endl;
            }

            // Waits for the search thread; aborts it first if requested.
            void finish_search(bool abort)
            {
                if (abort) stop_ = true;
                if (searchThread_.joinable()) searchThread_.join();
            }

            Move parse_move(std::string_view text)
            {
                MoveList legal;
                generate_legal(pos_, legal);
                for (const Move m : legal)
                {
                    if (move_to_uci(m) == text) return m;
                }
                return Move::none();
            }

            void handle_position(std::istringstream& args)
            {
                std::string token;
                args >> token;

                Position next;
                if (token == "startpos")
                {
                    next.set_from_fen(kStartFen);
                }
                else if (token == "fen")
                {
                    std::string fen;
                    while (args >> token && token != "moves")
                    {
                        if (!fen.empty()) fen += ' ';
                        fen += token;
                    }
                    if (!next.set_from_fen(fen)) return; // keep the old position
                }
                else
                {
                    return;
                }
                pos_ = next;

                while (args >> token)
                {
                    if (token == "moves") continue;
                    const Move m = parse_move(token);
                    if (m.is_none()) break; // illegal or malformed move: stop here
                    pos_.make_move(m);
                }
            }

            void handle_go(std::istringstream& args)
            {
                finish_search(true);

                GoParams go;
                std::string token;
                while (args >> token)
                {
                    if (token == "perft")
                    {
                        int depth = 1;
                        args >> depth;
                        const std::uint64_t total = perft_divide(pos_, depth, out_);
                        send("\nNodes searched: " + std::to_string(total));
                        return;
                    }
                    else if (token == "depth")
                    {
                        args >> go.depth;
                    }
                    else if (token == "movetime")
                    {
                        args >> go.movetime;
                    }
                    else if (token == "wtime")
                    {
                        args >> go.wtime;
                    }
                    else if (token == "btime")
                    {
                        args >> go.btime;
                    }
                    else if (token == "winc")
                    {
                        args >> go.winc;
                    }
                    else if (token == "binc")
                    {
                        args >> go.binc;
                    }
                    else if (token == "movestogo")
                    {
                        args >> go.movestogo;
                    }
                    else if (token == "infinite")
                    {
                        go.infinite = true;
                    }
                }

                infinite_ = go.infinite;
                stop_ = false;
                const SearchLimits limits = make_limits(go, pos_.side_to_move());
                searchThread_ = std::thread([this, limits] { run_search(limits); });
            }

            void run_search(SearchLimits limits)
            {
                const SearchResult result = search_iterative(
                    pos_, limits, stop_, [this](const SearchInfo& info) { send_info(info); });
                send(result.best.is_none() ? "bestmove 0000" : "bestmove " + move_to_uci(result.best));
            }

            void send_info(const SearchInfo& info)
            {
                std::ostringstream line;
                line << "info depth " << info.depth << " score " << score_string(info.score)
                    << " nodes " << info.nodes << " time " << info.timeMs;
                if (info.timeMs > 0)
                {
                    line << " nps " << info.nodes * 1000 / static_cast<std::uint64_t>(info.timeMs);
                }
                line << " pv " << move_to_uci(info.best);
                send(line.str());
            }

            std::istream& in_;
            std::ostream& out_;
            Position pos_;
            std::atomic<bool> stop_{false};
            bool infinite_ = false;
            std::mutex outMutex_;
            std::thread searchThread_;
        };
    } // namespace

    void uci_loop(std::istream& in, std::ostream& out)
    {
        UciEngine engine(in, out);
        engine.run();
    }
} // namespace KnightLift

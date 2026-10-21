#include "search.hpp"

#include <chrono>
#include <utility>

#include "eval.hpp"
#include "movegen.hpp"

namespace KnightLift {
    namespace {
        // Most Valuable Victim / Least Valuable Attacker: higher scores are searched first.
        int mvv_lva(const Position &pos, Move m) {
            if (m.is_promotion()) return 60; // above every capture score (max is 49)
            const int victim = (m.flag() == MoveFlag::EnPassant)
                                   ? 0
                                   : static_cast<int>(to_index(type_of(pos.piece_on(m.to()))));
            const int attacker = static_cast<int>(to_index(type_of(pos.piece_on(m.from()))));
            return 10 * (victim + 1) - (attacker + 1);
        }

        class Searcher {
        public:
            Searcher(Position &pos, const SearchLimits *limits, const std::atomic<bool> *stop)
                : pos_(pos), limits_(limits), stop_(stop), start_(std::chrono::steady_clock::now()) {
            }

            bool aborted() const { return aborted_; }
            std::uint64_t nodes() const { return nodes_; }
            void enable_stop_checks() { checkStop_ = true; }

            std::int64_t elapsed_ms() const {
                const auto elapsed = std::chrono::steady_clock::now() - start_;
                return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
            }

            int quiescence(int alpha, int beta, bool prune) {
                ++nodes_;
                if (checkStop_ && (nodes_ & 2047) == 0 && should_stop()) aborted_ = true;
                if (aborted_) return 0;

                const int standPat = evaluate(pos_);
                if (prune) {
                    if (standPat >= beta) return standPat;
                    if (standPat > alpha) alpha = standPat;
                }

                MoveList all;
                generate_pseudo_legal(pos_, all);
                MoveList captures;
                for (const Move m : all) {
                    if (m.is_capture() || m.is_promotion()) captures.push(m);
                }

                int scores[kMaxMoves];
                for (std::size_t i = 0; i < captures.size(); ++i) {
                    scores[i] = mvv_lva(pos_, captures[i]);
                }

                int best = standPat;
                for (std::size_t i = 0; i < captures.size(); ++i) {
                    // Selection sort step: bring the best remaining capture to position i.
                    std::size_t top = i;
                    for (std::size_t j = i + 1; j < captures.size(); ++j) {
                        if (scores[j] > scores[top]) top = j;
                    }
                    std::swap(scores[i], scores[top]);
                    captures.swap(i, top);
                    const Move m = captures[i];

                    pos_.make_move(m);
                    if (pos_.king_left_in_check()) {
                        pos_.unmake_move(m);
                        continue;
                    }
                    const int score = prune ? -quiescence(-beta, -alpha, true)
                                            : -quiescence(-kInfinity, kInfinity, false);
                    pos_.unmake_move(m);
                    if (aborted_) return 0;

                    if (score > best) {
                        best = score;
                        if (prune && score > alpha) alpha = score;
                    }
                    if (prune && alpha >= beta) break;
                }
                return best;
            }

            // Plain minimax: reference implementation, never aborted.
            int negamax(int depth, int ply) {
                ++nodes_;
                if (depth == 0) return quiescence(-kInfinity, kInfinity, true);
                MoveList list;
                generate_pseudo_legal(pos_, list);

                int best = -kInfinity;
                bool hasLegalMove = false;
                for (const Move m: list) {
                    pos_.make_move(m);
                    if (pos_.king_left_in_check()) {
                        pos_.unmake_move(m);
                        continue;
                    }
                    hasLegalMove = true;
                    const int score = -negamax(depth - 1, ply + 1);
                    pos_.unmake_move(m);
                    if (score > best) best = score;
                }

                if (!hasLegalMove) return pos_.in_check() ? -kMateScore + ply : 0;
                return best;
            }

            int alphabeta(int depth, int ply, int alpha, int beta) {
                ++nodes_;
                if (checkStop_ && (nodes_ & 2047) == 0 && should_stop()) aborted_ = true;
                if (aborted_) return 0;
                if (depth == 0) return quiescence(alpha, beta, true);

                MoveList list;
                generate_pseudo_legal(pos_, list);

                int best = -kInfinity;
                bool hasLegalMove = false;
                for (const Move m: list) {
                    pos_.make_move(m);
                    if (pos_.king_left_in_check()) {
                        pos_.unmake_move(m);
                        continue;
                    }
                    hasLegalMove = true;
                    const int score = -alphabeta(depth - 1, ply + 1, -beta, -alpha);
                    pos_.unmake_move(m);
                    if (aborted_) return 0;

                    if (score > best) {
                        best = score;
                        if (score > alpha) alpha = score;
                    }
                    if (alpha >= beta) break;
                }

                if (!hasLegalMove) return pos_.in_check() ? -kMateScore + ply : 0;
                return best;
            }

            // `first` (if not none) is searched first, e.g. the best move of the previous iteration.
            SearchResult search_root(int depth, Move first, bool useAlphaBeta) {
                MoveList list;
                generate_pseudo_legal(pos_, list);
                if (!first.is_none()) list.move_to_front(first);

                SearchResult result;
                result.score = -kInfinity;
                for (const Move m: list) {
                    pos_.make_move(m);
                    if (pos_.king_left_in_check()) {
                        pos_.unmake_move(m);
                        continue;
                    }
                    const int score = useAlphaBeta
                                          ? -alphabeta(depth - 1, 1, -kInfinity, -result.score)
                                          : -negamax(depth - 1, 1);
                    pos_.unmake_move(m);
                    if (aborted_) return result;

                    if (score > result.score) {
                        result.score = score;
                        result.best = m;
                    }
                }

                if (result.best.is_none()) result.score = pos_.in_check() ? -kMateScore : 0;
                result.nodes = nodes_;
                return result;
            }

        private:
            bool should_stop() const {
                if (stop_ != nullptr && stop_->load(std::memory_order_relaxed)) return true;
                return limits_ != nullptr && limits_->hardMs >= 0 && elapsed_ms() >= limits_->hardMs;
            }

            Position &pos_;
            const SearchLimits *limits_;
            const std::atomic<bool> *stop_;
            std::chrono::steady_clock::time_point start_;
            std::uint64_t nodes_ = 0;
            bool aborted_ = false;
            bool checkStop_ = false;
        };
    } // namespace

    SearchResult search(Position &pos, int depth, bool useAlphaBeta) {
        Searcher searcher(pos, nullptr, nullptr);
        return searcher.search_root(depth < 1 ? 1 : depth, Move::none(), useAlphaBeta);
    }

    SearchResult search_iterative(Position &pos, const SearchLimits &limits,
                                  const std::atomic<bool> &stop, const InfoCallback &onInfo) {
        Searcher searcher(pos, &limits, &stop);
        SearchResult best;

        for (int depth = 1; depth <= limits.maxDepth && depth <= kMaxDepth; ++depth) {
            const SearchResult r = searcher.search_root(depth, best.best, true);
            if (searcher.aborted()) break; // discard the unfinished iteration

            best = r;
            searcher.enable_stop_checks(); // depth 1 always completes
            if (onInfo) {
                onInfo({depth, r.score, searcher.nodes(), searcher.elapsed_ms(), r.best});
            }

            if (r.best.is_none()) break; // no legal moves
            if (r.score > kMateBound || r.score < -kMateBound) break; // forced mate proven
            if (limits.optimumMs >= 0 && searcher.elapsed_ms() >= limits.optimumMs / 2) break;
        }

        best.nodes = searcher.nodes();
        return best;
    }
} // namespace KnightLift

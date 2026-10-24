#include <chrono>
#include <utility>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cmath>

#include "eval.hpp"
#include "movegen.hpp"
#include "tt.hpp"
#include "search.hpp"
namespace KnightLift {
    namespace {
        TranspositionTable gTT(16);
        bool gNullMove = true;
        constexpr int kHistoryMax = 16384;
        using HistoryTable = std::array<std::array<int, kNumSquares>, kNumSquares>;

        // Mate scores are stored relative to the node, not to the root.
        int score_to_tt(int score, int ply) {
            if (score > kMateBound) return score + ply;
            if (score < -kMateBound) return score - ply;
            return score;
        }

        int score_from_tt(int score, int ply) {
            if (score > kMateBound) return score - ply;
            if (score < -kMateBound) return score + ply;
            return score;
        }

        // Most Valuable Victim / Least Valuable Attacker: higher scores are searched first.
        int mvv_lva(const Position &pos, Move m) {
            if (m.is_promotion()) return 60; // above every capture score (max is 49)
            const int victim = (m.flag() == MoveFlag::EnPassant)
                                   ? 0
                                   : static_cast<int>(to_index(type_of(pos.piece_on(m.to()))));
            const int attacker = static_cast<int>(to_index(type_of(pos.piece_on(m.from()))));
            return 10 * (victim + 1) - (attacker + 1);
        }

        int score_move(const Position &pos, Move m, Move first, Move killer0, Move killer1,
                       const HistoryTable &history) {
            if (m == first) return 2'000'000;
            if (m.is_capture() || m.is_promotion()) return 1'000'000 + mvv_lva(pos, m);
            if (m == killer0) return 900'000;
            if (m == killer1) return 800'000;
            return history[to_index(m.from())][to_index(m.to())];
        }

        // Selection-sort step: brings the best remaining move to index i.
        void pick_best(MoveList &list, int *scores, std::size_t i) {
            std::size_t top = i;
            for (std::size_t j = i + 1; j < list.size(); ++j) {
                if (scores[j] > scores[top]) top = j;
            }
            std::swap(scores[i], scores[top]);
            list.swap(i, top);
        }

        struct LmrTable {
            std::array<std::array<int, 64>, 64> r{};
            LmrTable() {
                for (int d = 1; d < 64; ++d)
                    for (int m = 1; m < 64; ++m)
                        r[d][m] = static_cast<int>(0.75 + std::log(d) * std::log(m) / 2.25);
            }
        };
        const LmrTable gLmr;

        class Searcher {
        public:
            Searcher(Position &pos, const SearchLimits *limits, const std::atomic<bool> *stop,
                     TranspositionTable *tt = nullptr)
                : pos_(pos), limits_(limits), stop_(stop), tt_(tt),
                  start_(std::chrono::steady_clock::now()) {
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
                for (const Move m: all) {
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
                    const int score = prune
                                          ? -quiescence(-beta, -alpha, true)
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
                if (depth <= 0) return quiescence(-kInfinity, kInfinity, true);
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

            int alphabeta(int depth, int ply, int alpha, int beta, bool allowNull = true) {
                ++nodes_;
                if (checkStop_ && (nodes_ & 2047) == 0 && should_stop()) aborted_ = true;
                if (aborted_) return 0;
                if (drawDetect_ && ply > 0) {
                    if (pos_.is_repetition(ply) || pos_.insufficient_material() ||
                        (pos_.halfmove_clock() >= 100 && !pos_.in_check())) {
                        return 0;
                        }
                }
                if (depth <= 0) return quiescence(alpha, beta, true);

                const int alphaOrig = alpha;
                Move ttMove = Move::none();
                if (tt_ != nullptr) {
                    TTEntry e;
                    if (tt_->probe(pos_.key(), e)) {
                        ttMove = e.move;
                        if (e.depth >= depth) {
                            const int s = score_from_tt(e.score, ply);
                            if (e.bound() == Bound::Exact ||
                                (e.bound() == Bound::Lower && s >= beta) ||
                                (e.bound() == Bound::Upper && s <= alpha)) {
                                return s;
                            }
                        }
                    }
                }

                const Color us = pos_.side_to_move();
                const Bitboard nonPawnMaterial =
                        pos_.pieces(us) & ~(pos_.pieces(us, PieceType::Pawn) |
                                            pos_.pieces(us, PieceType::King));
                if (nullMove_ && allowNull && depth >= 3 && nonPawnMaterial != 0 &&
                    !pos_.in_check() && evaluate(pos_) >= beta) {
                    const int nullDepth = depth - 1 - (2 + depth / 6);
                    pos_.make_null_move();
                    const int nullScore = nullDepth <= 0
                                              ? -quiescence(-beta, -beta + 1, true)
                                              : -alphabeta(nullDepth, ply + 1, -beta, -beta + 1, false);
                    pos_.unmake_null_move();
                    if (aborted_) return 0;
                    if (nullScore >= beta) return nullScore > kMateBound ? beta : nullScore;
                    }

                MoveList list;
                generate_pseudo_legal(pos_, list);

                int scores[kMaxMoves];
                for (std::size_t i = 0; i < list.size(); ++i) {
                    scores[i] = score_move(pos_, list[i], ttMove, killers_[ply][0],
                                           killers_[ply][1], history_[to_index(us)]);
                }

                Move quiets[kMaxMoves];
                int numQuiets = 0;
                int best = -kInfinity;
                Move bestMove = Move::none();
                bool hasLegalMove = false;
                const bool inCheck = pos_.in_check();
                int legalCount = 0;
                for (std::size_t i = 0; i < list.size(); ++i) {
                    pick_best(list, scores, i);
                    const Move m = list[i];
                    const bool quiet = !m.is_capture() && !m.is_promotion();
                    pos_.make_move(m);
                    if (pos_.king_left_in_check()) {
                        pos_.unmake_move(m);
                        continue;
                    }
                    int score;
                    if (legalCount == 0) {
                        score = -alphabeta(depth - 1, ply + 1, -beta, -alpha);
                    } else {
                        int reduction = 0;
                        if (lmr_ && depth >= 3 && legalCount >= 3 && quiet && !inCheck &&
                            !pos_.in_check()) {
                            reduction = std::min(gLmr.r[std::min(depth, 63)][std::min(legalCount, 63)],
                                                 depth - 2);
                            }
                        score = -alphabeta(depth - 1 - reduction, ply + 1, -alpha - 1, -alpha);
                        if (!aborted_ && reduction > 0 && score > alpha)
                            score = -alphabeta(depth - 1, ply + 1, -alpha - 1, -alpha);
                        if (!aborted_ && score > alpha && score < beta)
                            score = -alphabeta(depth - 1, ply + 1, -beta, -alpha);
                    }
                    ++legalCount;
                    hasLegalMove = true;
                    pos_.unmake_move(m);
                    if (aborted_) return 0;

                    if (score > best) {
                        best = score;
                        bestMove = m;
                        if (score > alpha) alpha = score;
                    }
                    if (alpha >= beta) {
                        if (quiet) {
                            store_killer(ply, m);
                            const int bonus = std::min(depth * depth, 1600);
                            update_history(us, m, bonus);
                            for (int q = 0; q < numQuiets; ++q) update_history(us, quiets[q], -bonus);
                        }
                        break;
                    }
                    if (quiet) quiets[numQuiets++] = m;
                }

                if (!hasLegalMove) return pos_.in_check() ? -kMateScore + ply : 0;

                if (tt_ != nullptr) {
                    const Bound bound = best >= beta
                                            ? Bound::Lower
                                            : best > alphaOrig
                                                  ? Bound::Exact
                                                  : Bound::Upper;
                    // A fail-low has no meaningful best move.
                    tt_->store(pos_.key(), bound == Bound::Upper ? Move::none() : bestMove,
                               score_to_tt(best, ply), depth, bound);
                }
                return best;
            }

            // `first` (if not none) is searched first, e.g. the best move of the previous iteration.
            SearchResult search_root(int depth, Move first, bool useAlphaBeta) {
                MoveList list;
                bool searchedAny = false;
                const Color us = pos_.side_to_move();
                generate_pseudo_legal(pos_, list);
                if (first.is_none() && tt_ != nullptr) {
                    TTEntry e;
                    if (tt_->probe(pos_.key(), e)) first = e.move;
                }


                int scores[kMaxMoves];
                for (std::size_t i = 0; i < list.size(); ++i) {
                    scores[i] = score_move(pos_, list[i], first, Move::none(), Move::none(),
                                           history_[to_index(us)]);
                }

                SearchResult result;
                result.score = -kInfinity;
                for (std::size_t i = 0; i < list.size(); ++i) {
                    pick_best(list, scores, i);
                    const Move m = list[i];
                    pos_.make_move(m);
                    if (pos_.king_left_in_check()) {
                        pos_.unmake_move(m);
                        continue;
                    }
                    int score;
                    if (!useAlphaBeta) {
                        score = -negamax(depth - 1, 1);
                    } else if (!searchedAny) {
                        score = -alphabeta(depth - 1, 1, -kInfinity, kInfinity);
                    } else {
                        score = -alphabeta(depth - 1, 1, -result.score - 1, -result.score);
                        if (!aborted_ && score > result.score)
                            score = -alphabeta(depth - 1, 1, -kInfinity, -result.score);
                    }
                    searchedAny = true;
                    pos_.unmake_move(m);
                    if (aborted_) return result;

                    if (score > result.score) {
                        result.score = score;
                        result.best = m;
                    }
                }

                if (result.best.is_none()) {
                    result.score = pos_.in_check() ? -kMateScore : 0;
                } else if (tt_ != nullptr) {
                    tt_->store(pos_.key(), result.best, score_to_tt(result.score, 0), depth,
                               Bound::Exact);
                }
                result.nodes = nodes_;
                return result;
            }

            void enable_null_move(bool on) { nullMove_ = on; }
            void enable_draw_detection(bool on) { drawDetect_ = on; }
            void enable_lmr(bool on) { lmr_ = on; }

        private:
            bool should_stop() const {
                if (stop_ != nullptr && stop_->load(std::memory_order_relaxed)) return true;
                return limits_ != nullptr && limits_->hardMs >= 0 && elapsed_ms() >= limits_->hardMs;
            }

            void store_killer(int ply, Move m) {
                if (killers_[ply][0] == m) return;
                killers_[ply][1] = killers_[ply][0];
                killers_[ply][0] = m;
            }

            void update_history(Color c, Move m, int bonus) {
                int &h = history_[to_index(c)][to_index(m.from())][to_index(m.to())];
                h += bonus - h * std::abs(bonus) / kHistoryMax;
            }

            Position &pos_;
            const SearchLimits *limits_;
            const std::atomic<bool> *stop_;
            TranspositionTable *tt_;
            std::chrono::steady_clock::time_point start_;
            std::uint64_t nodes_ = 0;
            bool aborted_ = false;
            bool checkStop_ = false;
            std::array<std::array<Move, 2>, kMaxDepth + 2> killers_{};
            std::array<HistoryTable, kNumColors> history_{};
            bool nullMove_ = false;
            bool drawDetect_ = false;
            bool lmr_ = false;
        };
    } // namespace

    void tt_resize(std::size_t megabytes) { gTT.resize(megabytes); }
    void tt_clear() { gTT.clear(); }
    int tt_hashfull() { return gTT.hashfull(); }

    SearchResult search(Position &pos, int depth, bool useAlphaBeta) {
        Searcher searcher(pos, nullptr, nullptr);
        return searcher.search_root(depth < 1 ? 1 : depth, Move::none(), useAlphaBeta);
    }

    int quiescence_score(Position &pos) {
        Searcher searcher(pos, nullptr, nullptr);
        return searcher.quiescence(-kInfinity, kInfinity, true);
    }

    bool quiescence_score_within(Position &pos, int maxMs, int &score) {
        SearchLimits limits;
        limits.hardMs = maxMs;
        Searcher searcher(pos, &limits, nullptr);
        searcher.enable_stop_checks();
        score = searcher.quiescence(-kInfinity, kInfinity, true);
        return !searcher.aborted();
    }

    SearchResult search_iterative(Position &pos, const SearchLimits &limits,
                                  const std::atomic<bool> &stop, const InfoCallback &onInfo) {
        Searcher searcher(pos, &limits, &stop, &gTT);
        searcher.enable_null_move(gNullMove);
        searcher.enable_lmr(true);
        searcher.enable_draw_detection(true);
        SearchResult best;

        for (int depth = 1; depth <= limits.maxDepth && depth <= kMaxDepth; ++depth) {
            const SearchResult r = searcher.search_root(depth, best.best, true);
            if (searcher.aborted()) break; // discard the unfinished iteration

            best = r;
            searcher.enable_stop_checks(); // depth 1 always completes
            if (onInfo) {
                SearchInfo info{depth, r.score, searcher.nodes(), searcher.elapsed_ms(), r.best};
                info.hashfull = gTT.hashfull();
                onInfo(info);
            }

            if (r.best.is_none()) break; // no legal moves
            if (r.score > kMateBound || r.score < -kMateBound) break; // forced mate proven
            if (limits.optimumMs >= 0 && searcher.elapsed_ms() >= limits.optimumMs / 2) break;
        }

        best.nodes = searcher.nodes();
        return best;
    }

    void set_null_move(bool enabled) { gNullMove = enabled; }
} // namespace KnightLift

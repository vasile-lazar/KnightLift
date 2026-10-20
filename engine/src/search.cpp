#include "search.hpp"

#include "eval.hpp"
#include "movegen.hpp"

namespace KnightLift {

    namespace {

        class Searcher {
        public:
            explicit Searcher(Position& pos) : pos_(pos) {}

            int negamax(int depth, int ply) {
                ++nodes_;
                if (depth == 0) return evaluate(pos_);

                MoveList list;
                generate_pseudo_legal(pos_, list);

                int best = -kInfinity;
                bool hasLegalMove = false;
                for (const Move m : list) {
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

            std::uint64_t nodes() const { return nodes_; }

        private:
            Position& pos_;
            std::uint64_t nodes_ = 0;
        };

    }  // namespace

    SearchResult search(Position& pos, int depth) {
        if (depth < 1) depth = 1;

        Searcher searcher(pos);
        SearchResult result;
        result.score = -kInfinity;

        MoveList list;
        generate_pseudo_legal(pos, list);
        for (const Move m : list) {
            pos.make_move(m);
            if (pos.king_left_in_check()) {
                pos.unmake_move(m);
                continue;
            }
            const int score = -searcher.negamax(depth - 1, 1);
            pos.unmake_move(m);
            if (score > result.score) {
                result.score = score;
                result.best = m;
            }
        }

        if (result.best.is_none()) result.score = pos.in_check() ? -kMateScore : 0;
        result.nodes = searcher.nodes();
        return result;
    }

}  // namespace KnightLift
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <cstddef>

#include "move.hpp"
#include "position.hpp"

namespace KnightLift
{
    constexpr int kInfinity = 32001;
    constexpr int kMateScore = 32000;
    constexpr int kMateBound = kMateScore - 1000; // |score| above this encodes a mate
    constexpr int kMaxDepth = 64;

    struct SearchResult
    {
        Move best = Move::none();
        int score = 0;
        std::uint64_t nodes = 0;
    };

    struct SearchLimits
    {
        int maxDepth = kMaxDepth;
        std::int64_t optimumMs = -1; // soft limit: no new iteration after half of this
        std::int64_t hardMs = -1; // hard limit: abort the search (-1 = none)
    };

    struct SearchInfo
    {
        int depth = 0;
        int score = 0;
        std::uint64_t nodes = 0;
        std::int64_t timeMs = 0;
        Move best = Move::none();
        int hashfull = 0;
    };

    using InfoCallback = std::function<void(const SearchInfo&)>;

    // Fixed-depth search, never interrupted. useAlphaBeta = false is plain minimax.
    SearchResult search(Position& pos, int depth, bool useAlphaBeta = true);

    int quiescence_score(Position &pos);
    bool quiescence_score_within(Position &pos, int maxMs, int &score);

    // Iterative deepening with time limits; stops when `stop` becomes true.
    // Calls onInfo (if set) after every completed depth.
    SearchResult search_iterative(Position& pos, const SearchLimits& limits,
                                  const std::atomic<bool>& stop, const InfoCallback& onInfo);

    // Transposition table control (used by UCI; call only while no search is running).
    void tt_resize(std::size_t megabytes);
    void tt_clear();
    int tt_hashfull();

    void set_null_move(bool enabled);
    void set_lmr(bool enabled);
    void set_aspiration(bool enabled);
    void set_history(bool enabled);
} // namespace KnightLift

#include "tt.hpp"

#include <algorithm>

namespace KnightLift {

    void TranspositionTable::resize(std::size_t megabytes) {
        std::size_t n = megabytes * 1024 * 1024 / sizeof(TTEntry);
        if (n < 1) n = 1;
        table_.assign(n, TTEntry{});
        generation_ = 0;
    }

    void TranspositionTable::clear() {
        std::fill(table_.begin(), table_.end(), TTEntry{});
        generation_ = 0;
    }

    std::size_t TranspositionTable::index(Key key) const {
#if defined(__SIZEOF_INT128__)
        __extension__ typedef unsigned __int128 u128;
        return static_cast<std::size_t>((static_cast<u128>(key) * table_.size()) >> 64);
#else
        return static_cast<std::size_t>(key % table_.size());
#endif
    }

    bool TranspositionTable::probe(Key key, TTEntry &out) const {
        const TTEntry &e = table_[index(key)];
        if (e.bound() == Bound::None || e.key32 != static_cast<std::uint32_t>(key)) return false;
        out = e;
        return true;
    }

    void TranspositionTable::store(Key key, Move move, int score, int depth, Bound bound) {
        TTEntry &e = table_[index(key)];
        const std::uint32_t k32 = static_cast<std::uint32_t>(key);

        // Keep the old best move if this store has none for the same position.
        if (!move.is_none() || e.key32 != k32) e.move = move;

        if (bound == Bound::Exact || e.key32 != k32 || depth + 4 > e.depth ||
            e.generation() != generation_) {
            e.key32 = k32;
            e.score = static_cast<std::int16_t>(score);
            e.depth = static_cast<std::uint8_t>(std::clamp(depth, 0, 255));
            e.genBound = static_cast<std::uint8_t>((generation_ << 2) | static_cast<std::uint8_t>(bound));
        }
    }

    int TranspositionTable::hashfull() const {
        const std::size_t n = std::min<std::size_t>(1000, table_.size());
        int used = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (table_[i].bound() != Bound::None && table_[i].generation() == generation_) ++used;
        }
        return static_cast<int>(used * 1000 / n);
    }

} // namespace KnightLift
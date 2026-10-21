#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "move.hpp"
#include "zobrist.hpp"

namespace KnightLift {

    enum class Bound : std::uint8_t { None = 0, Upper = 1, Lower = 2, Exact = 3 };

    struct TTEntry {
        std::uint32_t key32 = 0;
        Move move = Move::none();
        std::int16_t score = 0;
        std::uint8_t depth = 0;
        std::uint8_t genBound = 0; // generation (upper 6 bits) | bound (lower 2 bits)

        Bound bound() const { return static_cast<Bound>(genBound & 3); }
        std::uint8_t generation() const { return genBound >> 2; }
    };

    class TranspositionTable {
    public:
        explicit TranspositionTable(std::size_t megabytes = 16) { resize(megabytes); }

        void resize(std::size_t megabytes); // reallocates and clears
        void clear();
        void new_search() { generation_ = (generation_ + 1) & 63; }

        // Copies the entry into `out` and returns true if this position is stored.
        bool probe(Key key, TTEntry &out) const;
        void store(Key key, Move move, int score, int depth, Bound bound);

        int hashfull() const; // permille of sampled slots from the current search

    private:
        std::size_t index(Key key) const;

        std::vector<TTEntry> table_;
        std::uint8_t generation_ = 0;
    };

} // namespace KnightLift
#pragma once

#include <array>
#include <cstdint>

#include "types.hpp"

namespace KnightLift {

    using Key = std::uint64_t;

    namespace Zobrist {

        namespace detail {
            constexpr Key splitmix64(std::uint64_t &state) {
                std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
                z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
                return z ^ (z >> 31);
            }
        } // namespace detail

        struct Keys {
            std::array<std::array<Key, kNumSquares>, kNumPieces> pieceSquare{};
            std::array<Key, 16> castling{};
            std::array<Key, 8> epFile{};
            Key side = 0;
        };

        constexpr Keys make_keys() {
            Keys k;
            std::uint64_t state = 0x4B6E6967687431ULL; // fixed seed
            for (auto &row: k.pieceSquare)
                for (Key &x: row) x = detail::splitmix64(state);
            for (Key &x: k.castling) x = detail::splitmix64(state);
            for (Key &x: k.epFile) x = detail::splitmix64(state);
            k.side = detail::splitmix64(state);
            return k;
        }

        inline constexpr Keys kKeys = make_keys();

    } // namespace Zobrist

} // namespace KnightLift
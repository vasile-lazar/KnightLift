#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "types.hpp"

namespace KnightLift::Nnue {

    constexpr int kInputs = 768;
    constexpr int kHidden = 256;
    constexpr int kQA = 512;
    constexpr int kQB = 512;
    constexpr int kScale = 400;

    struct Accumulator {
        std::array<std::array<std::int16_t, kHidden>, 2> v{}; // [perspective][hidden unit]
        bool operator==(const Accumulator &) const = default;
    };

    namespace detail {
        inline bool gEnabled = false; // UseNNUE on and a network loaded
    }

    inline bool enabled() { return detail::gEnabled; }

    // Same layout as the Python reference: ((enemy ? 6 : 0) + piece type) * 64 + square,
    // the square mirrored vertically (^ 56) for the black perspective.
    inline int feature_index(Color persp, Piece p, Square s) {
        const int rel = (color_of(p) == persp) ? 0 : 1;
        const int sq = static_cast<int>(to_index(s)) ^ (persp == Color::White ? 0 : 56);
        return (rel * 6 + static_cast<int>(to_index(type_of(p)))) * 64 + sq;
    }

    bool load(const std::string &path, std::string &error);
    void set_use(bool use);
    bool loaded();

    void reset(Accumulator &acc); // bias values when enabled, zeros otherwise
    void add_feature(Accumulator &acc, Piece p, Square s);
    void sub_feature(Accumulator &acc, Piece p, Square s);
    int evaluate(const Accumulator &acc, Color sideToMove); // cp, side to move's view

} // namespace KnightLift::Nnue
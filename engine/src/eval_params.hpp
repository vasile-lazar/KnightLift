#pragma once

#include <array>
#include <cstddef>

#include "position.hpp"
#include "types.hpp"

namespace KnightLift {

    // Layout of the tunable evaluation parameters. Every index is a (middlegame, endgame)
    // pair, and both values are multiplied by the same integer coefficient of a position.
    namespace EvalParams {
        constexpr std::size_t kMaterial = 5;  // P N B R Q (the king has no material value)
        constexpr std::size_t kIdxMaterial = 0;
        constexpr std::size_t kIdxPst = kIdxMaterial + kMaterial;  // 6 x 64, White view, rank 8 first
        constexpr std::size_t kIdxDoubled = kIdxPst + kNumPieceTypes * kNumSquares;
        constexpr std::size_t kIdxIsolated = kIdxDoubled + 1;
        constexpr std::size_t kIdxPassed = kIdxIsolated + 1;  // 8, relative rank
        constexpr std::size_t kIdxMobility = kIdxPassed + 8;  // 4: N B R Q
        constexpr std::size_t kIdxShieldNear = kIdxMobility + 4;  // endgame value fixed at 0
        constexpr std::size_t kIdxShieldFar = kIdxShieldNear + 1;  // endgame value fixed at 0
        constexpr std::size_t kNumParams = kIdxShieldFar + 1;

        static_assert(kNumParams == 405);
    }  // namespace EvalParams

    struct ParamPair {
        int mg;
        int eg;
    };

    using ParamArray = std::array<ParamPair, EvalParams::kNumParams>;
    using CoefArray = std::array<int, EvalParams::kNumParams>;

    struct EvalTrace {
        CoefArray coef{};  // White count minus Black count per parameter
        int phase = 0;     // clamped to 0..24
        int constMg = 0;   // fixed middlegame part (king attack penalty), White minus Black
    };

    // The engine's current parameter values, in the layout above.
    ParamArray current_params();

    // Coefficients of every parameter for this position.
    EvalTrace eval_trace(const Position &pos);

    // Score from the side to move's point of view, same integer arithmetic as evaluate().
    int eval_from_params(const EvalTrace &trace, const ParamArray &params, Color sideToMove);

}  // namespace KnightLift
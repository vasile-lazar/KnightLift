# Measurements

## Test environment

| Item       | Value                                     |
|------------|-------------------------------------------|
| CPU        | Intel Core i5-14400F                      |
| RAM        | 32 GB                                     |
| GPU        | NVIDIA RTX 5060 8 GB (NNUE training only) |
| OS         | Windows 11                                |
| Compiler   | GCC (CLion bundled MinGW)                 |
| CMake      | v4.0.2                                    |
| Build type | Release                                   |

## Results

Every result needs: date, git commit, compiler, flags, build type.

| Date       | Commit                                   | Test                                                                | Result       | Compiler / flags                |
|------------|------------------------------------------|---------------------------------------------------------------------|--------------|---------------------------------|
| 2026-10-19 | b97b38d04e84ff2447994662a7a7b94012cab922 | init_attacks (PEXT tables, 107k entries)                            | 5.642 ms     | GCC, -O3 -march=native, Release |
| 2026-10-20 | 8e97ee96f95eca90b412ddd725b07cf063f64546 | perft startpos depth 5, baseline (pseudo-legal + make/check/unmake) | 61.6802 Mnps | GCC, -O3 -march=native, Release |
| 2026-10-21 | c5beddb44abba9186452c98302c4de1d8c4e304b | perft startpos depth 5, baseline                                    | 55.8067 Mnps | GCC, -O3 -march=native, Release |

| Depth | Minimax nodes | Alpha-beta nodes | Time (both) |
|-------|---------------|------------------|-------------|
| 1     | 20            | 20               | 0           |
| 2     | 420           | 59               | 0           |
| 3     | 9322          | 585              | 0           |
| 4     | 206603        | 2011             | 2 ms        |
| 5     | 5072212       | 37143            | 79 ms       |

## 3d vs 3c engine versions with a opening book

Score of KL_3d vs KL_3c: 9 - 8 - 33  [0.510] 50
...      KL_3d playing White: 6 - 2 - 17  [0.580] 25
...      KL_3d playing Black: 3 - 6 - 16  [0.440] 25
...      White vs Black: 12 - 5 - 33  [0.570] 50
Elo difference: 6.9 +/- 56.6, LOS: 59.6 %, DrawRatio: 66.0 %
SPRT: llr 0 (0.0%), lbound -inf, ubound inf

Player: KL_3d
"Draw by 3-fold repetition": 33
"Loss: Black mates": 2
"Loss: White mates": 6
"Win: Black mates": 3
"Win: White mates": 6
Player: KL_3c
"Draw by 3-fold repetition": 33
"Loss: Black mates": 3
"Loss: White mates": 6
"Win: Black mates": 2
"Win: White mates": 6
Finished match

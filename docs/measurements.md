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
... KL_3d playing White: 6 - 2 - 17  [0.580] 25
... KL_3d playing Black: 3 - 6 - 16  [0.440] 25
... White vs Black: 12 - 5 - 33  [0.570] 50
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

## 4a vs 4b engines

4a:
position startpos
go depth 8
info depth 1 score cp 0 nodes 40 time 0 pv a2a3
info depth 2 score cp 0 nodes 142 time 0 pv a2a3
info depth 3 score cp 0 nodes 1135 time 1 nps 1135000 pv a2a3
info depth 4 score cp 0 nodes 3647 time 1 nps 3647000 pv a2a3
info depth 5 score cp 0 nodes 28793 time 2 nps 14396500 pv a2a3
info depth 6 score cp 0 nodes 113117 time 6 nps 18852833 pv a2a3
info depth 7 score cp 0 nodes 993344 time 29 nps 34253241 pv a2a3
info depth 8 score cp 0 nodes 8141928 time 439 nps 18546533 pv a2a3
bestmove a2a3

4b:
position startpos
go depth 8
info depth 1 score cp 0 nodes 40 hashfull 0 time 0 pv a2a3
info depth 2 score cp 0 nodes 142 hashfull 0 time 0 pv a2a3
info depth 3 score cp 0 nodes 1135 hashfull 0 time 0 pv a2a3
info depth 4 score cp 0 nodes 3251 hashfull 0 time 1 nps 3251000 pv a2a3
info depth 5 score cp 0 nodes 18871 hashfull 1 time 1 nps 18871000 pv a2a3
info depth 6 score cp 0 nodes 55858 hashfull 3 time 4 nps 13964500 pv a2a3
info depth 7 score cp 0 nodes 260149 hashfull 8 time 9 nps 28905444 pv a2a3
info depth 8 score cp 0 nodes 1052459 hashfull 35 time 63 nps 16705698 pv a2a3
bestmove a2a3

## 4b vs 4c vs 4d bench on depth 7

4b: 1,241,734,355 nodes in 108 seconds
4c: 21,843,147 nodes in 1.1 seconds
4d: 16,985,289 nodes in 0.8 seconds
4e: 17,364,113 nodes in 0.94 seconds
4f: 2,894,330 nodes in 0.2 seconds
5a: 6,178,242 nodes in 0.5 seconds

| 	            | history off         | 	history on          |
|--------------|---------------------|----------------------|
| NullMove off | 16,985,289 (806 ms) | 	17,364,113 (930 ms) |
| NullMove on	 | 2,795,472 (192 ms)  | 2,894,330 (205 ms)   |                 

## bench on depth 8
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

## 3d vs 3c engine versions with an opening book

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
5b: 5,630,863 nodes in 0.5 seconds
5c: 6,106,503 nodes in 0.6 seconds
5d: 5,970,017 nodes in 0.77 seconds
4g: 5,969,646 nodes in 0.92 seconds
5e: 7,562,799 nodes in 1.2 seconds
6b: 5,057,085 nodes in 0.55 seconds
6c: 1,318,842 nodes in 0.2 seconds
6d, 6e, 6f: 1,368,691 nodes in 0.2 seconds


| 	            | history off         | 	history on          |
|--------------|---------------------|----------------------|
| NullMove off | 16,985,289 (806 ms) | 	17,364,113 (930 ms) |
| NullMove on	 | 2,795,472 (192 ms)  | 2,894,330 (205 ms)   |                 

### 5a vs 4f (cutechess-cli, tc 8+0.08, Hash 16, 8moves_v3, -repeat, concurrency 6)
- Builds: 4f bench7 = 2,894,330 nodes; 5a bench7 = 6,178,242 nodes (Release, GCC).
- Result: 13-0-2 in 15 finished games (5 unfinished games discarded at SPRT stop). SPRT elo0=0 elo1=50 alpha=beta=0.05: H1 accepted (LLR 3.17).
- Elo point estimate +458 (error bar undefined, too few games). No illegal moves, no time losses.

### 5c vs 5b (cutechess-cli, tc 8+0.08, Hash 16, 8moves_v3, -repeat, concurrency 6)
- Builds: 5b bench7 = 5,630,863 nodes; 5c bench7 = 6,106,503 nodes (Release, GCC).
- Result (from 5b's side): 88-114-299 in 501 games, Elo -18.0 +/- 19.3 -> 5c about +18 +/- 19 Elo, LOS 96.6% for 5c. Draw ratio 59.7% (289 by 3-fold repetition).
- SPRT (elo0=0, elo1=10) was oriented to 5b, so it is not a valid test of 5c; match stopped manually at 501 games, 6 unfinished games discarded. Conclusion: probable small gain from mobility with placeholder values; 5e tuning expected to improve it.
- Intermediate snapshots for reference: 40 games 10-8-22 (5b ahead), 200 games 31-56-113 (5c ahead); early snapshots are noise.

### 5d vs 5c (cutechess-cli, tc 8+0.08, Hash 16, 8moves_v3, -repeat, concurrency 6)
- Builds: 5c bench7 = 6,106,503 nodes in 0.60 s; 5d bench7 = 5,970,017 nodes in 0.77 s (Release, GCC). nps about 25% lower in 5d.
- Result (5d first): 109-109-267 in 485 games, Elo 0.0 +/- 20.7, LOS 50%. 6 unfinished games discarded. SPRT elo0=0 elo1=10: LLR -0.45, no bound reached.
- 259 of 485 games were 3-fold repetition draws, which compresses decisive results (motivates 4g).
- Conclusion: no measurable gain at initial values and about 25% eval cost; kept for 5e tuning and for the single-pass eval optimisation, which should recover the speed.

### Single-pass eval (branch perf-eval-single-pass, not merged)
- Goal: recover the nps lost to 5d by computing each attack bitboard once for mobility and king zone.
- Correctness: bench 8 total nodes 12,431,983 and every per-position line identical to 4g; new equivalence test passes.
- Speed (bench 8, 5 runs each, alternating, nothing else running): 4g median 1761 ms (about 7.06 Mnps), single pass median 1802 ms (about 6.9 Mnps). Single pass about 2.3% slower, batches do not overlap.
- Conclusion: the repeated piece_attacks call is not the eval bottleneck; refactor not merged. Next: measure the eval in isolation before optimising anything else.

### bench eval (Release, GCC, i5-14400F), main after 4g
- 2,000,000 evals: 146 ms = 73 ns/eval; 5,000,000 evals: 363 ms = 72 ns/eval.
- Checksum per round (10 bench FENs): 435 (fingerprint of the current eval; must stay 435 for any behaviour-preserving refactor).
- Search at the same build: about 7.06 Mnps = about 143 ns/node, so the eval is roughly 40-50% of node time (microbench is hot-cache and likely understates).

### bench eval ablation (Release, GCC, 500000 rounds = 5M evals)
| build                                      | ns/eval |
|--------------------------------------------|---------|
| full eval                                  | 72      |
| no pawn_structure                          | 58      |
| no mobility                                | 61      |
| no king_safety                             | 53      |
| none of the three (material + PST + phase) | 24      |
- Approximate cost per part: PST loop 24 ns, king safety 19, pawn structure 14, mobility 11.
- Decision: incremental material/PST/phase first (largest part, bit-identical result), pawn hash table afterward.

### Incremental material/PST/phase (Release, GCC, i5-14400F)
- bench eval: 72 ns/eval -> 51 ns/eval (-29%). Checksum per round unchanged (435).
- bench 8: total nodes 12,431,983 (identical to 4g, all per-position lines identical); time <fill in> ms (4g median 1761 ms).
- Eval is now roughly 35-40% of node time; remaining cost: king safety about 19 ns, pawn structure about 14, mobility about 11.

### 5e-2 tuning data (Lichess Elite Database, 2020-09 to 2020-12, tools/pgn_to_positions.py)
| month   | games read | games used | train     | val     |
|---------|------------|------------|-----------|---------|
| 2020-09 | 364,782    | 284,019    | 2,037,191 | 225,479 |
| 2020-10 | 403,071    | 315,072    | 2,258,979 | 250,369 |
| 2020-11 | 441,263    | 345,428    | 2,478,379 | 273,529 |
| 2020-12 | 491,920    | 386,023    | 2,767,791 | 307,699 |
- Total: 1,330,542 games used, 9,542,340 train positions, 1,057,076 val positions.
- Result share (White POV): win 0.47, draw 0.114, loss 0.416 in every month.
- About 78% of games passed the filters (the rest were non-Normal terminations or too short).

### 5e-3 Texel tuning run (tools/tune.cpp, 16 threads)
- Data: 9,542,340 train and 1,057,076 val positions (Lichess Elite 2020-09..12). 300 epochs, Adam, lr 1.0 with cosine decay to 10%.
- K = 0.00540615.
- Loss: train 0.189476 -> 0.185407, val 0.189389 -> 0.185470. A constant predictor scores about 0.221, so the eval's explained variance went from about 14.2% to 16.0%.
- No overfitting (train/val gap < 0.0001); about 95% of the gain in the first 100 epochs. Output file: tuned_5e.txt.

### 5e vs pre5e (cutechess-cli, tc 8+0.08, Hash 16, 8moves_v3, -repeat, concurrency 6)
- Result (5e first): 59-81-45 in 185 games, score 0.441 (about -41 +/- 44 Elo), stopped manually, SPRT not concluded. Draw ratio 24% (about 50% in earlier matches).
- Bench 7: pre5e 5,969,646 nodes (739 ms), 5e 7,562,799 (1,005 ms). Bench 8: 12,431,983 vs 18,867,033. Tree growth about 1.3 to 1.5x, nps about -7%.
- Next: fixed-depth match (depth 7, no SPRT) to separate eval quality from speed.

### 5e vs pre5e, fixed depth (cutechess-cli, tc=inf depth=7, Hash 16, 8moves_v3, -repeat, 6 games in parallel, no SPRT)
- 5e first: 112-108-180 in 400 games, Elo +3.5 +/- 25.3, LOS 60.6%, draw ratio 45% (168 repetition, 7 fifty-move, 5 insufficient material).
- Timed match for comparison: 59-81-45 at 185 games (about -41 +/- 44), stopped manually.

### 5e_r2 vs pre5e, fixed depth (cutechess-cli, tc=inf depth=7, Hash 16, 8moves_v3, -repeat, 6 games in parallel, no SPRT)
- 5e_r2 first: 108-113-179 in 400 games, Elo -4.3 +/- 25.3, LOS 36.8%, draw ratio 44.8% (170 repetition, 4 fifty-move, 4 insufficient material, 1 stalemate).
- Bench: 107 to 114% of pre5e (fill in the absolute nodes for bench 7 and bench 8).
- Round 1 for comparison: +3.5 +/- 25.3 (game-result labels). Round 2 labels: Stockfish evaluations.

## 6b: PVS vs pre5e

- Commit: <6b commit hash>, GCC (CLion bundled MinGW), Release, C++20, -march=native.
- Bench 7 (depth 7): 5,969,646 -> 5,057,085 nodes (0.847x, 15% fewer).
- Bench 8 (depth 8): 12,431,983 -> 10,162,376 nodes (0.817x, 18% fewer). 6b alone: 1325 ms, 7.67 Mnps. No same-session timing of pre5e, so no speed comparison.
- Match: tc 8+0.08, Hash 16, concurrency 6, book 8moves_v3.pgn, SPRT elo0=0 elo1=10 alpha=beta=0.05. 6b vs pre5e: 175-109-202 over 486 games, +47.5 +/- 23.7, LOS 100.0%, draw ratio 41.6%, llr 2.95, H1 accepted. 5 unfinished games discarded.
- Earlier partial run (killed): 40-20-40 at 100 games, +70.4 +/- 53.5, llr 0.919. Inflated by early noise, not used.
- Note: the "alpha-beta never searches more" test was removed in 6b, because PVS re-searches can exceed minimax's node count at depth 1 (see design-decisions 6b).

## 6c: LMR vs 6b

- Commit: <6c commit hash>, GCC (CLion bundled MinGW), Release, C++20, -march=native.
- Bench 7 (depth 7): 5,057,085 -> 1,318,842 nodes (0.261x, 3.8x fewer), 213 ms, 6.19 Mnps.
- Bench 8 (depth 8): 10,162,376 -> 2,632,590 nodes (0.259x, 3.9x fewer), 475 ms, 5.54 Mnps. No same-session timing of 6b, so no speed comparison; Mnps drops because the remaining nodes are the more expensive ones.
- Match: tc 8+0.08, Hash 16, concurrency 6, book 8moves_v3.pgn, SPRT elo0=0 elo1=10 alpha=beta=0.05. 6c vs 6b: 67-17-73 over 157 games, +114.6 +/- 40.0, LOS 100.0%, draw ratio 46.5%, llr 2.96, H1 accepted. 5 unfinished games discarded.
- Caveat: SPRT stopped at 157 games, so the point estimate is likely inflated (lower bound about +75). No same-build ablation yet; planned together with the history and null-move re-measurement via a UCI option.

## 6d: aspiration windows vs 6c

- Commit: <6d commit hash>, GCC (CLion bundled MinGW), Release, C++20, -march=native.
- Bench 7 (depth 7): 1,318,842 -> 1,368,691 nodes (1.038x), 192 ms, 7.13 Mnps.
- Bench 8 (depth 8): 2,632,590 -> 2,741,414 nodes (1.041x), 379 ms, 7.23 Mnps. No same-session timing of 6c, so no speed comparison.
- Match: tc 8+0.08, Hash 16, concurrency 6, book 8moves_v3.pgn, SPRT elo0=0 elo1=10 alpha=beta=0.05. 6d vs 6c: 211-177-312 over 700 games, +16.9 +/- 19.2, LOS 95.8%, draw ratio 44.6%, llr 1.24 (bounds -2.94 / 2.94), no bound reached. Stopped manually, 6 unfinished games discarded.
- Result: inconclusive. Node count at fixed depth rose about 4%. Root best moves and scores differ from 6c in some bench positions (for example position 7 at depth 8: 21 -> 13), consistent with search instability under narrow windows; ties between q and r promotion (d7c8) are equal-score ties.
- Decision: merged on the same grounds as 4g (positive point estimate, LOS about 96%, standard technique), to be ablated with a UCI option later. Initial delta 25 and the doubling factor are untuned.

## 6e: UCI options LMR and Aspiration (verification)

- Commit: <6e commit hash>, GCC (CLion bundled MinGW), Release, C++20, -march=native.
- Defaults (LMR on, Aspiration on): bench 7 1,368,691 and bench 8 2,741,414 nodes, equal to 6d.
- Aspiration off: bench 7 1,318,842 and bench 8 2,632,590 nodes, equal to 6c.
- LMR off and Aspiration off: bench 7 5,057,085 and bench 8 10,162,376 nodes, equal to 6b.
- Conclusion: the options reproduce the earlier builds exactly, so the same exe can be used for same-build ablation matches.

## 6f: History and NullMove ablation (same exe)

- Commit: <6f commit hash>, GCC (CLion bundled MinGW), Release, C++20, -march=native.
- Defaults: bench 7 1,368,691 and bench 8 2,741,414 nodes, equal to 6d/6e.
- History off: bench 7 (depth 7) 1,380,101 nodes (1.008x), bench 8 (depth 8) 2,835,668 nodes (1.034x). With the material-only eval (4e/4f) history cost +2-3.5% nodes; with the stage-5 eval, PVS and LMR it saves 0.8% (depth 7) to 3.4% (depth 8).
- NullMove off: bench 7 (depth 7) 1,855,016 nodes (1.355x), bench 8 (depth 8) 4,148,232 nodes (1.513x). Null move saved about 2.7x with the stage-5 eval before PVS and LMR, now 1.36-1.51x: LMR and PVS take over part of its effect.
- Timings not compared (different sessions, Mnps varies 4.5-6.4).
- Match History on vs off: <W-L-D, games, Elo, SPRT line>.
- Match NullMove on vs off: <W-L-D, games, Elo, SPRT line>.

## 6g: capture generator for quiescence

- Commit: <6g commit hash>, GCC (CLion bundled MinGW), Release, C++20, -march=native.
- Bench 7 (depth 7): 1,368,691 nodes and bench 8 (depth 8): 2,741,414 nodes, identical to 6d/6e/6f, as required for a change with no behaviour change. The new test (capture list equals the filtered full list, in the same order, on the root and all children of six FENs) passes.
- Speed: not measured. Timings were taken on a machine with other processes running and were discarded as too noisy, so no speedup is claimed. The motivation (qsearch skips generating and filtering quiet moves) is a design argument here, not a measured result.




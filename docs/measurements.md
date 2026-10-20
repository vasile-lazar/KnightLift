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

| Depth | Minimax nodes | Alpha-beta nodes | Time (both) |
|-------|---------------|------------------|-------------|
| 1     | 20            | 20               | 0           |
| 2     | 420           | 59               | 0           |
| 3     | 9322          | 585              | 0           |
| 4     | 206603        | 2011             | 2 ms        |
| 5     | 5072212       | 37143            | 79 ms       |

## Nodes and time before alpha-beta comparison

go depth 1
info depth 1 score cp 0 nodes 20 time 0
bestmove a2a3
go depth 2
info depth 2 score cp 0 nodes 420 time 0
bestmove a2a3
go depth 3
info depth 3 score cp 0 nodes 9322 time 0
bestmove a2a3
go depth 4
info depth 4 score cp 0 nodes 206603 time 3 nps 68867666
bestmove a2a3
go depth 5
info depth 5 score cp 100 nodes 5072212 time 70 nps 72460171
bestmove b2b3
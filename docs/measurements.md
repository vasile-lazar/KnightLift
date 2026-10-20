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

| Date       | Commit                                   | Test                                     | Result   | Compiler / flags                |
|------------|------------------------------------------|------------------------------------------|----------|---------------------------------|
| 2026-10-19 | b97b38d04e84ff2447994662a7a7b94012cab922 | init_attacks (PEXT tables, 107k entries) | 5.642 ms | GCC, -O3 -march=native, Release |
| 2026-10-20 | <commit> | perft startpos depth 5, baseline (pseudo-legal + make/check/unmake) | 61.6802 Mnps | GCC, -O3 -march=native, Release |
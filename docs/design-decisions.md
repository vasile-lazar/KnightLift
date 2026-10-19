# KnightLift: design decisions log

Running log for the thesis. One entry per component: what was decided,
which alternatives existed, and why.

## 0. Engine language (2026-10-19)
- Chosen: C++20.
- Alternative: C# (faster to write for me, lower performance ceiling).
- Why: bitboard movegen, make/unmake, transposition table and NNUE inference
  are cache- and SIMD-sensitive. C++ gives intrinsics, constexpr table
  generation, <bit> operations and predictable memory layout. Reference
  engines and NNUE tooling are mostly C++/Rust, so literature maps directly.
- The web backend stays in .NET, so C# systems design is still demonstrated.

## 1a. Build system and toolchain (2026-10-19)
- Build system: CMake >= 3.20. The root file delegates to `engine/` with
  `add_subdirectory`; tests and tools will be added as siblings.
- Standard: C++20, extensions off (-std=c++20), for portability.
- Warnings on: -Wall -Wextra -Wpedantic (/W4 on MSVC).
- Toolchain: CLion with bundled MinGW (GCC) on Windows 11. Clang and MSVC
  builds are planned as a comparison measurement.
- All performance numbers come from Release builds, with compiler version
  and flags recorded in measurements.md.

## 1b. Core types and square mapping (2026-10-19)
- Square mapping: little-endian rank-file (LERF): a1 = 0, b1 = 1, ..., h8 = 63.
  square = rank * 8 + file; file = sq & 7; rank = sq >> 3.
- Consequence: north = +8 (<< 8), east = +1 (<< 1), so pawn pushes and
  sliding fills are plain shifts. East/west shifts need file masks to avoid
  wrap-around.
- Alternatives (for the background chapter): 8x8 mailbox, 10x12 mailbox,
  0x88, and rank-file mappings with a8 = 0.
- Types: `enum class` with std::uint8_t underlying type (Color, PieceType,
  Piece, File, Rank, Square). Prevents accidental mixing; costs an explicit
  cast, wrapped in one helper `to_index`.
- Sentinel values (Square::None = 64, Piece::None = 12) sit outside the valid
  range and are never used as indices or shift amounts.
- Invariants are checked at compile time with static_assert.
- Verified: square_bb(E4) prints 0x10000000 (bit 28).

## Sources
- Chess Programming Wiki: Bitboards, Square Mapping Considerations.
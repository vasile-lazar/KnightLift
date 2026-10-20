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

## 1c. Bitboard utilities (2026-10-19)
- Bit operations use C++20 <bit> (std::popcount, std::countr_zero), wrapped in
  small constexpr functions (popcount, lsb, pop_lsb, more_than_one). No
  compiler-specific intrinsics.
- pop_lsb (lsb, then bb &= bb - 1) is the standard loop for iterating over
  set bits.
- File and rank masks are derived from kFileABB / kRank1BB by shifting;
  needed to prevent east/west wrap-around in attack generation.
- Note for measurements: the default x86-64 baseline has no popcnt
  instruction; compare perft with and without hardware popcount.

## 1d. Position and FEN (2026-10-19)
- Position keeps redundant views: 12 piece bitboards, 2 color bitboards,
  occupancy and a 64-entry mailbox. Bitboards drive move generation; the
  mailbox answers "which piece is on this square" in O(1). All mutation goes
  through a small set of functions to keep the views in sync.
- State: side to move, castling rights (4-bit mask), en passant square,
  halfmove clock, fullmove number.
- FEN parsing returns bool and leaves an empty position on failure (UCI input
  must never crash the engine). Validation: placement geometry, side,
  castling letters, en passant file/rank, counters, one king per side.
- Verified by round-trip (parse then print equals input) on four standard
  positions, and by rejection of five malformed FENs.
- Planned for make/unmake: a stack of State records (captured piece, castling
  rights, en passant square, halfmove clock, later the hash), instead of
  copy-make.

## 1e-1. Move encoding (2026-10-19)
- 16-bit move: from (6 bits) | to (6 bits) | flag (4 bits).
- Flag nibble: bit 2 = capture, bit 3 = promotion, low 2 bits = promotion
  piece (knight, bishop, rook, queen) or special move (double push, castles).
  is_capture and is_promotion are single bit tests.
- 16 bits keep transposition-table entries small. Scores will live in a
  separate ScoredMove (step 4). Alternative considered: 32-bit move with
  moving and captured piece embedded.
- Castling is encoded as the king move (e1g1 / e1c1), matching UCI text.
- Move 0 (a1a1) is the "no move" value: it can never be legal.
- MoveList: fixed 256-entry array on the stack (max known legal moves: 218).
  To measure later: cost of zero-initializing the array per node.

## 1e-2. Leaper attack tables (2026-10-19)
- Knight, king and pawn attacks are precomputed per square (pawns per color)
  in constexpr tables: zero startup cost, stored as read-only data, verified
  at compile time with static_assert (totals: knight 336, king 420, pawn 98
  per color).
- Construction uses (file, rank) offsets with separate bounds checks, which
  avoids the edge wrap-around bug of shift-based construction. Run time is
  irrelevant because it executes at compile time.
- inline constexpr tables give one shared copy across translation units.
- Attacker detection uses symmetry: attackers of square s by color c =
  attacks_of(~c, s) & pieces(c, type).

## 1e-3a. Sliding attacks, reference implementation (2026-10-19)
- Sliders take (square, occupancy). The result includes the first blocker on
  each ray (any color) and nothing behind it; move generation masks out
  friendly pieces. This gives captures and "defended piece" detection for free.
- Ray walk with separate file/rank bounds checks; queen = bishop | rook.
- constexpr, verified at compile time (empty-board totals: rook 896, bishop
  560; spot checks with blockers).
- Role: oracle for testing the optimized implementations, and table builder
  for PEXT/magic tables. Not used in the search.
- Planned measurement: nanoseconds per lookup for reference vs PEXT vs magic
  bitboards.

## 1e-3b. PEXT bitboards for sliding attacks (2026-10-19)
- Lookup: index = PEXT(occupied, relevant_mask[sq]); attacks =
  table[sq][index]. The relevant mask excludes ray-end (edge) squares, whose
  occupancy cannot change the result. Rook tables: 102,400 entries (800 KiB);
  bishop: 5,248 entries (41 KiB); the PEXT index is dense, so these are the
  minimum sizes, with no magic numbers to search for.
- Tables are filled at startup by init_attacks() from the reference
  implementation, using PDEP to enumerate blocker subsets. Not constexpr:
  ~107k entries would approach the compiler's constexpr step limit.
- Portability: hardware PEXT when __BMI2__ is defined (CMake option
  KNIGHTLIFT_NATIVE, -march=native); correct but slower software fallback
  otherwise. Release binaries must use a fixed target level, not native.
- Verified against the reference implementation with 10,000 random
  occupancies per square for both piece types, and by table-size checks.
- To measure: init time; lookup cost for reference vs PEXT vs magic bitboards.
- Known caveat: PEXT is microcoded and slow on AMD Zen 1/2, so a
  magic-bitboard fallback is planned (1e-3c).

## 1e-4. Attack queries and pseudo-legal move generation (2026-10-20)
- Position::attackers_to(sq, occupied) uses attack symmetry (attackers of sq
  by color c = attacks_of(~c, sq) & pieces(c, type)); occupancy is a
  parameter so later code can test hypothetical boards (king moves, pins).
- Movegen is pseudo-legal (pins/self-check filtered after make in 1e-6);
  castling is fully checked at generation (rights, empty squares, king not
  in check, path and destination not attacked).
- Pawns are generated per pawn (simple); bulk-shift version is a candidate
  optimization to measure.
- FEN now rejects pawns on the first/last rank.
- Verified on four hand-counted positions (start 20, castling 26,
  promotion 7, en passant 7).

## 1e-5. Make / unmake (2026-10-20)
- Irreversible state (captured piece, castling rights, en passant square,
  halfmove clock) is pushed on a fixed 2048-entry stack per move; unmake
  pops it. Alternative considered: copy-make (copy the Position per move).
- Castling rights updated with a per-square mask table
  (rights &= mask[from] & mask[to]).
- All board edits go through put/remove/move_piece; consistent() rebuilds
  the bitboards from the mailbox as a debugging check.
- Tested by a depth-3 make/unmake round trip over the full pseudo-legal
  tree on seven positions: consistency after every make, exact FEN after
  every unmake.

## 1e-6/1e-7. Legality, perft and test structure (2026-10-20)
- Legality: make the pseudo-legal move, test whether the mover's king is
  attacked, unmake if so. Simple baseline; a pin/check-aware generator is a
  planned optimization.
- Perft (counts of the legal-move tree) validated against six standard
  positions (start, Kiwipete, positions 3-6, plus the mirror of position 4).
  perft_divide gives per-root-move counts for debugging against another engine.
- Structure: knightlift_core static library, knightlift executable,
  knightlift_tests executable registered in CTest; flags and include path are
  PUBLIC on the library so all targets compile consistently.

## 2a. UCI front end (2026-10-20)
- uci_loop(istream, ostream): testable by scripting input. Supports uci,
  isready, ucinewgame, position (startpos/fen + moves), go, quit, plus d and
  "go perft N". Unknown commands ignored per spec.
- Moves in "position ... moves" are matched against the legal move list; a
  bad FEN is parsed into a temporary Position so state is never corrupted.
- go currently plays a random legal move (placeholder for search); used to
  validate movegen and protocol handling in a GUI.
- Replies are flushed (std::endl): a pipe is fully buffered and would hang the GUI.

## 3a. Evaluation and negamax (2026-10-20)
- Negamax (score = -child score) with centipawn scores from the side to
  move's view. Material-only evaluation (P100 N320 B330 R500 Q900).
- Mate score kMateScore - ply (shorter mates preferred); no legal moves =
  mate if in check, otherwise stalemate (0). Mate scores reported to UCI as
  "mate N" in moves.
- Legality via make / check / unmake, as in perft.
- Sanity check: without pruning, nodes(depth N) = sum of perft(1..N).
- Known gaps: horizon effect (quiescence, step 3d); no repetition or
  50-move handling; deterministic first-move tie-break.

## 3b. Alpha-beta (2026-10-20)
- Negamax alpha-beta, fail-soft, window (-beta, -alpha) passed to children.
  Same score and best move as plain minimax at equal depth, with fewer
  nodes; plain minimax is kept (flag) as reference and for measurements.
- Test: score/best move equality against minimax on five positions, depths
  1-4. Node reduction recorded per depth (measurements.md).
- Move ordering is still generation order; ordering (MVV-LVA, killers,
  history) is stage 4 and should lower the effective branching factor further.

## Sources
- Chess Programming Wiki: Bitboards, Square Mapping Considerations.
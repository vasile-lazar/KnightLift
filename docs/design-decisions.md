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

## 3c. Iterative deepening and time management (2026-10-20)
- Iterative deepening; previous iteration's best move is searched first at
  the root. Depth 1 always completes, so a move always exists. Aborted
  iterations are discarded; the position is restored on abort.
- Stop/clock checked every 2048 nodes. Time: movetime - 30 ms; otherwise
  optimum = time/movesToGo(30) + 3/4 inc, hard limit = min(3*optimum,
  time - 30 ms); no new iteration after optimum/2. Constants to be tuned
  with SPRT (stage 6).
- Search runs on its own thread; output serialized by a mutex; commands
  touching the position wait for the search to end first.
- Stops early when a forced mate is proven.

## 3d. Quiescence search (2026-10-20)
- At depth 0 the search continues with captures and promotions only, with
  stand-pat (static eval as a lower bound), alpha-beta cutoffs and MVV-LVA
  ordering (selection sort). Fixes the horizon effect (checked by a
  poisoned-pawn test).
- Plain minimax uses the same quiescence without pruning (prune = false) so
  the alpha-beta equivalence tests stay valid.
- Simplifications: no check evasions in quiescence, no delta pruning/SEE;
  captures obtained by filtering pseudo-legal moves (dedicated generator
  later).
- Measured: KL_3d vs KL_3c match result (record in measurements.md).

## 4a. Zobrist hashing

- 64-bit keys: piece×square (12×64), castling (16, indexed by the 4-bit mask), ep file (8), side to move.
- Keys are generated at compile time with splitmix64 and a fixed seed: no init function, reproducible runs.
- Incremental update inside put_piece/remove_piece/move_piece; make_move only handles castling, ep and side.
- State stores the previous key; unmake restores it by assignment instead of reversing the xors.
- En passant is hashed only if a pawn of the side to move can capture on the ep square, so identical positions get identical keys (TT hits, repetition detection).
- set_from_fen computes the key from scratch (compute_key); consistent() verifies key_ == compute_key().

## 4b. Transposition table

- Entry 12 bytes: key32 (low 32 bits of the Zobrist key), best move, int16 score, uint8 depth, generation (6 bits) + bound (2 bits).
- Index by multiply-shift ((key * size) >> 64), so the size is not limited to a power of two. key32 uses the LOW bits because the index is derived from the high bits; using the high bits for both would make the stored check redundant.
- One entry per slot. Replace if: exact bound, different position, depth + 4 > stored depth, or older generation. The old move is kept when the same position is stored without a move.
- Mate scores are stored relative to the node (not the root) and converted back on probe, so mate distances stay correct across transpositions.
- Cutoff only when stored depth >= required depth and the bound is compatible with the window. Never at the root (ply 0). The TT move is searched first at every node and at the root.
- Quiescence does not use the table (postponed). Interaction with repetition detection (path dependence) will be revisited when draw detection is added.
- UCI: option Hash (MB, default 16, min 1, max 4096); ucinewgame clears the table; one generation per search.

## 4b-2. bench command

- `bench [depth]` (default 7) searches a fixed list of 10 positions with iterative deepening to the given depth, clearing the TT before each one, and prints per-position nodes plus total nodes, time and nps.
- Search is single-threaded and depth-limited, so total nodes is deterministic: it is the signature of a search version and the main yardstick for steps whose Elo gain is too small to measure while the eval is material-only.
- Elo/SPRT matches are postponed until the hand-crafted eval (stage 5) gives games enough signal.

## 4c. Move ordering: TT move and MVV-LVA in the main search

- Every move gets a score; the search picks the best remaining move by selection sort, one move at a time. Picking lazily avoids sorting moves that are never searched after an early cutoff.
- Order: TT move (or the previous iteration's best move at the root), then captures and promotions by MVV-LVA, then quiet moves in generation order.
- The TT move is scored like any other move instead of being moved to the front. A move that is not in the list (a 32-bit key collision) is simply never selected, so no special handling is needed.
- Yardstick: bench total nodes at depth 6, before and after. Bad-capture detection (SEE) is postponed.

## 4d. Killer moves

- Two killer slots per ply, filled by quiet moves (no capture, no promotion) that cause a beta cutoff in the main search. The new killer goes in slot 0 and the old slot 0 is shifted to slot 1; a killer already in slot 0 is not duplicated.
- Order: TT move, captures/promotions (MVV-LVA), killer 0, killer 1, other quiet moves. Without SEE, bad captures still rank above killers (postponed).
- Killers live in the Searcher, so they start empty for every search and are never shared between searches. They are not validated: a killer that is not in this position's move list is simply never matched.
- Yardstick: bench total nodes (depth 7), before and after.

## 4e. History heuristic

- Table history[color][from][to], one int per entry, zeroed at the start of every search (it lives in the Searcher). Persistence across moves with aging is postponed.
- On a quiet beta cutoff: bonus = min(depth*depth, 1600) for the cutoff move, and the same malus for every quiet move tried earlier in the node.
- Update with "gravity": h += bonus - h*|bonus|/16384, which keeps every entry bounded in (-16384, 16384) without a separate aging step.
- Move ordering score ranges: TT move 2,000,000; captures 1,000,000 + MVV-LVA; killer 0 900,000; killer 1 800,000; quiet moves by their history value (can be negative).
- Constants (cap, max) are first guesses; they will be tuned when Elo matches are available.
- Yardstick: bench total nodes (depth 7), before and after.

## 4f. Null-move pruning

- In alphabeta, before generating moves: if depth >= 3, no null move was just made, the side to move is not in check, it has at least one non-pawn non-king piece (zugzwang guard) and the static eval >= beta, make a null move and search with depth - 1 - R and the zero window (-beta, -beta+1). If the result >= beta, return it (a mate score from the null search is not trusted and beta is returned instead).
- R = 2 + depth/6 (R = 2 for depth 3-5, R = 3 for depth 6-11, ...). If depth - 1 - R <= 0 the null search is a quiescence search.
- No two null moves in a row (allowNull flag passed down the recursion).
- Applies only to search_iterative, never to the fixed-depth reference search(), so the minimax equivalence tests stay valid.
- UCI check option NullMove (default true) lets later Elo matches measure on vs off. Without Elo matches the bench can only show the speed side, so the accuracy side is checked by comparing per-position scores with the option on and off.
- Postponed: verification search at high depth, storing the null-move cutoff in the TT, tuning R.

## 5a: Tapered piece-square evaluation

- The eval is the sum, per piece, of (material + PST) in two variants: middlegame (mg) and endgame (eg). The two sums are interpolated by a game phase derived from the remaining non-pawn material.
- Weights N=1, B=1, R=2, Q=4, total 24 (full starting material). phase = min(sum, 24). Score = (mg*phase + eg*(24-phase)) / 24. Promotions can exceed 24, hence the clamp.
- Initial values are PeSTO (Ronald Friederich), including its mg/eg material values, because those tables are calibrated against them (mixing with 100/320/330/500/900 would double-count). They are only a starting point: 5e replaces them with tuned values.
- Tables are written from White's view, rank 8 first (readable in source). With LERF (a1=0), White indexes `sq ^ 56`, Black indexes `sq` (vertical mirror), so one table serves both colors.
- King wants safety in the middlegame and centralization in the endgame; pawns gain value as they advance in the endgame. A single table cannot express that, and a hard phase switch causes eval discontinuities that destabilize the search.
- Hard mg/eg switch (discontinuous); computing PSTs incrementally in make_move (premature, revisit with NNUE accumulator work); a single table (above).
- Bench nodes change by design (eval affects pruning), so the metric is Elo vs 4f (cutechess-cli, 8moves_v3, tc 8+0.08, SPRT). Bench depth 6 recorded for reference only.

## 5b: Pawn structure
- Three terms per side, each with a mg and an eg value, added to the tapered sums from 5a: doubled pawns (penalty per pawn beyond the first on a file), isolated pawns (no friendly pawn on adjacent files), passed pawns (no enemy pawn ahead on the same or adjacent files), the latter with a bonus indexed by relative rank.
- Bitboard-only: file masks, adjacent-file masks, and per-colour "front span" masks (squares ahead on the pawn's file and adjacent files). Masks are constexpr tables built at compile time. No pawn hash table yet.
- Pawn structure is the cheapest strong positional signal after PSTs, and the passed-pawn bonus is the main endgame driver that the PST pawn table only approximates.
- Pawn hash table now (premature; measure eval cost first, revisit in 5e or stage 6); backward pawns, connected/phalanx pawns, blocked passers (postponed to a later 5b+ step so each term can be measured separately).
- Hand-picked starting points (doubled -10/-20, isolated -10/-15, passed bonus growing with rank), explicitly placeholders for the 5e tuning.
- Elo vs 5a (cutechess-cli, 8moves_v3, tc 8+0.08, SPRT elo0=0 elo1=10, -games 2 -repeat). Bench nodes recorded for reference only. If the SPRT is inconclusive, record the result and keep the term set, since the 5e tuner will decide the values.

## 5c: Mobility
- For knights, bishops, rooks and queens, count the squares the piece attacks inside a mobility area, and add (count - base) * weight to the mg and eg sums (separate weights per piece type), tapered as before.
- All squares except those occupied by own pieces and those attacked by enemy pawns. Own pieces are excluded so a piece does not get credit for "moving" onto its own pieces; enemy-pawn attacks are excluded because such moves lose the piece for a pawn. Sliders use the full occupancy (no x-rays through pieces).
- Bonus is linear in the count with a per-piece baseline (the typical count, so an average piece scores about 0). Real mobility curves are concave; a table per piece type is the 5e tuning target.
- Counting pseudo-legal moves (needs move generation inside eval, slow; attack bitboards are the standard cheaper proxy); pins/king-safety-aware mobility (belongs to 5d); concave tables now (the number of free parameters is premature before the tuner exists).
- Baseline/mg/eg: N 4/4/4, B 6/5/5, R 6/2/4, Q 13/1/2. Placeholders for 5e.
- Elo vs 5b (cutechess-cli, 8moves_v3, tc 8+0.08, SPRT elo0=0 elo1=10). Bench nodes for reference; also note bench nps, because mobility makes eval noticeably slower.

## 5d: King safety
- Two middlegame-only terms, computed per side by `king_safety(pos, color)`:
1. Pawn shield: friendly pawns on the king's file and its two neighbours (file clamped to B..G so a corner king uses B-D / E-G... three files), +10 per pawn on the rank directly in front of the king, +5 per pawn two ranks in front.
2. King-zone attackers: the zone is the king square plus the squares it attacks. Each enemy N/B/R/Q attacking at least one zone square counts as an attacker with a weight (N2 B2 R3 Q5). With two or more attackers, penalty = min(units^2 / 2, 250), where units is the weight sum.
- King safety matters while queens and rooks are on the board. Adding it to the mg sum only lets the tapered eval fade it out with the phase, so no separate phase logic is needed.
- A single piece rarely mates; danger grows faster than linearly with the number of attackers. This is the standard shape (Stockfish/Crafty style), simplified to a formula instead of a table so 5e can replace it by a tuned table.
- `king_safety` is declared in eval.hpp so tests can check it directly, because the shield and attacker terms are too small to isolate through full `evaluate` (PSTs and mobility change with every piece move).
- Pawn storm terms, open/semi-open file penalties next to the king, safe-check detection, attacker counts weighted by attacked zone squares (all postponed; each needs separate measurement). A single combined pass with mobility to avoid computing piece attacks twice (premature; speed work after the terms are settled).
- Placeholders for 5e.
- Elo vs 5c (cutechess-cli, new version listed first, 8moves_v3, tc 8+0.08, SPRT elo0=0 elo1=10). The expected gain is small, so let it run to a bound or at least 1000 games, and record the nps cost from bench.

## 4g: Draw detection in search
- Three draw rules are recognised inside the search (not in the static eval) and score 0:
1. Repetition: a position that already occurred inside the current search path counts as a draw at its first repeat (twofold); a position that only repeats positions from before the root needs a third occurrence (threefold, like the real rule).
2. Fifty-move rule: halfmove clock >= 100, unless the side to move is in check (a possible mate takes priority; the case where it is in check but not mated is rare and accepted).
3. Insufficient material: K vs K and K+single minor vs K.
- At node entry in `alphabeta`, before the TT probe and before quiescence, never at the root (the root must return a move). Gated by a Searcher flag enabled only in `search_iterative`, the same way as null move, so the fixed-depth `search()` and the minimax reference stay identical and the equivalence tests remain valid.
- 289 of 501 games in the 5c vs 5b match ended by 3-fold repetition. Without repetition awareness the engine cannot avoid repeating when ahead or exploit it when behind, which also wastes eval gains and inflates the draw rate.
- Graph-history interaction: TT entries can store a draw score that depends on the path. Standard in engines, ignored for now.
- Repetition in `evaluate` (wrong layer, eval has no path), a repetition hash table (the State stack scan limited by the halfmove clock is cheap enough), a contempt value (later, after tuning), a UCI option to switch it off (add only if the Elo match needs an ablation).
- Elo vs the previous build with the same eval (cutechess, new version first, tc 8+0.08, SPRT elo0=0 elo1=10). Also compare the share of "Draw by 3-fold repetition" games, which should drop clearly. Bench nodes may change slightly (draw scores prune differently).
- 289 of 501 games in the 5c vs 5b match and 259 of 485 in 5d vs 5c ended by 3-fold repetition. Without repetition awareness the engine cannot avoid repeating when ahead or exploit it when behind, which also wastes eval gains and inflates the draw rate.

## Tool: `bench eval` (evaluation microbenchmark)
- `bench eval [rounds]` calls `evaluate` on the 10 bench positions `rounds` times (default 200000, so 2 million calls) and prints the total time, ns per call and a checksum of all returned scores.
- Reason: the single-pass refactor showed that guessing the eval's cost from search nps is unreliable. Search nps mixes eval, movegen, make/unmake, TT and quiescence; this isolates the eval.
- Comparison with search: at about 7 Mnps the search spends about 143 ns per node. The ratio ns/eval to ns/node shows the eval share (the ratio is approximate, since the search calls the eval once per quiescence node and once per null-move check).
- The checksum is deterministic: two builds with the same eval must print the same checksum, so it doubles as a cheap "eval unchanged" check for refactors.
- Rules: Release build only, nothing else running, 5 runs, take the median.
- Decision rule: if the eval is more than about half of the per-node time, optimise the eval; if it is below about 30%, the time is in movegen and quiescence and the dedicated capture generator is the next speed step.

## Incremental material, PST and phase
- `Position` keeps three integers updated in `put_piece`, `remove_piece` and `move_piece`: `psqMg_` and `psqEg_` (White minus Black material + PST, middlegame and endgame) and `phase_` (unclamped sum of piece phase weights). Every make/unmake path goes through those three primitives, so none of the move types needs special code.
- The tables move to a new header `eval_tables.hpp`. A combined signed table `kPieceMg[piece][square]` / `kPieceEg` contains material + PST, the `^ 56` mirror for White and the sign (+ White, - Black), so an update is one array lookup and one add.
- `State` stores the three values before each move and `unmake_move` restores them, the same way as the key. Null moves change nothing.
- `consistent()` recomputes the three values from the mailbox, so every existing make/unmake and perft test now also checks the incremental update.
- `evaluate` reads the values and only computes pawn structure, mobility and king safety. The result is bit-identical (integer sums, same tapering formula).
- Why: bench eval ablation showed the PST loop costs about 24 of 72 ns per eval. This is also the accumulator pattern needed for NNUE in stage 7.
- Cost: three adds per piece move in `make_move`, and 12 more bytes per `State` (80 KB per Position copy; copies are rare).
- Rejected for now: a pawn hash table (next step, needs a pawn key), lazy eval cutoffs (changes the eval, needs an Elo match).
- Measurement: bench 8 total nodes (12,431,983) and every per-position line must be identical to 4g; `bench eval` checksum per round must stay 435; then compare ns/eval (72) and bench 8 time (about 1761 ms).

## 5e-1: Evaluation coefficient trace
- Tuning needs the eval as a function of its parameters. `evaluate` stays the fast engine path (constexpr tables, incremental PST). A second function, `eval_trace(pos)`, walks the same terms and records, for every parameter, an integer coefficient (White count minus Black count) instead of a score.
- Parameters are (middlegame, endgame) pairs sharing one coefficient: material (5), PST (6 x 64), doubled, isolated, passed (8 relative ranks), mobility weight (4: N B R Q), pawn shield near and far (mg only). 405 pairs, 810 numbers.
- A position's score is then `((constMg + sum c_i * mg_i) * phase + (sum c_i * eg_i) * (24 - phase)) / 24`, with the same integer division as `evaluate`. The king attack penalty is nonlinear (quadratic with a cap), so it is not tuned in this round: its White-minus-Black value is stored as `constMg`.
- Mobility baselines stay fixed constants, so `w * (n - base)` is linear in w with coefficient `n - base`.
- `eval_from_params(trace, params, side)` reproduces the engine score from a parameter array; `current_params()` builds the array from the engine's constants.
- Test: for every node of a depth-2 walk of the perft positions, `eval_from_params(eval_trace(pos), current_params(), side) == evaluate(pos)`. This catches any divergence between the trace and the real eval, and must be rerun after every eval change.
- King shield and attack penalty were split into shared helpers (`shield_counts`, `attack_penalty`) so the engine path and the trace path use the same code for those terms.
- Rejected: templating `evaluate` itself on a trace type (touches the hot path, harder to read), tuning through repeated `evaluate` calls with modified tables (the tables are constexpr and the PST sums are incremental), tuning the king attack penalty now (needs a table form first, later step).
- Measurement: no behaviour change; `bench 8` nodes (12,431,983) and `bench eval` checksum per round (435) must stay identical, ns/eval within noise of 51.

## 5e-2: Tuning data pipeline
- Source: the Lichess Elite Database (database.nikonoel.fr), monthly PGN files of Lichess games between strong players (the site filters 2400+ against 2200+, bullet excluded). Chosen over engine self-play for 5e because the game quality is far above my engine's, and over the full Lichess open database because that one is hundreds of GB unpacked per month.
- A Python script (`tools/pgn_to_positions.py`, python-chess) turns PGN into text lines `<FEN>;<result>`, with the result from White's point of view (1.0, 0.5, 0.0). Python is used for the PGN/SAN parsing only; the tuner itself stays in C++ (5e-3).
- Filters per position: skip the first 10 plies (opening theory), skip positions in check, skip positions reached by a capture or promotion (a rough quiet filter), skip games whose Termination is not Normal (time forfeits and abandonments mislabel the position).
- Sampling: 8 random positions per game (seeded), so positions of one game, which share a label, do not dominate the set.
- Split: every 10th game goes to the validation file and the rest to the training file, split by game so no game appears in both (a split by position would leak the label).
- Target size: about 4 million training positions (about 500k games), enough for 405 parameters with room to spare.
- Known limitations: labels are game results of human games, so they are noisy; the quiet filter is crude (a position can still hold a pending capture); the Elite database has no engine evaluations.
- Rejected for now: engine-based quiet filter (static eval against quiescence score, a possible refinement in 5e-3), Stockfish-labelled data (not needed for Texel; relevant for NNUE in stage 7), self-play data (slow and weak at this stage).
- Measurement: the script prints games read, games used, positions per file and the result distribution; spot-check ten positions in the engine with `position fen ...` and `d`. The final value of the data is measured only by the Elo match after tuning (5e-4).

## 5e-4: Tuned parameters, round 1 (rejected)
- The Texel result of 5e-3 (9.5 million positions from Lichess Elite 2020-09 to 2020-12, K = 0.00541, validation loss 0.18939 -> 0.18547) was applied to the evaluation constants on branch `stage-5e4-tuned-values` and compared with the previous build.
- Fixed depth 7 (400 games): 112-108-180, +3.5 +/- 25.3 Elo, so no gain in eval quality. Timed 8+0.08 (stopped at 185 games): 59-81-45, about -41 +/- 44 Elo. Bench 7 nodes 5,969,646 -> 7,562,799 and bench 8 12,431,983 -> 18,867,033 (tree about 1.3 to 1.5x), nps about -7%.
- Interpretation: labels from human blitz results are noisy (the eval explains about 16% of the result variance), the quiet filter was crude, and human play rewards attacking factors more than an engine defender does; the tuned values fit the data but do not play better.
- Decision: values not merged; the trace, data script and tuner remain. Raw output kept in `docs/tuning/5e-round1.txt`.
- Next: labels from Stockfish evaluations (Lichess evaluation database), with a quiescence-based quiet filter, instead of game results.

## 5e-5: Stockfish-labelled tuning data
- Round 1 used game results as labels and did not improve the eval at equal depth (fixed depth 7: +3.5 +/- 25.3 Elo). Round 2 uses Stockfish evaluations from the Lichess evaluation database (394,669,566 positions, one JSON line each: FEN without clocks and a list of evaluations with depth and principal variations with cp or mate).
- A Python script (`tools/evals_to_positions.py`) streams the compressed file without unpacking it, samples positions at random (default 2%), keeps only positions whose deepest evaluation has depth >= 20 and is a centipawn score with |cp| <= 1000, and writes `<FEN>;<cp>` lines. Every 20th kept position goes to the validation file.
- The tuner gets a `--label cp` mode: target = sigmoid(K * cp), prediction = sigmoid(K * engine score), loss = mean squared difference. K is fixed (default 0.00368, the scale Lichess uses to turn cp into winning chances) because fitting K here is degenerate: K -> 0 makes both sides 0.5 and the loss vanishes.
- Point of view: it is not certain whether `cp` is stored from White's or the side to move's view, so the tuner computes the correlation between the engine's static score and the label under both conventions on the first 20,000 quiet positions and uses the better one. It prints both values.
- Quiet filter: skip positions where the side to move is in check, and positions where the quiescence search score differs from the static eval by more than 10 cp (a pending capture sequence the static eval cannot see). `quiescence_score(Position&)` is added to the search API for this.
- Known limitations: positions come from Lichess analysis boards (human game positions, not engine games), Stockfish evaluates with its own assumptions, and the split between train and validation is by position (near-duplicates from one game can leak).
- Rejected for now: mixing game results into the label (NNUE-style blend, later), regularisation toward the old values (only if the result drifts), a second move-ordering-aware filter.
- Measurement: correlation printed by the tuner (expected clearly positive for the right convention), validation loss before and after, then a fixed-depth match against `knightlift_pre5e.exe`, and bench 7 nodes to watch the tree size.

## 5e-6: Tuned parameters, round 2 (rejected)
- The Stockfish-labelled round of 5e-5 (4.28 million quiet positions, validation loss 0.0175665 -> 0.0161043, -8.3%) was applied to the evaluation constants on branch `stage-5e6-tuned-r2` and compared with the pre-tuning build.
- Fixed depth 7 (400 games): 108-113-179, -4.3 +/- 25.3 Elo. Bench nodes 107 to 114% of the pre-tuning build.
- Together with round 1 (+3.5 +/- 25.3), two label sources (game results, Stockfish evaluations) gave no gain at equal depth. Most likely explanation: the evaluation feature set limits what re-weighting can achieve (no king attack structure, piece coordination, open files or threat terms), and the search compensates for most remaining errors in quiet positions.
- Decision: values not merged; the trace, data scripts and tuner remain. Raw output kept in `docs/tuning/5e-round2.txt`. Hand-crafted evaluation tuning stops here; the labelled quiet-position pipeline is reused for NNUE (stage 7).
- Limitation: 400 games resolve only differences above about 20 Elo, so a gain below that is not excluded, only not shown.

## 6a: Match script

- Decision: tools/run_match.bat wraps cutechess-cli. Usage: run_match NEW OLD for the timed SPRT (tc 8+0.08, elo0=0 elo1=10, concurrency 6), run_match NEW OLD 7 for the fixed-depth match (tc=inf depth=7, 200 rounds, no SPRT). NEW and OLD are the exe names without the knightlift_ prefix and .exe.
- Where it applies: every strength comparison from stage 6 on.
- Motivation: the command is long and the same mistakes keep recurring (new version not listed first, so SPRT tests the wrong engine; PGN name reused and overwritten). The script puts NEW first by construction and picks a fresh PGN name (NEW_vs_OLD.pgn, then _2, _3, ...).
- Limitations: Windows batch only, paths hard-coded to this machine, no score or game-count logging when a match is killed (still recorded by hand).
- Rejected alternatives: Python wrapper (extra dependency for a 20-line job).
- Measurement method: not applicable, it is infrastructure. Checked by running it with a short fixed-depth match and confirming the engine order and PGN naming.

## 6b: Principal variation search

- Decision: after the first legal move of a node, every other move is searched with a null window (-alpha-1, -alpha). If it returns alpha < score < beta, it is re-searched with the full window (-beta, -alpha). Applied in alphabeta() and search_root(). At the root, beta is infinity, so the re-search condition is score > best so far.
- Where it applies: all nodes of alphabeta(), so also fixed-depth search() with useAlphaBeta; the minimax reference is untouched.
- Motivation: with good ordering (TT move, MVV-LVA, killers, history) the first move is usually best, and null-window searches cut off cheaper than full-window ones. The tuned evals made the tree 1.3-1.5x larger; this is the first of the methods that address that.
- Limitations: pays off only with good ordering; each failed null-window search costs a re-search. Bench node counts are not comparable to the old ones as a strength measure.
- Rejected alternatives: none yet. Not combined with LMR in this step so the effect can be measured alone.
- Measurement method: the alpha-beta == minimax tests must still pass (same score at depth <= 3). Bench 7 and bench 8 (Release) with the depth written next to each number, compared to 5,969,646 and 12,431,983. Strength: timed SPRT, PVS vs knightlift_pre5e.exe copy (new first).

## 6c: Late move reductions

- Decision: in alphabeta(), a move is searched at reduced depth when depth >= 3, it is at least the 4th legal move of the node, it is quiet (no capture, no promotion), the side to move is not in check, and the move does not give check. Reduction R = min(int(0.75 + ln(depth) * ln(moveNumber) / 2.25), depth - 2), from a table precomputed at startup. The reduced search uses the null window; if it returns > alpha, the move is re-searched at full depth with the null window, then with the full window if it is still inside (alpha, beta), as in PVS.
- Where it applies: alphabeta() only when enabled, which is search_iterative. Fixed-depth search() and the minimax reference stay without LMR, like null move and the TT, so the equivalence tests hold.
- Motivation: with ordering by TT move, captures, killers and history, late quiet moves rarely become best. Searching them shallower and verifying only the ones that beat alpha shifts depth to the principal variation. The tuned evals made the tree 1.3-1.5x larger; LMR addresses that together with PVS.
- Limitations: unsound by design, a reduced move that is actually best can be missed (it only gets found if it beats alpha at reduced depth). Depends on ordering quality, which is why history was measured as useless before the stage-5 eval and has to be re-measured. The constants (0.75, 2.25, 4th move) are the usual values, not tuned for this engine.
- Rejected alternatives: none yet. Not combined with reduced reductions for killers, PV nodes or improving positions, so the plain version is measured first.
- Measurement method: bench 7 and bench 8 (Release, depth written next to the number) against 6b: 5,057,085 and 10,162,376. Strength: timed SPRT, 6c vs 6b (new first). Node count alone says nothing about strength here, since LMR changes which lines are searched.

## 6d: Aspiration windows

- Decision: from depth 5 on, search_iterative calls search_root with the window (prev - delta, prev + delta), where prev is the score of the previous iteration and delta starts at 25. On a fail low (score <= alpha) alpha is lowered, on a fail high (score >= beta) beta is raised, in both cases delta doubles (delta += delta / 2 after the first widening is a common alternative, not used). Once delta exceeds 1000 the window becomes the full window. At fail low, beta is moved to (alpha + beta) / 2. A fail high keeps the previous best move.
- Where it applies: search_root() gets alpha and beta parameters; only search_iterative uses a narrow window. Fixed-depth search() passes the full window, so the equivalence tests are unchanged.
- Motivation: a narrow window gives more cutoffs in the whole tree, and the score changes little between consecutive iterations. With PVS and LMR already in, the tree is smaller, so the gain is expected to be small, and the re-search cost is the risk.
- Limitations: each fail costs a re-search of the whole iteration. Mate scores and sharp positions fail often. The initial delta 25 is a usual value, not tuned. TT entries stored from a failed search are bounds, which is correct but can reduce later TT cutoffs.
- Rejected alternatives: none yet. Delta tuning and widening by a different factor are postponed.
- Measurement method: bench 7 and bench 8 (Release, depth written next to the number) against 6c: 1,318,842 and 2,632,590. Bench uses fixed depth, so it sees the node effect of the windows. Strength: timed SPRT, 6d vs 6c (new first).

## 6e: UCI options LMR and Aspiration

- Decision: two UCI check options, LMR and Aspiration (both default true), set from setoption into globals in search.cpp, like NullMove. search_iterative reads them: enable_lmr(gLmrEnabled), and the narrow window is used only if gAspiration is on.
- Where it applies: search_iterative only. Fixed-depth search() and the minimax reference are unaffected.
- Motivation: 6d was inconclusive (+16.9 +/- 19.2 at 700 games). A same-build ablation (option on vs off, same exe) separates the effect of the technique from build differences, and it is needed for the re-measurement of history and null move after LMR.
- Limitations: an option checked on every iteration costs nothing measurable, but each toggle is one more path that must stay tested. The defaults must stay on, so normal play is unchanged.
- Rejected alternatives: compile-time flags (need a separate build per ablation, which brings back the stale-exe risk).
- Measurement method: with defaults the bench totals must equal 6d (bench 7 1,368,691, bench 8 2,741,414). With Aspiration off they must equal 6c (1,318,842 and 2,632,590). With both off they must equal 6b (5,057,085 and 10,162,376). These are the verification of the toggles.

## 6f: History option and ablation script

- Decision: a UCI check option History (default true). With it off, search_iterative skips update_history, so the history table stays at zero and quiet moves are ordered by killers and generation order only, the same condition as the 4f ablation. tools/run_ablation.bat runs one exe against itself with option.X=true against option.X=false (the "on" side first, so the SPRT tests the feature).
- Where it applies: search_iterative only. Fixed-depth search() always updates history, as before.
- Motivation: history was measured as useless with the material-only eval (4e, 4f: +2-3.5% nodes) and has not been re-measured since the stage-5 eval, PVS and LMR. LMR in particular depends on ordering quality, so the value of history can differ now. NullMove already has an option, so both ablations run on the same exe.
- Limitations: turning history off removes the update cost as well as the ordering effect, so the match measures both together. History is not used for LMR decisions yet, which could change its value later.
- Rejected alternatives: a separate scoring path that ignores the table but still updates it (more code for the same conclusion).
- Measurement method: with defaults the bench totals must equal 6d/6e (bench 7 1,368,691, bench 8 2,741,414). Then bench 7 and bench 8 with History off and with NullMove off (depth written next to each total), then a timed SPRT per option on the same exe (feature on vs off).

## 6g: Capture generator for quiescence

- Decision: a new generate_pseudo_legal_captures(pos, list) produces only captures (including en passant) and promotions, in the same relative order as generate_pseudo_legal would emit them. quiescence() uses it instead of generating all pseudo-legal moves and filtering.
- Where it applies: quiescence() in all searches (search() and search_iterative), so the minimax equivalence tests keep covering it.
- Motivation: qsearch is a large share of node time and most generated quiet moves are discarded immediately. Skipping them saves generation and filtering work.
- Limitations: the capture generator shares the pawn and piece loops with the full generator through a template flag (CapturesOnly), so there is no second copy to keep consistent. The compile-time flag costs nothing at runtime. The capture list must contain exactly the moves that the old filter (is_capture or is_promotion) kept, in the same order; a test checks this.
- Rejected alternatives: none yet.
- Measurement method: no behaviour change, so bench 7 and bench 8 totals must stay 1,368,691 and 2,741,414 with all options at default (depth written next to each). A test compares the capture list with the filtered full list (same moves, same order) on the perft FENs. Speed: bench 8 time and nodes/second, several runs each, same session as the previous exe (knightlift_6f.exe). No SPRT needed unless the node counts change.

## 6h: SEE pruning in quiescence

- Decision: a function see_ge(pos, move, threshold) computes the static exchange on the target square (swap algorithm, least valuable attacker first, x-rays through sliders by recomputing slider attacks with the updated occupancy, king captures last and only if the square is no longer defended). In quiescence, a capture is skipped when its SEE is below 0. Promotions and en passant are not pruned. Captures whose victim is worth at least as much as the attacker skip the SEE call, since they cannot lose material in the first exchange step.
- Where it applies: quiescence() when prune is true, so search(), search_iterative() and the minimax reference use the same pruning and the alpha-beta == minimax tests stay valid. The tuner's quiet filter (quiescence_score) changes slightly as well.
- Motivation: qsearch searches every capture, including queen-takes-defended-pawn, which almost never helps the side to move. Skipping losing captures cuts the qsearch tree, which is a large share of node time. This was in the postponed list since 3d.
- Limitations: SEE is static and ignores pins and checks, so it can prune a capture that wins through a tactic (a discovered attack, a mate). Material values for SEE are the simple ones (P 100, N 325, B 325, R 500, Q 975), not the tuned eval values. The qsearch still has no check evasions, so in-check positions are not covered by this step.
- Rejected alternatives: delta pruning (separate step, to be measured alone), SEE threshold above 0.
- Measurement method: bench 7 and bench 8 (Release, depth written next to each) against 6g: 1,368,691 and 2,741,414. Unit tests for see_ge on hand-made positions (equal trade, losing capture, x-ray battery, pinned-attacker case documented as ignored). Strength: timed SPRT, 6h vs 6g (new first). The node count changes, so bench alone says nothing about strength.

## 6i: Reverse futility pruning

- Decision: in alphabeta(), at non-PV nodes (beta - alpha == 1) with depth <= 6, not in check, and beta not a mate score, if the static eval minus 80 * depth is at least beta, the node returns the static eval without searching. The static eval is now computed once per node (when not in check) and shared with the null move condition. A UCI check option RFP (default true) switches it off for ablation.
- Where it applies: search_iterative only, like null move, LMR and the TT, so the minimax equivalence tests stay valid.
- Motivation: at shallow depth, a position that is already far above beta by static eval almost always fails high anyway. The null move saved only 1.36-1.51x after PVS and LMR (6f), so another cheap pruning of the same kind is the next candidate for shrinking the tree.
- Limitations: the margin 80 * depth is a common value, untuned, and the static eval ignores tactics (a quiet-looking position with a hanging piece can be wrongly pruned). Returns the static eval (fail-soft). Not applied when in check or in PV nodes.
- Rejected alternatives: forward futility pruning of individual moves (separate step), margin tuning (postponed).
- Measurement method: with RFP off, bench 7 and bench 8 must equal 6h (995,161 and 2,096,444, depth written next to each), which verifies the shared eval refactor. With RFP on, node counts change; strength by timed SPRT, 6i vs 6h (new first), and the same-exe ablation if the result is unclear.

## 6j: Futility pruning

- Decision: in alphabeta(), at non-PV nodes (beta - alpha == 1) with depth <= 3, not in check, and alpha not a mate score, let futilityValue = static eval + 100 * depth. If futilityValue <= alpha, quiet moves after the first legal move are skipped when they do not give check. If any move is skipped, best is raised to at least futilityValue, so the stored TT bound and the returned fail-soft value stay consistent. A UCI check option Futility (default true) switches it off for ablation.
- Where it applies: search_iterative only, like RFP, null move and LMR.
- Motivation: RFP prunes nodes whose eval is far above beta. Futility is the mirror case: near the leaves, a quiet move rarely lifts a position that is far below alpha. After PVS, LMR, SEE and RFP the tree is still dominated by shallow nodes in non-PV lines.
- Limitations: the margin 100 * depth and the depth limit 3 are common values, untuned. Quiet moves that create threats without giving check (a pawn push attacking a queen) can be wrongly skipped. Skipped moves do not increase the legal move count used by LMR.
- Rejected alternatives: pruning captures (covered by SEE in quiescence), margin tuning (planned as its own step with UCI spin options).
- Measurement method: with Futility off, bench 7 and bench 8 must equal 6i (518,166 and 1,039,981, depth written next to each). With Futility on, node counts change; strength by timed SPRT, 6j vs 6i (new first), plus a same-exe ablation if the result is unclear.






## Sources
- Chess Programming Wiki: Bitboards, Square Mapping Considerations.
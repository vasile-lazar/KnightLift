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

## Sources
- Chess Programming Wiki: Bitboards, Square Mapping Considerations.
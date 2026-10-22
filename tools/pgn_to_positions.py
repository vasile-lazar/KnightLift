"""Extract labelled positions from a PGN file for Texel tuning.

Each output line is "<FEN>;<result>", result from White's point of view
(1.0, 0.5, 0.0). Every 10th game goes to the validation file, so no game
appears in both sets.
"""
import argparse
import random

import chess
import chess.pgn

RESULTS = {"1-0": 1.0, "0-1": 0.0, "1/2-1/2": 0.5}


def sample_game(game, skip_plies, per_game, rng):
    board = game.board()
    fens = []
    ply = 0
    for move in game.mainline_moves():
        special = board.is_capture(move) or move.promotion is not None
        board.push(move)
        ply += 1
        if ply <= skip_plies or special or board.is_check():
            continue
        fens.append(board.fen())
    if len(fens) > per_game:
        fens = rng.sample(fens, per_game)
    return fens


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pgn", help="input PGN file")
    ap.add_argument("out_prefix", help="writes <prefix>.train.txt and <prefix>.val.txt")
    ap.add_argument("--per-game", type=int, default=8)
    ap.add_argument("--skip-plies", type=int, default=10)
    ap.add_argument("--max-games", type=int, default=0, help="0 = read all games")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    games_read = 0
    games_used = 0
    per_result = {1.0: 0, 0.5: 0, 0.0: 0}
    written = {"train": 0, "val": 0}

    with open(args.pgn, encoding="utf-8", errors="replace") as pgn, \
            open(args.out_prefix + ".train.txt", "w") as train, \
            open(args.out_prefix + ".val.txt", "w") as val:
        while args.max_games == 0 or games_read < args.max_games:
            game = chess.pgn.read_game(pgn)
            if game is None:
                break
            games_read += 1

            result = RESULTS.get(game.headers.get("Result", "*"))
            if result is None or game.headers.get("Termination", "Normal") != "Normal":
                continue
            fens = sample_game(game, args.skip_plies, args.per_game, rng)
            if not fens:
                continue

            games_used += 1
            name = "val" if games_read % 10 == 0 else "train"
            target = val if name == "val" else train
            for fen in fens:
                target.write(f"{fen};{result}\n")
            written[name] += len(fens)
            per_result[result] += len(fens)

    total = written["train"] + written["val"]
    print(f"games read: {games_read}, games used: {games_used}")
    print(f"positions: train {written['train']}, val {written['val']}")
    if total:
        print("result share (White POV): "
              f"win {per_result[1.0] / total:.3f}, draw {per_result[0.5] / total:.3f}, "
              f"loss {per_result[0.0] / total:.3f}")


if __name__ == "__main__":
    main()
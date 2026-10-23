"""Extract Stockfish-labelled positions from the Lichess evaluation database.

Input: lichess_db_eval.jsonl.zst (streamed, no need to unpack it).
Each output line is "<FEN>;<cp>", where cp is the Stockfish evaluation as stored
in the database (its point of view is detected later by the tuner). Positions
with mate scores, shallow evaluations or very lopsided evaluations are skipped.
Every 20th kept position goes to the validation file.
"""
import argparse
import io
import json
import random

import zstandard


def best_cp(entry, min_depth):
    """cp of the deepest evaluation, or None if it is shallow or a mate score."""
    best = None
    for ev in entry.get("evals", []):
        pvs = ev.get("pvs") or []
        if pvs and (best is None or ev.get("depth", 0) > best[0]):
            best = (ev.get("depth", 0), pvs[0])
    if best is None or best[0] < min_depth or "cp" not in best[1]:
        return None
    return best[1]["cp"]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("zst", help="lichess_db_eval.jsonl.zst")
    ap.add_argument("out_prefix", help="writes <prefix>.train.txt and <prefix>.val.txt")
    ap.add_argument("--sample-rate", type=float, default=0.02)
    ap.add_argument("--max-positions", type=int, default=8_000_000)
    ap.add_argument("--min-depth", type=int, default=20)
    ap.add_argument("--max-cp", type=int, default=1000)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    lines = 0
    kept = 0
    written = {"train": 0, "val": 0}

    dctx = zstandard.ZstdDecompressor(max_window_size=2**31)
    with open(args.zst, "rb") as fh, \
            open(args.out_prefix + ".train.txt", "w") as train, \
            open(args.out_prefix + ".val.txt", "w") as val:
        try:
            reader = dctx.stream_reader(fh, read_across_frames=True)
        except TypeError:
            reader = dctx.stream_reader(fh)
        text = io.TextIOWrapper(reader, encoding="utf-8")
        for line in text:
            lines += 1
            if lines % 20_000_000 == 0:
                print(f"read {lines} lines, kept {kept}", flush=True)
            if rng.random() >= args.sample_rate:
                continue
            entry = json.loads(line)
            cp = best_cp(entry, args.min_depth)
            if cp is None or abs(cp) > args.max_cp:
                continue
            kept += 1
            name = "val" if kept % 20 == 0 else "train"
            (val if name == "val" else train).write(f"{entry['fen']};{cp}\n")
            written[name] += 1
            if kept >= args.max_positions:
                break

    print(f"lines read: {lines}, positions kept: {kept} "
          f"(train {written['train']}, val {written['val']})")


if __name__ == "__main__":
    main()
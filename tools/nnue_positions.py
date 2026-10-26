"""Extract quiet Stockfish-labelled positions from the Lichess evaluation database for NNUE.

Output lines are "<FEN>;<cp>", same format as evals_to_positions.py. cp is White's point of view.
A position is kept if: the deepest evaluation has depth >= --min-depth, |cp| <= --max-cp,
the side to move is not in check, and the first move of the database line is neither a
capture nor a promotion (the position is "quiet").
"""
import argparse
import collections
import io
import json
import multiprocessing as mp
import os
import random

import chess
import zstandard


def best_pv(entry, min_depth):
    best = None
    for ev in entry.get("evals", []):
        pvs = ev.get("pvs") or []
        if pvs and (best is None or ev.get("depth", 0) > best[0]):
            best = (ev.get("depth", 0), pvs[0])
    if best is None or best[0] < min_depth or "cp" not in best[1]:
        return None
    return best[1]


def process_chunk(task):
    idx, lines, sample_rate, min_depth, max_cp, seed = task
    rng = random.Random(seed * 1_000_003 + idx)  # reproducible, independent of worker order
    out = []
    for line in lines:
        if rng.random() >= sample_rate:
            continue
        try:
            entry = json.loads(line)
            pv = best_pv(entry, min_depth)
            if pv is None or abs(pv["cp"]) > max_cp:
                continue
            moves = pv.get("line", "").split()
            if not moves:
                continue
            board = chess.Board(entry["fen"])
            move = chess.Move.from_uci(moves[0])
            if board.is_check() or move.promotion or board.is_capture(move):
                continue
            out.append(f"{entry['fen']};{pv['cp']}")
        except (ValueError, KeyError):
            continue
    return len(lines), out


def chunks(text, size):
    chunk = []
    for line in text:
        chunk.append(line)
        if len(chunk) == size:
            yield chunk
            chunk = []
    if chunk:
        yield chunk


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("zst", help="lichess_db_eval.jsonl.zst")
    ap.add_argument("out_prefix", help="writes <prefix>.train.txt and <prefix>.val.txt")
    ap.add_argument("--sample-rate", type=float, default=0.2)
    ap.add_argument("--max-positions", type=int, default=40_000_000)
    ap.add_argument("--min-depth", type=int, default=20)
    ap.add_argument("--max-cp", type=int, default=1000)
    ap.add_argument("--val-every", type=int, default=100, help="every Nth kept position goes to validation")
    ap.add_argument("--workers", type=int, default=max(1, (os.cpu_count() or 2) - 2))
    ap.add_argument("--chunk", type=int, default=20_000)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    state = {"lines": 0, "kept": 0, "train": 0, "val": 0, "done": False}

    dctx = zstandard.ZstdDecompressor(max_window_size=2**31)
    with open(args.zst, "rb") as fh, \
            open(args.out_prefix + ".train.txt", "w") as train, \
            open(args.out_prefix + ".val.txt", "w") as val:
        try:
            reader = dctx.stream_reader(fh, read_across_frames=True)
        except TypeError:
            reader = dctx.stream_reader(fh)
        text = io.TextIOWrapper(reader, encoding="utf-8")

        def write(result):
            n, out = result
            state["lines"] += n
            for entry_text in out:
                if state["done"]:
                    break
                state["kept"] += 1
                name = "val" if state["kept"] % args.val_every == 0 else "train"
                (val if name == "val" else train).write(entry_text + "\n")
                state[name] += 1
                if state["kept"] >= args.max_positions:
                    state["done"] = True

        pending = collections.deque()
        window = args.workers * 4  # bounded queue, so memory stays small
        with mp.Pool(args.workers) as pool:
            for idx, chunk in enumerate(chunks(text, args.chunk)):
                task = (idx, chunk, args.sample_rate, args.min_depth, args.max_cp, args.seed)
                pending.append(pool.apply_async(process_chunk, (task,)))
                if len(pending) >= window:
                    write(pending.popleft().get())
                if state["done"]:
                    break
                if idx % 100 == 0:
                    print(f"read {state['lines']} lines, kept {state['kept']}", flush=True)
            while pending and not state["done"]:
                write(pending.popleft().get())

    print(f"lines read: {state['lines']}, positions kept: {state['kept']} "
          f"(train {state['train']}, val {state['val']})")


if __name__ == "__main__":
    main()
"""Pack a "FEN;cp" text file into numpy arrays for NNUE training.

Writes <out_prefix>.boards.npy (uint8, N x 64), <out_prefix>.stm.npy (uint8) and
<out_prefix>.cp.npy (int16, White's point of view).
Board codes: 0 empty, 1-6 white P N B R Q K, 7-12 black p n b r q k. Square 0 = a1, 63 = h8.
"""
import argparse

import numpy as np

SYMBOLS = " PNBRQKpnbrqk"
CODE = {c: i for i, c in enumerate(SYMBOLS) if i}


def placement_from_row(row):
    ranks = []
    for r in range(7, -1, -1):
        text = ""
        empty = 0
        for f in range(8):
            c = int(row[r * 8 + f])
            if c == 0:
                empty += 1
            else:
                if empty:
                    text += str(empty)
                    empty = 0
                text += SYMBOLS[c]
        if empty:
            text += str(empty)
        ranks.append(text)
    return "/".join(ranks)


def count_lines(path):
    n = 0
    with open(path, "rb") as fh:
        while True:
            block = fh.read(1 << 24)
            if not block:
                break
            n += block.count(b"\n")
    return n


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("txt", help='text file with lines "FEN;cp"')
    ap.add_argument("out_prefix")
    args = ap.parse_args()

    n = count_lines(args.txt)
    print(f"{n} positions")
    boards = np.lib.format.open_memmap(args.out_prefix + ".boards.npy", mode="w+",
                                       dtype=np.uint8, shape=(n, 64))
    stm = np.zeros(n, dtype=np.uint8)
    cp = np.zeros(n, dtype=np.int16)
    buf = bytearray(64)

    i = 0
    with open(args.txt) as fh:
        for line in fh:
            if i >= n:
                break  # partial last line without newline: ignored
            fen, label = line.rstrip("\n").split(";")
            fields = fen.split(" ")
            placement = fields[0]

            buf[:] = bytes(64)
            rank, file = 7, 0
            for ch in placement:
                if ch == "/":
                    rank -= 1
                    file = 0
                elif ch.isdigit():
                    file += int(ch)
                else:
                    buf[rank * 8 + file] = CODE[ch]
                    file += 1

            boards[i] = np.frombuffer(buf, dtype=np.uint8)
            stm[i] = 0 if fields[1] == "w" else 1
            cp[i] = int(label)

            if i < 1000:
                assert placement_from_row(boards[i]) == placement, fen
                assert ("w" if stm[i] == 0 else "b") == fields[1], fen
            i += 1
            if i % 5_000_000 == 0:
                print(f"{i} done", flush=True)

    boards.flush()
    np.save(args.out_prefix + ".stm.npy", stm)
    np.save(args.out_prefix + ".cp.npy", cp)

    pieces = (boards[:100_000] != 0).sum(axis=1)
    print(f"packed {i} of {n} positions")
    print(f"white to move: {100.0 * (stm == 0).mean():.1f}%")
    print(f"mean |cp|: {np.abs(cp.astype(np.int32)).mean():.1f}, max |cp|: {np.abs(cp.astype(np.int32)).max()}")
    print(f"mean pieces per position (first 100000): {pieces.mean():.1f}")


if __name__ == "__main__":
    main()
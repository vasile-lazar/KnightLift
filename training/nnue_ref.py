"""Integer reference implementation of the KnightLift NNUE.

The C++ inference must reproduce evaluate() exactly. Board codes: 0 empty, 1-6 white
P N B R Q K, 7-12 black p n b r q k. Square 0 = a1, 63 = h8.
"""
import struct

import numpy as np

NUM_FEATURES = 768
QA = 512
QB = 512
SCALE = 400
MAGIC = b"KNNUE001"
SYMBOLS = " PNBRQKpnbrqk"
CODE = {c: i for i, c in enumerate(SYMBOLS) if i}


def feature_table():
    """table[perspective, piece code, square] -> feature index (768 = empty square)."""
    table = np.full((2, 13, 64), NUM_FEATURES, dtype=np.int64)
    for persp in range(2):
        for code in range(1, 13):
            color = (code - 1) // 6
            ptype = (code - 1) % 6
            rel = 0 if color == persp else 1
            for sq in range(64):
                s = sq if persp == 0 else sq ^ 56
                table[persp, code, sq] = (rel * 6 + ptype) * 64 + s
    return table


TABLE = feature_table()


def trunc_div(a, b):
    """Integer division truncating toward zero, like C++ (scalar)."""
    q = abs(a) // b
    return q if a >= 0 else -q


def parse_fen(fen):
    """-> (list of 64 board codes, side to move: 0 white, 1 black)."""
    fields = fen.split(" ")
    board = [0] * 64
    rank, file = 7, 0
    for ch in fields[0]:
        if ch == "/":
            rank -= 1
            file = 0
        elif ch.isdigit():
            file += int(ch)
        else:
            board[rank * 8 + file] = CODE[ch]
            file += 1
    return board, (0 if fields[1] == "w" else 1)


class QuantNet:
    def __init__(self, w1, b1, w2, b2):
        self.w1 = np.asarray(w1, dtype=np.int64)    # (768, H)
        self.b1 = np.asarray(b1, dtype=np.int64)    # (H,)
        self.w2 = np.asarray(w2, dtype=np.int64)    # (2H,)
        self.b2 = int(b2)
        self.hidden = self.w1.shape[1]

    def accumulator(self, board, persp):
        acc = self.b1.copy()
        for sq in range(64):
            code = board[sq]
            if code:
                acc += self.w1[TABLE[persp, code, sq]]
        return acc

    def evaluate(self, board, stm):
        """Score in centipawns from the side to move's point of view (scalar reference)."""
        us = self.accumulator(board, stm)
        them = self.accumulator(board, 1 - stm)
        hidden = np.concatenate([np.clip(us, 0, QA), np.clip(them, 0, QA)])
        total = int(hidden @ self.w2) + self.b2
        return trunc_div(total * SCALE, QA * QB)

    def evaluate_batch(self, boards, stm, chunk=1000):
        """Same result for many positions. Returns (scores, max |accumulator|, max |sum|)."""
        w1p = np.vstack([self.w1, np.zeros((1, self.hidden), dtype=np.int64)]).astype(np.int32)
        scores = np.zeros(boards.shape[0], dtype=np.int64)
        max_acc = 0
        max_sum = 0
        sq = np.arange(64)[None, :]
        for start in range(0, boards.shape[0], chunk):
            b = boards[start:start + chunk].astype(np.int64)
            us_p = stm[start:start + chunk].astype(np.int64)[:, None]
            acc_us = self.b1 + w1p[TABLE[us_p, b, sq]].sum(axis=1, dtype=np.int64)
            acc_them = self.b1 + w1p[TABLE[1 - us_p, b, sq]].sum(axis=1, dtype=np.int64)
            hidden = np.concatenate([np.clip(acc_us, 0, QA), np.clip(acc_them, 0, QA)], axis=1)
            total = hidden @ self.w2 + self.b2
            scores[start:start + chunk] = np.sign(total * SCALE) * (np.abs(total * SCALE) // (QA * QB))
            max_acc = max(max_acc, int(np.abs(acc_us).max()), int(np.abs(acc_them).max()))
            max_sum = max(max_sum, int(np.abs(total).max()))
        return scores, max_acc, max_sum


def write_network(path, net):
    with open(path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<5i", NUM_FEATURES, net.hidden, QA, QB, SCALE))
        f.write(net.w1.astype("<i2").tobytes())
        f.write(net.b1.astype("<i2").tobytes())
        f.write(net.w2.astype("<i2").tobytes())
        f.write(struct.pack("<i", net.b2))


def read_network(path):
    with open(path, "rb") as f:
        assert f.read(8) == MAGIC, "bad magic"
        inputs, hidden, qa, qb, scale = struct.unpack("<5i", f.read(20))
        assert (inputs, qa, qb, scale) == (NUM_FEATURES, QA, QB, SCALE)
        w1 = np.frombuffer(f.read(inputs * hidden * 2), dtype="<i2").reshape(inputs, hidden)
        b1 = np.frombuffer(f.read(hidden * 2), dtype="<i2")
        w2 = np.frombuffer(f.read(2 * hidden * 2), dtype="<i2")
        (b2,) = struct.unpack("<i", f.read(4))
        assert f.read() == b"", "trailing data"
    return QuantNet(w1, b1, w2, b2)
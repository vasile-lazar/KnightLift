"""Quantise a trained checkpoint, write the network file, check it, write golden vectors."""
import argparse

import numpy as np
import torch

import nnue_ref as ref

K = 0.00368

EXTRA_FENS = [
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "8/8/4k3/8/8/4K3/4P3/8 w - - 0 1",
    "4k3/8/8/8/8/8/8/4K2R b K - 0 1",
]


def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-x))


def float_scores(boards, stm, w1, b1, w2, b2, chunk=1000):
    """Float network (no quantisation) in cp from the side to move's point of view."""
    w1p = np.vstack([w1, np.zeros((1, w1.shape[1]))])
    out = np.zeros(boards.shape[0])
    sq = np.arange(64)[None, :]
    for start in range(0, boards.shape[0], chunk):
        b = boards[start:start + chunk].astype(np.int64)
        us_p = stm[start:start + chunk].astype(np.int64)[:, None]
        acc_us = b1 + w1p[ref.TABLE[us_p, b, sq]].sum(axis=1)
        acc_them = b1 + w1p[ref.TABLE[1 - us_p, b, sq]].sum(axis=1)
        hidden = np.concatenate([np.clip(acc_us, 0, 1), np.clip(acc_them, 0, 1)], axis=1)
        out[start:start + chunk] = (hidden @ w2 + b2) * ref.SCALE
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ckpt", required=True, help="checkpoint, for example nnue_v1_best.pt")
    ap.add_argument("--out", required=True, help="network file to write (.nnue)")
    ap.add_argument("--val", required=True, help="prefix of the packed validation files")
    ap.add_argument("--val-txt", required=True, help="validation text file (FEN;cp), for golden vectors")
    ap.add_argument("--golden", required=True, help="golden vector file to write (FEN;score)")
    ap.add_argument("--samples", type=int, default=100_000)
    ap.add_argument("--golden-count", type=int, default=200)
    args = ap.parse_args()

    sd = torch.load(args.ckpt, map_location="cpu")["model"]
    w1_f = sd["ft.weight"].numpy().T.astype(np.float64)       # (768, H)
    b1_f = sd["ft.bias"].numpy().astype(np.float64)
    w2_f = sd["out.weight"].numpy()[0].astype(np.float64)     # (2H,)
    b2_f = float(sd["out.bias"].numpy()[0])

    w1 = np.rint(w1_f * ref.QA)
    b1 = np.rint(b1_f * ref.QA)
    w2 = np.rint(w2_f * ref.QB)
    b2 = int(round(b2_f * ref.QA * ref.QB))
    for name, arr in (("w1", w1), ("b1", b1), ("w2", w2)):
        assert np.abs(arr).max() <= 32767, f"{name} does not fit int16"
    # Worst case over all positions: a position has at most 32 active features.
    acc_bound = np.sort(np.abs(w1), axis=0)[-32:].sum(axis=0) + np.abs(b1)
    sum_bound = ref.QA * np.abs(w2).sum() + abs(b2)
    print(f"worst-case |accumulator| bound {acc_bound.max():.0f} (int16 limit 32767), "
          f"worst-case |sum| bound {sum_bound:.0f} (int32 limit 2147483647)")
    assert acc_bound.max() <= 32767, "accumulator could overflow int16"
    assert sum_bound <= 2**31 - 1, "sum could overflow int32"
    print(f"max |w1| {np.abs(w1).max():.0f}, max |b1| {np.abs(b1).max():.0f}, "
          f"max |w2| {np.abs(w2).max():.0f}, b2 {b2}")

    net = ref.QuantNet(w1, b1, w2, b2)
    ref.write_network(args.out, net)
    back = ref.read_network(args.out)
    assert (np.array_equal(back.w1, net.w1) and np.array_equal(back.b1, net.b1)
            and np.array_equal(back.w2, net.w2) and back.b2 == net.b2), "file does not read back"

    boards = np.load(args.val + ".boards.npy", mmap_mode="r")[:args.samples]
    stm = np.load(args.val + ".stm.npy")[:args.samples]
    cp_white = np.load(args.val + ".cp.npy")[:args.samples].astype(np.float64)
    boards = np.ascontiguousarray(boards)

    q_scores, max_acc, max_sum = net.evaluate_batch(boards, stm)
    f_scores = float_scores(boards, stm, w1_f, b1_f, w2_f, b2_f)

    # The scalar and the batched integer implementations must agree.
    for i in range(min(200, len(stm))):
        assert net.evaluate(boards[i].tolist(), int(stm[i])) == q_scores[i], f"scalar mismatch at {i}"

    cp_stm = np.where(stm == 0, cp_white, -cp_white)
    target = sigmoid(K * cp_stm)
    loss_f = np.mean((sigmoid(K * f_scores) - target) ** 2)
    loss_q = np.mean((sigmoid(K * q_scores) - target) ** 2)
    diff = np.abs(f_scores - q_scores)
    print(f"validation loss on {len(stm)} positions: float {loss_f:.6f}, quantised {loss_q:.6f}")
    print(f"float vs quantised score difference: mean {diff.mean():.2f} cp, max {diff.max():.1f} cp")
    print(f"max |accumulator| {max_acc} (int16 limit 32767), max |sum| {max_sum} (int32 limit 2147483647)")

    lines = []
    with open(args.val_txt) as fh:
        for line in fh:
            if len(lines) >= args.golden_count:
                break
            lines.append(line.split(";")[0])
    lines.extend(EXTRA_FENS)
    with open(args.golden, "w") as out:
        for fen in lines:
            board, side = ref.parse_fen(fen)
            out.write(f"{fen};{net.evaluate(board, side)}\n")
    print(f"wrote {len(lines)} golden vectors to {args.golden}")


if __name__ == "__main__":
    main()
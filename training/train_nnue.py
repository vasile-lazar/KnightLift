"""Train the NNUE network on packed positions (see tools/pack_positions.py).

Network: 768 inputs -> H (feature transformer shared by both perspectives)
-> clamp [0, 1] -> concat(side to move, other side) 2H -> 1.
"""
import argparse
import math
import time

import numpy as np
import torch
import torch.nn as nn

K = 0.00368        # same constant as the tuner: probability = sigmoid(K * cp)
SCALE = 400.0      # network output 1.0 corresponds to 400 cp
NUM_FEATURES = 768


def make_feature_table(device):
    """table[perspective, piece code, square] -> feature index; 768 means 'empty square'."""
    table = torch.full((2, 13, 64), NUM_FEATURES, dtype=torch.int64)
    for persp in range(2):
        for code in range(1, 13):
            color = (code - 1) // 6        # 0 white, 1 black
            ptype = (code - 1) % 6         # P N B R Q K
            rel = 0 if color == persp else 1   # 0 = own piece, 1 = enemy piece
            for sq in range(64):
                s = sq if persp == 0 else sq ^ 56
                table[persp, code, sq] = (rel * 6 + ptype) * 64 + s
    return table.to(device)


def make_inputs(boards, stm, table, arange64):
    b = boards.long()                      # (B, 64)
    us = stm.long()[:, None]               # (B, 1): perspective of the side to move
    them = 1 - us
    sq = arange64[None, :]                 # (1, 64)
    f_us = table[us, b, sq]                # (B, 64)
    f_them = table[them, b, sq]
    x_us = torch.zeros(b.shape[0], NUM_FEATURES + 1, device=boards.device)
    x_them = torch.zeros_like(x_us)
    x_us.scatter_(1, f_us, 1.0)
    x_them.scatter_(1, f_them, 1.0)
    return x_us[:, :NUM_FEATURES], x_them[:, :NUM_FEATURES]


def make_targets(cp, stm):
    cp = cp.float()
    cp_stm = torch.where(stm == 0, cp, -cp)   # cp is White's point of view
    return torch.sigmoid(K * cp_stm)


class Net(nn.Module):
    def __init__(self, hidden):
        super().__init__()
        self.ft = nn.Linear(NUM_FEATURES, hidden)
        self.out = nn.Linear(2 * hidden, 1)

    def forward(self, x_us, x_them):
        h = torch.cat([self.ft(x_us), self.ft(x_them)], dim=1)
        h = torch.clamp(h, 0.0, 1.0)
        return self.out(h).squeeze(1)


def load_split(prefix, device):
    boards = torch.from_numpy(np.load(prefix + ".boards.npy")).to(device)
    stm = torch.from_numpy(np.load(prefix + ".stm.npy")).to(device)
    cp = torch.from_numpy(np.load(prefix + ".cp.npy")).to(device)
    return boards, stm, cp


def batch_loss_sum(model, boards, stm, cp, table, arange64):
    x_us, x_them = make_inputs(boards, stm, table, arange64)
    pred = torch.sigmoid(K * SCALE * model(x_us, x_them))
    target = make_targets(cp, stm)
    return ((pred - target) ** 2).sum()


@torch.no_grad()
def evaluate(model, data, table, arange64, batch):
    boards, stm, cp = data
    model.eval()
    total = torch.zeros((), device=boards.device)
    for i in range(0, boards.shape[0], batch):
        total += batch_loss_sum(model, boards[i:i + batch], stm[i:i + batch],
                                cp[i:i + batch], table, arange64)
    model.train()
    return (total / boards.shape[0]).item()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--train", required=True, help="prefix of the packed training files")
    ap.add_argument("--val", required=True, help="prefix of the packed validation files")
    ap.add_argument("--out", required=True, help="checkpoint prefix: <out>_best.pt, <out>_last.pt")
    ap.add_argument("--hidden", type=int, default=256)
    ap.add_argument("--batch", type=int, default=16384)
    ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--lr-final", type=float, default=1e-5)
    ap.add_argument("--resume", default=None, help="<out>_last.pt to continue from")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()

    assert torch.cuda.is_available(), "no CUDA device"
    device = torch.device("cuda")
    torch.manual_seed(args.seed)
    torch.set_float32_matmul_precision("high")   # TF32 matrix products

    train = load_split(args.train, device)
    val = load_split(args.val, device)
    n_train = train[0].shape[0]
    print(f"train {n_train} positions, val {val[0].shape[0]} positions", flush=True)

    t_val = make_targets(val[2], val[1])
    baseline = ((t_val - t_val.mean()) ** 2).mean().item()
    print(f"baseline validation loss (always predict the mean): {baseline:.6f}", flush=True)

    table = make_feature_table(device)
    arange64 = torch.arange(64, device=device)

    model = Net(args.hidden).to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    steps_per_epoch = n_train // args.batch
    total_steps = steps_per_epoch * args.epochs
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=total_steps,
                                                       eta_min=args.lr_final)
    start_epoch = 0
    best = math.inf
    if args.resume:
        ckpt = torch.load(args.resume, map_location=device)
        model.load_state_dict(ckpt["model"])
        opt.load_state_dict(ckpt["opt"])
        sched.load_state_dict(ckpt["sched"])
        start_epoch = ckpt["epoch"]
        best = ckpt["best"]
        print(f"resumed at epoch {start_epoch}", flush=True)

    for epoch in range(start_epoch, args.epochs):
        t0 = time.time()
        perm = torch.randperm(n_train, device=device)
        running = torch.zeros((), device=device)
        for step in range(steps_per_epoch):
            idx = perm[step * args.batch:(step + 1) * args.batch]
            loss = batch_loss_sum(model, train[0][idx], train[1][idx], train[2][idx],
                                  table, arange64) / args.batch
            opt.zero_grad(set_to_none=True)
            loss.backward()
            opt.step()
            sched.step()
            running += loss.detach()
        train_loss = (running / steps_per_epoch).item()
        val_loss = evaluate(model, val, table, arange64, 65536)
        secs = time.time() - t0
        print(f"epoch {epoch + 1}/{args.epochs}  train {train_loss:.6f}  val {val_loss:.6f}  "
              f"lr {sched.get_last_lr()[0]:.2e}  {secs:.0f}s  "
              f"{steps_per_epoch * args.batch / secs / 1e6:.2f}M pos/s", flush=True)

        if val_loss < best:
            best = val_loss
            torch.save({"model": model.state_dict(), "hidden": args.hidden,
                        "val_loss": val_loss, "epoch": epoch + 1}, args.out + "_best.pt")
        torch.save({"model": model.state_dict(), "opt": opt.state_dict(),
                    "sched": sched.state_dict(), "epoch": epoch + 1, "best": best,
                    "hidden": args.hidden}, args.out + "_last.pt")

    print(f"best validation loss {best:.6f}")


if __name__ == "__main__":
    main()
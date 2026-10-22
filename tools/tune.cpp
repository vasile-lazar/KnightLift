// Texel tuner: fits the evaluation parameters to game results.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <cstdlib>

#include "../engine/src/attacks.hpp"
#include "../engine/src/eval.hpp"
#include "../engine/src/eval_params.hpp"
#include "../engine/src/eval_tables.hpp"
#include "../engine/src/position.hpp"
#include "../engine/src/search.hpp"


using namespace KnightLift;

namespace {
    constexpr std::size_t kP = EvalParams::kNumParams;
    constexpr std::size_t kW = 2 * kP; // weights: [2*i] = middlegame, [2*i+1] = endgame
    constexpr double kMaxPhase = 24.0;
    static_assert(EvalTables::kMaxPhase == 24);

    struct Dataset {
        std::vector<std::uint64_t> start{0}; // entry range of position i: [start[i], start[i+1])
        std::vector<std::uint16_t> index;
        std::vector<std::int16_t> coef;
        std::vector<std::int16_t> constMg;
        std::vector<std::uint8_t> phase;
        std::vector<float> result;
        std::vector<float> raw; // cp labels as stored (cp mode)
        std::vector<std::uint8_t> whiteToMove; // side to move per position (cp mode)

        std::size_t size() const { return result.size(); }
    };

    double score_of(const Dataset &d, std::size_t i, const std::vector<double> &w) {
        double mg = d.constMg[i];
        double eg = 0.0;
        for (std::uint64_t e = d.start[i]; e < d.start[i + 1]; ++e) {
            const double c = d.coef[e];
            mg += c * w[2 * d.index[e]];
            eg += c * w[2 * d.index[e] + 1];
        }
        const double ph = d.phase[i];
        return (mg * ph + eg * (kMaxPhase - ph)) / kMaxPhase;
    }

    inline double predict(double s, double k) { return 1.0 / (1.0 + std::exp(-k * s)); }

    struct LoadOptions {
        bool cpLabels = false;
        int quietMargin = 10;
    };

    struct Calib {
        double eval; // engine score, White's point of view
        double cp; // label as stored
        bool whiteToMove;
    };

    bool load(const std::string &path, Dataset &d, std::size_t maxPositions, const LoadOptions &opt,
              const std::vector<double> &w0, const ParamArray &params0, std::size_t &mismatches,
              std::vector<Calib> *calib) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "cannot open " << path << '\n';
            return false;
        }
        Position pos;
        std::string line;
        std::size_t bad = 0;
        std::size_t skipped = 0;
        while (std::getline(in, line)) {
            if (maxPositions != 0 && d.size() >= maxPositions) break;
            const std::size_t sep = line.rfind(';');
            if (sep == std::string::npos ||
                !pos.set_from_fen(std::string_view(line).substr(0, sep))) {
                ++bad;
                continue;
            }
            const float label = std::stof(line.substr(sep + 1));

            if (opt.cpLabels) {
                // Quiet filter: not in check, and no capture sequence that changes the static score.
                if (pos.in_check()) {
                    ++skipped;
                    continue;
                }
                const int stat = evaluate(pos);
                if (std::abs(quiescence_score(pos) - stat) > opt.quietMargin) {
                    ++skipped;
                    continue;
                }
                const bool white = pos.side_to_move() == Color::White;
                if (calib != nullptr && calib->size() < 20000) {
                    calib->push_back({static_cast<double>(white ? stat : -stat), label, white});
                }
                d.raw.push_back(label);
                d.whiteToMove.push_back(white ? 1 : 0);
                d.result.push_back(0.5f); // replaced by finalize_cp once K is known
            } else {
                d.result.push_back(label);
            }

            const EvalTrace tr = eval_trace(pos);
            for (std::size_t i = 0; i < kP; ++i) {
                if (tr.coef[i] != 0) {
                    d.index.push_back(static_cast<std::uint16_t>(i));
                    d.coef.push_back(static_cast<std::int16_t>(tr.coef[i]));
                }
            }
            d.start.push_back(d.index.size());
            d.constMg.push_back(static_cast<std::int16_t>(tr.constMg));
            d.phase.push_back(static_cast<std::uint8_t>(tr.phase));

            if (d.size() <= 2000) {
                const double stored = score_of(d, d.size() - 1, w0);
                const double reference = eval_from_params(tr, params0, Color::White);
                if (std::abs(stored - reference) > 1.0) ++mismatches;
            }
        }
        if (bad != 0) std::cerr << bad << " unreadable lines in " << path << '\n';
        if (skipped != 0) std::cout << skipped << " non-quiet positions skipped in " << path << '\n';
        return true;
    }

    double correlation(const std::vector<Calib> &c, bool sideToMoveView) {
        double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
        const double n = static_cast<double>(c.size());
        for (const Calib &e: c) {
            const double y = (sideToMoveView && !e.whiteToMove) ? -e.cp : e.cp;
            sx += e.eval;
            sy += y;
            sxx += e.eval * e.eval;
            syy += y * y;
            sxy += e.eval * y;
        }
        return (n * sxy - sx * sy) / std::sqrt((n * sxx - sx * sx) * (n * syy - sy * sy));
    }

    // True when the stored cp looks like it is from the side to move's point of view.
    bool detect_side_to_move_view(const std::vector<Calib> &c) {
        const double white = correlation(c, false);
        const double stm = correlation(c, true);
        std::cout << "label correlation with engine score: White view " << white
                << ", side-to-move view " << stm << '\n';
        return stm > white;
    }

    void finalize_cp(Dataset &d, bool sideToMoveView, double k) {
        for (std::size_t i = 0; i < d.size(); ++i) {
            const double white = (sideToMoveView && !d.whiteToMove[i]) ? -d.raw[i] : d.raw[i];
            d.result[i] = static_cast<float>(predict(white, k));
        }
        d.raw.clear();
        d.whiteToMove.clear();
    }


    template<typename F>
    void parallel_for(std::size_t n, unsigned threads, F &&body) {
        std::vector<std::thread> pool;
        const std::size_t chunk = (n + threads - 1) / threads;
        for (unsigned t = 0; t < threads; ++t) {
            const std::size_t b = t * chunk;
            const std::size_t e = std::min(n, b + chunk);
            if (b >= e) break;
            pool.emplace_back([&body, b, e, t] { body(b, e, t); });
        }
        for (std::thread &th: pool) th.join();
    }

    double mean_loss(const Dataset &d, const std::vector<double> &w, double k, unsigned threads) {
        std::vector<double> part(threads, 0.0);
        parallel_for(d.size(), threads, [&](std::size_t b, std::size_t e, unsigned t) {
            double sum = 0.0;
            for (std::size_t i = b; i < e; ++i) {
                const double err = d.result[i] - predict(score_of(d, i, w), k);
                sum += err * err;
            }
            part[t] = sum;
        });
        double total = 0.0;
        for (const double p: part) total += p;
        return total / static_cast<double>(d.size());
    }

    double loss_and_gradient(const Dataset &d, const std::vector<double> &w, double k,
                             unsigned threads, std::vector<double> &grad) {
        std::vector<std::vector<double> > g(threads, std::vector<double>(kW, 0.0));
        std::vector<double> part(threads, 0.0);
        parallel_for(d.size(), threads, [&](std::size_t b, std::size_t e, unsigned t) {
            double sum = 0.0;
            std::vector<double> &gt = g[t];
            for (std::size_t i = b; i < e; ++i) {
                const double p = predict(score_of(d, i, w), k);
                const double err = p - d.result[i];
                sum += err * err;
                const double common = 2.0 * err * p * (1.0 - p) * k;
                const double ph = d.phase[i];
                const double fmg = common * ph / kMaxPhase;
                const double feg = common * (kMaxPhase - ph) / kMaxPhase;
                for (std::uint64_t en = d.start[i]; en < d.start[i + 1]; ++en) {
                    const double c = d.coef[en];
                    gt[2 * d.index[en]] += c * fmg;
                    gt[2 * d.index[en] + 1] += c * feg;
                }
            }
            part[t] = sum;
        });

        const double n = static_cast<double>(d.size());
        grad.assign(kW, 0.0);
        double total = 0.0;
        for (unsigned t = 0; t < threads; ++t) {
            total += part[t];
            for (std::size_t j = 0; j < kW; ++j) grad[j] += g[t][j];
        }
        for (double &x: grad) x /= n;
        return total / n;
    }

    // Golden-section search for the K that minimises the loss with the given weights.
    double fit_k(const Dataset &d, const std::vector<double> &w, unsigned threads) {
        double lo = 0.0005;
        double hi = 0.02;
        const double phi = 0.6180339887498949;
        double a = hi - phi * (hi - lo);
        double b = lo + phi * (hi - lo);
        double fa = mean_loss(d, w, a, threads);
        double fb = mean_loss(d, w, b, threads);
        for (int it = 0; it < 24; ++it) {
            if (fa < fb) {
                hi = b;
                b = a;
                fb = fa;
                a = hi - phi * (hi - lo);
                fa = mean_loss(d, w, a, threads);
            } else {
                lo = a;
                a = b;
                fa = fb;
                b = lo + phi * (hi - lo);
                fb = mean_loss(d, w, b, threads);
            }
        }
        return 0.5 * (lo + hi);
    }

    int iv(const std::vector<double> &w, std::size_t param, int phase) {
        return static_cast<int>(std::lround(w[2 * param + static_cast<std::size_t>(phase)]));
    }

    void print_list(std::ostream &o, const std::vector<double> &w, std::size_t first, std::size_t count,
                    int phase) {
        o << "{";
        for (std::size_t i = 0; i < count; ++i) {
            o << (i ? ", " : "") << iv(w, first + i, phase);
        }
        o << "}";
    }

    void print_pst(std::ostream &o, const std::vector<double> &w, int phase) {
        static const char *names[] = {"Pawn", "Knight", "Bishop", "Rook", "Queen", "King"};
        for (std::size_t t = 0; t < kNumPieceTypes; ++t) {
            o << "// " << names[t] << "\n{\n";
            for (std::size_t r = 0; r < 8; ++r) {
                o << "    ";
                for (std::size_t f = 0; f < 8; ++f) {
                    o << iv(w, EvalParams::kIdxPst + t * kNumSquares + r * 8 + f, phase);
                    o << ((r == 7 && f == 7) ? "" : ", ");
                }
                o << '\n';
            }
            o << "},\n";
        }
    }

    void write_result(const std::string &path, const std::vector<double> &w, double k,
                      double initialVal, double bestVal) {
        std::ofstream o(path);
        using namespace EvalParams;
        o << "// K = " << k << ", validation loss " << initialVal << " -> " << bestVal << "\n\n";

        o << "// eval_tables.hpp: kMgValue / kEgValue (P N B R Q K)\n";
        o << "kMgValue = ";
        print_list(o, w, kIdxMaterial, kMaterial, 0);
        o << " + {0}\nkEgValue = ";
        print_list(o, w, kIdxMaterial, kMaterial, 1);
        o << " + {0}\n\n// eval_tables.hpp: kMgPst\n";
        print_pst(o, w, 0);
        o << "\n// eval_tables.hpp: kEgPst\n";
        print_pst(o, w, 1);

        o << "\n// eval.cpp: pawn structure\n";
        o << "kDoubledMg = " << iv(w, kIdxDoubled, 0) << ", kDoubledEg = " << iv(w, kIdxDoubled, 1) << "\n";
        o << "kIsolatedMg = " << iv(w, kIdxIsolated, 0) << ", kIsolatedEg = " << iv(w, kIdxIsolated, 1) << "\n";
        o << "kPassedMg = ";
        print_list(o, w, kIdxPassed, 8, 0);
        o << "\nkPassedEg = ";
        print_list(o, w, kIdxPassed, 8, 1);

        o << "\n\n// eval.cpp: kMobility {base, mg, eg} (bases unchanged: 4, 6, 6, 13)\n";
        const char *names[] = {"Knight", "Bishop", "Rook", "Queen"};
        for (std::size_t i = 0; i < 4; ++i) {
            o << names[i] << ": mg " << iv(w, kIdxMobility + i, 0) << ", eg " << iv(w, kIdxMobility + i, 1) << "\n";
        }

        o << "\n// eval_tables.hpp: king shield\n";
        o << "kShieldNear = " << iv(w, kIdxShieldNear, 0) << ", kShieldFar = " << iv(w, kIdxShieldFar, 0) << "\n";
    }
} // namespace

int main(int argc, char **argv) {
    init_attacks();

    std::string label = "result";
    double fixedK = 0.0;
    int quietMargin = 10;
    std::vector<std::string> trainFiles;
    std::vector<std::string> valFiles;
    std::string out = "tuned_params.txt";
    int epochs = 300;
    double lr = 1.0;
    std::size_t maxPositions = 0;
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
        if (a == "--train") trainFiles.push_back(next());
        else if (a == "--val") valFiles.push_back(next());
        else if (a == "--out") out = next();
        else if (a == "--epochs") epochs = std::stoi(next());
        else if (a == "--lr") lr = std::stod(next());
        else if (a == "--max-positions") maxPositions = std::stoull(next());
        else if (a == "--threads") threads = static_cast<unsigned>(std::stoi(next()));
        else if (a == "--label") label = next();
        else if (a == "--k") fixedK = std::stod(next());
        else if (a == "--quiet-margin") quietMargin = std::stoi(next());
        else {
            std::cerr << "unknown argument " << a << '\n';
            return 1;
        }
    }
    if (label != "result" && label != "cp") {
        std::cerr << "--label must be result or cp\n";
        return 1;
    }

    if (trainFiles.empty() || valFiles.empty()) {
        std::cerr << "usage: knightlift_tune --train <file>... --val <file>... [--out f] "
                "[--epochs n] [--lr x] [--max-positions n] [--threads n]\n";
        return 1;
    }

    const ParamArray params0 = current_params();
    std::vector<double> w(kW);
    for (std::size_t i = 0; i < kP; ++i) {
        w[2 * i] = params0[i].mg;
        w[2 * i + 1] = params0[i].eg;
    }
    const std::vector<double> w0 = w;

    Dataset train;
    Dataset val;
    std::size_t mismatches = 0;
    LoadOptions opt;
    opt.cpLabels = (label == "cp");
    opt.quietMargin = quietMargin;
    std::vector<Calib> calib;
    for (const std::string &f: trainFiles) {
        if (!load(f, train, maxPositions, opt, w0, params0, mismatches, &calib)) return 1;
    }
    for (const std::string &f: valFiles) {
        if (!load(f, val, maxPositions / 10, opt, w0, params0, mismatches, nullptr)) return 1;
    }
    std::cout << "train positions " << train.size() << ", val positions " << val.size()
            << ", threads " << threads << '\n';
    if (mismatches != 0) {
        std::cerr << "self-check FAILED: " << mismatches << " stored scores differ from eval_from_params\n";
        return 1;
    }
    if (train.size() == 0 || val.size() == 0) return 1;

    std::vector<bool> frozen(kW, false);
    frozen[2 * EvalParams::kIdxShieldNear + 1] = true;
    frozen[2 * EvalParams::kIdxShieldFar + 1] = true;

    double k = 0.0;
    if (opt.cpLabels) {
        k = fixedK > 0.0 ? fixedK : 0.00368;
        const bool stm = detect_side_to_move_view(calib);
        finalize_cp(train, stm, k);
        finalize_cp(val, stm, k);
    } else {
        k = fit_k(train, w, threads);
    }
    const double initialTrain = mean_loss(train, w, k, threads);
    const double initialVal = mean_loss(val, w, k, threads);
    std::cout << "K " << k << ", initial loss train " << initialTrain << ", val " << initialVal << '\n';

    std::vector<double> m(kW, 0.0);
    std::vector<double> v(kW, 0.0);
    std::vector<double> grad;
    std::vector<double> best = w;
    double bestVal = initialVal;
    const double b1 = 0.9;
    const double b2 = 0.999;
    const double eps = 1e-8;

    for (int ep = 1; ep <= epochs; ++ep) {
        const double trainLoss = loss_and_gradient(train, w, k, threads, grad);
        const double rate = lr * (0.1 + 0.9 * 0.5 * (1.0 + std::cos(std::numbers::pi * (ep - 1) / epochs)));
        for (std::size_t j = 0; j < kW; ++j) {
            if (frozen[j]) continue;
            m[j] = b1 * m[j] + (1.0 - b1) * grad[j];
            v[j] = b2 * v[j] + (1.0 - b2) * grad[j] * grad[j];
            const double mh = m[j] / (1.0 - std::pow(b1, ep));
            const double vh = v[j] / (1.0 - std::pow(b2, ep));
            w[j] -= rate * mh / (std::sqrt(vh) + eps);
        }
        if (ep % 10 == 0 || ep == epochs) {
            const double valLoss = mean_loss(val, w, k, threads);
            std::cout << "epoch " << ep << " train " << trainLoss << " val " << valLoss << '\n';
            if (valLoss < bestVal) {
                bestVal = valLoss;
                best = w;
            }
        }
    }

    write_result(out, best, k, initialVal, bestVal);
    std::cout << "best val loss " << bestVal << " (initial " << initialVal << "), written to " << out << '\n';
    return 0;
}

#include "nnue.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <memory>

namespace KnightLift::Nnue {

    static_assert(std::endian::native == std::endian::little, "network files are little endian");

    namespace {
        struct Network {
            alignas(32) std::int16_t w1[kInputs * kHidden];
            alignas(32) std::int16_t b1[kHidden];
            alignas(32) std::int16_t w2[2 * kHidden];
            std::int32_t b2;
        };

        Network gNet{};
        bool gLoaded = false;
        bool gUse = false;

        void update_enabled() { detail::gEnabled = gUse && gLoaded; }
    } // namespace

    bool load(const std::string &path, std::string &error) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            error = "cannot open " + path;
            return false;
        }

        char magic[8];
        std::int32_t header[5];
        in.read(magic, sizeof(magic));
        in.read(reinterpret_cast<char *>(header), sizeof(header));
        if (!in || std::memcmp(magic, "KNNUE001", 8) != 0) {
            error = "not a KNNUE001 file";
            return false;
        }
        if (header[0] != kInputs || header[1] != kHidden || header[2] != kQA ||
            header[3] != kQB || header[4] != kScale) {
            error = "network dimensions or scales do not match this build";
            return false;
        }

        auto net = std::make_unique<Network>();
        in.read(reinterpret_cast<char *>(net->w1), sizeof(net->w1));
        in.read(reinterpret_cast<char *>(net->b1), sizeof(net->b1));
        in.read(reinterpret_cast<char *>(net->w2), sizeof(net->w2));
        in.read(reinterpret_cast<char *>(&net->b2), sizeof(net->b2));
        if (!in) {
            error = "file is truncated";
            return false;
        }
        if (in.peek() != std::char_traits<char>::eof()) {
            error = "unexpected data after the network";
            return false;
        }

        gNet = *net;
        gLoaded = true;
        update_enabled();
        return true;
    }

    void set_use(bool use) {
        gUse = use;
        update_enabled();
    }

    bool loaded() { return gLoaded; }

    void reset(Accumulator &acc) {
        for (auto &side: acc.v) {
            for (int i = 0; i < kHidden; ++i) side[i] = enabled() ? gNet.b1[i] : std::int16_t{0};
        }
    }

    void add_feature(Accumulator &acc, Piece p, Square s) {
        for (int persp = 0; persp < 2; ++persp) {
            const std::int16_t *row =
                    &gNet.w1[feature_index(static_cast<Color>(persp), p, s) * kHidden];
            std::int16_t *a = acc.v[persp].data();
            for (int i = 0; i < kHidden; ++i) a[i] = static_cast<std::int16_t>(a[i] + row[i]);
        }
    }

    void sub_feature(Accumulator &acc, Piece p, Square s) {
        for (int persp = 0; persp < 2; ++persp) {
            const std::int16_t *row =
                    &gNet.w1[feature_index(static_cast<Color>(persp), p, s) * kHidden];
            std::int16_t *a = acc.v[persp].data();
            for (int i = 0; i < kHidden; ++i) a[i] = static_cast<std::int16_t>(a[i] - row[i]);
        }
    }

    int evaluate(const Accumulator &acc, Color sideToMove) {
        const auto &us = acc.v[to_index(sideToMove)];
        const auto &them = acc.v[to_index(~sideToMove)];

        std::int32_t sum = gNet.b2;
        for (int i = 0; i < kHidden; ++i) {
            sum += std::clamp<std::int32_t>(us[i], 0, kQA) * gNet.w2[i];
        }
        for (int i = 0; i < kHidden; ++i) {
            sum += std::clamp<std::int32_t>(them[i], 0, kQA) * gNet.w2[kHidden + i];
        }
        // Integer division truncates toward zero, as in the Python reference.
        return static_cast<int>(static_cast<std::int64_t>(sum) * kScale / (kQA * kQB));
    }

} // namespace KnightLift::Nnue
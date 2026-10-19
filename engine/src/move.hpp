#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "types.hpp"

namespace KnightLift {

enum class MoveFlag : std::uint8_t {
    Quiet = 0,
    DoublePush = 1,
    KingCastle = 2,
    QueenCastle = 3,
    Capture = 4,
    EnPassant = 5,
    PromoKnight = 8,
    PromoBishop = 9,
    PromoRook = 10,
    PromoQueen = 11,
    PromoCaptureKnight = 12,
    PromoCaptureBishop = 13,
    PromoCaptureRook = 14,
    PromoCaptureQueen = 15,
};

class Move {
public:
    constexpr Move() = default;

    constexpr Move(Square from, Square to, MoveFlag flag)
        : data_(static_cast<std::uint16_t>(
              to_index(from) | (to_index(to) << 6) | (to_index(flag) << 12))) {}

    constexpr Square from() const { return static_cast<Square>(data_ & 0x3F); }
    constexpr Square to() const { return static_cast<Square>((data_ >> 6) & 0x3F); }
    constexpr MoveFlag flag() const { return static_cast<MoveFlag>(data_ >> 12); }

    constexpr bool is_capture() const { return (data_ & kCaptureBit) != 0; }
    constexpr bool is_promotion() const { return (data_ & kPromotionBit) != 0; }

    // Precondition: is_promotion().
    constexpr PieceType promotion_type() const {
        return static_cast<PieceType>(to_index(PieceType::Knight) + ((data_ >> 12) & 3));
    }

    constexpr bool is_none() const { return data_ == 0; }
    constexpr std::uint16_t raw() const { return data_; }

    constexpr bool operator==(const Move&) const = default;

    static constexpr Move none() { return Move(); }

private:
    static constexpr std::uint16_t kCaptureBit = 0x4000;
    static constexpr std::uint16_t kPromotionBit = 0x8000;

    std::uint16_t data_ = 0;
};

static_assert(sizeof(Move) == 2);
static_assert(Move(Square::E7, Square::D8, MoveFlag::PromoCaptureQueen).from() == Square::E7);
static_assert(Move(Square::E7, Square::D8, MoveFlag::PromoCaptureQueen).to() == Square::D8);
static_assert(Move(Square::E7, Square::D8, MoveFlag::PromoCaptureQueen).is_capture());
static_assert(Move(Square::E7, Square::D8, MoveFlag::PromoCaptureQueen).is_promotion());
static_assert(Move(Square::E7, Square::D8, MoveFlag::PromoCaptureQueen).promotion_type() ==
              PieceType::Queen);
static_assert(Move(Square::E7, Square::E8, MoveFlag::PromoKnight).promotion_type() ==
              PieceType::Knight);
static_assert(!Move(Square::E2, Square::E4, MoveFlag::DoublePush).is_capture());
static_assert(Move(Square::E5, Square::D6, MoveFlag::EnPassant).is_capture());
static_assert(!Move(Square::E1, Square::G1, MoveFlag::KingCastle).is_capture());
static_assert(!Move(Square::E1, Square::G1, MoveFlag::KingCastle).is_promotion());
static_assert(Move::none().is_none());

inline std::string move_to_uci(Move m) {
    if (m.is_none()) return "0000";

    constexpr std::string_view kPromoChars = "nbrq";

    std::string out;
    out += static_cast<char>('a' + to_index(file_of(m.from())));
    out += static_cast<char>('1' + to_index(rank_of(m.from())));
    out += static_cast<char>('a' + to_index(file_of(m.to())));
    out += static_cast<char>('1' + to_index(rank_of(m.to())));
    if (m.is_promotion()) {
        out += kPromoChars[to_index(m.promotion_type()) - to_index(PieceType::Knight)];
    }
    return out;
}

constexpr std::size_t kMaxMoves = 256;

class MoveList {
public:
    void push(Move m) {
        assert(size_ < kMaxMoves);
        moves_[size_++] = m;
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    Move operator[](std::size_t i) const { return moves_[i]; }

    const Move* begin() const { return moves_.data(); }
    const Move* end() const { return moves_.data() + size_; }

private:
    std::array<Move, kMaxMoves> moves_;
    std::size_t size_ = 0;
};

}  // namespace KnightLift
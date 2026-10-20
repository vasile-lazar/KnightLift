#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace KnightLift {
    using Bitboard = std::uint64_t;

    template<typename E>
        requires std::is_enum_v<E>
    constexpr std::size_t to_index(E e) {
        return static_cast<std::size_t>(e);
    }

    // ---- Color ----------------------------------------------------------------

    enum class Color : std::uint8_t { White, Black };

    constexpr std::size_t kNumColors = 2;

    constexpr Color operator~(Color c) {
        return static_cast<Color>(to_index(c) ^ 1);
    }

    // ---- Pieces ---------------------------------------------------------------

    enum class PieceType : std::uint8_t { Pawn, Knight, Bishop, Rook, Queen, King };

    constexpr std::size_t kNumPieceTypes = 6;

    enum class Piece : std::uint8_t {
        WhitePawn, WhiteKnight, WhiteBishop, WhiteRook, WhiteQueen, WhiteKing,
        BlackPawn, BlackKnight, BlackBishop, BlackRook, BlackQueen, BlackKing,
        None
    };

    constexpr std::size_t kNumPieces = 12;

    constexpr Piece make_piece(Color c, PieceType pt) {
        return static_cast<Piece>(to_index(c) * kNumPieceTypes + to_index(pt));
    }

    constexpr Color color_of(Piece p) {
        return static_cast<Color>(to_index(p) / kNumPieceTypes);
    }

    constexpr PieceType type_of(Piece p) {
        return static_cast<PieceType>(to_index(p) % kNumPieceTypes);
    }

    // ---- Squares --------------------------------------------------------------

    enum class File : std::uint8_t { A, B, C, D, E, F, G, H };

    enum class Rank : std::uint8_t { R1, R2, R3, R4, R5, R6, R7, R8 };

    enum class Square : std::uint8_t {
        A1, B1, C1, D1, E1, F1, G1, H1,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A8, B8, C8, D8, E8, F8, G8, H8,
        None
    };

    constexpr std::size_t kNumSquares = 64;

    constexpr Square make_square(File f, Rank r) {
        return static_cast<Square>(to_index(r) * 8 + to_index(f));
    }

    constexpr File file_of(Square s) {
        return static_cast<File>(to_index(s) & 7);
    }

    constexpr Rank rank_of(Square s) {
        return static_cast<Rank>(to_index(s) >> 3);
    }

    constexpr Bitboard square_bb(Square s) {
        return Bitboard{1} << to_index(s);
    }

    constexpr Square offset(Square s, int delta) {
        return static_cast<Square>(static_cast<int>(to_index(s)) + delta);
    }

    // ---- Compile-time sanity checks -------------------------------------------

    static_assert(to_index(Square::A1) == 0);
    static_assert(to_index(Square::H1) == 7);
    static_assert(to_index(Square::A2) == 8);
    static_assert(to_index(Square::H8) == 63);
    static_assert(make_square(File::E, Rank::R4) == Square::E4);
    static_assert(file_of(Square::G6) == File::G && rank_of(Square::G6) == Rank::R6);
    static_assert(square_bb(Square::H8) == 0x8000000000000000ULL);
    static_assert(~Color::White == Color::Black);
    static_assert(make_piece(Color::Black, PieceType::Queen) == Piece::BlackQueen);
    static_assert(color_of(Piece::BlackQueen) == Color::Black);
    static_assert(type_of(Piece::BlackQueen) == PieceType::Queen);
} // namespace KnightLift

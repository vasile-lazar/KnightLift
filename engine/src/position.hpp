#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "bitboard.hpp"
#include "types.hpp"

namespace KnightLift {

    using CastlingRights = std::uint8_t;

    constexpr CastlingRights kWhiteKingside = 1;
    constexpr CastlingRights kWhiteQueenside = 2;
    constexpr CastlingRights kBlackKingside = 4;
    constexpr CastlingRights kBlackQueenside = 8;

    constexpr std::string_view kStartFen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    class Position {
    public:
        Position() { clear(); }

        bool set_from_fen(std::string_view fen);
        std::string to_fen() const;
        std::string to_string() const;

        Color side_to_move() const { return sideToMove_; }
        CastlingRights castling_rights() const { return castling_; }
        Square ep_square() const { return epSquare_; }
        int halfmove_clock() const { return halfmoveClock_; }
        int fullmove_number() const { return fullmoveNumber_; }

        Bitboard occupied() const { return occupied_; }
        Bitboard pieces(Color c) const { return colorBB_[to_index(c)]; }
        Bitboard pieces(Piece p) const { return pieceBB_[to_index(p)]; }
        Bitboard pieces(Color c, PieceType pt) const {
            return pieceBB_[to_index(make_piece(c, pt))];
        }
        Bitboard pieces(PieceType pt) const {
            return pieceBB_[to_index(make_piece(Color::White, pt))] |
                   pieceBB_[to_index(make_piece(Color::Black, pt))];
        }
        Square king_square(Color c) const { return lsb(pieces(c, PieceType::King)); }

        Bitboard attackers_to(Square s, Bitboard occupied) const;  // pieces of both colors
        bool is_attacked(Square s, Color by) const;
        bool in_check() const { return is_attacked(king_square(sideToMove_), ~sideToMove_); }
        Piece piece_on(Square s) const { return board_[to_index(s)]; }

    private:
        void clear();
        void put_piece(Piece p, Square s);

        std::array<Bitboard, kNumPieces> pieceBB_;
        std::array<Bitboard, kNumColors> colorBB_;
        Bitboard occupied_;
        std::array<Piece, kNumSquares> board_;

        Color sideToMove_;
        CastlingRights castling_;
        Square epSquare_;
        int halfmoveClock_;
        int fullmoveNumber_;
    };

}  // namespace KnightLift
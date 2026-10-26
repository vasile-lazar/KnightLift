#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstddef>

#include "bitboard.hpp"
#include "types.hpp"
#include "move.hpp"
#include "zobrist.hpp"
#include "nnue.hpp"

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
        bool is_repetition(int ply) const;   // ply = plies since the search root
        bool insufficient_material() const;  // K vs K, K+minor vs K
        Piece piece_on(Square s) const { return board_[to_index(s)]; }
        void make_move(Move m);
        void make_null_move();
        void unmake_null_move();
        void unmake_move(Move m);
        bool consistent() const;  // debug: bitboards agree with the mailbox
        Key key() const { return key_; }
        int psq_mg() const { return psqMg_; }  // White minus Black material + PST, middlegame
        int psq_eg() const { return psqEg_; }
        int phase() const { return phase_; }   // unclamped sum of piece phase weights
        Key compute_key() const; // from scratch, used by set_from_fen and consistent()

        // After make_move: true if the side that just moved left its own king attacked.
        bool king_left_in_check() const {
            return is_attacked(king_square(~sideToMove_), sideToMove_);
        }
        const Nnue::Accumulator &accumulator() const { return acc_; }
        void refresh_accumulators(); // rebuild from the board (after UseNNUE or EvalFile changes)
        // private:
        Nnue::Accumulator acc_;
    private:
        void clear();
        void put_piece(Piece p, Square s);
        void remove_piece(Square s);
        void move_piece(Square from, Square to);

        struct State {
            Piece captured = Piece::None;
            CastlingRights castling = 0;
            Square epSquare = Square::None;
            int halfmoveClock = 0;
            Key key = 0;
            int psqMg = 0;
            int psqEg = 0;
            int phase = 0;
        };
        Key ep_key() const; // 0 if no ep square or no pawn can capture on it

        static constexpr std::size_t kMaxGamePly = 2048;
        std::array<State, kMaxGamePly> history_;
        std::size_t historyPly_ = 0;
        std::array<Bitboard, kNumPieces> pieceBB_;
        std::array<Bitboard, kNumColors> colorBB_;
        Bitboard occupied_;
        std::array<Piece, kNumSquares> board_;

        Color sideToMove_;
        Key key_ = 0;
        int psqMg_ = 0;
        int psqEg_ = 0;
        int phase_ = 0;
        CastlingRights castling_;
        Square epSquare_;
        int halfmoveClock_;
        int fullmoveNumber_;
    };

}  // namespace KnightLift
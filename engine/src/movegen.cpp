#include "movegen.hpp"

#include "attacks.hpp"

namespace KnightLift {

namespace {

Square offset(Square s, int delta) {
    return static_cast<Square>(static_cast<int>(to_index(s)) + delta);
}

void add_targets(Square from, Bitboard targets, Bitboard enemies, MoveList& list) {
    while (targets) {
        const Square to = pop_lsb(targets);
        list.push(Move(from, to,
                       (enemies & square_bb(to)) ? MoveFlag::Capture : MoveFlag::Quiet));
    }
}

void add_promotions(Square from, Square to, bool capture, MoveList& list) {
    list.push(Move(from, to, capture ? MoveFlag::PromoCaptureQueen : MoveFlag::PromoQueen));
    list.push(Move(from, to, capture ? MoveFlag::PromoCaptureRook : MoveFlag::PromoRook));
    list.push(Move(from, to, capture ? MoveFlag::PromoCaptureBishop : MoveFlag::PromoBishop));
    list.push(Move(from, to, capture ? MoveFlag::PromoCaptureKnight : MoveFlag::PromoKnight));
}

void generate_pawn_moves(const Position& pos, Color us, MoveList& list) {
    const Bitboard empty = ~pos.occupied();
    const Bitboard enemies = pos.pieces(~us);
    const int up = (us == Color::White) ? 8 : -8;
    const Rank startRank = (us == Color::White) ? Rank::R2 : Rank::R7;
    const Rank promoRank = (us == Color::White) ? Rank::R8 : Rank::R1;

    Bitboard pawns = pos.pieces(us, PieceType::Pawn);
    while (pawns) {
        const Square from = pop_lsb(pawns);

        // Pushes.
        const Square one = offset(from, up);
        if (empty & square_bb(one)) {
            if (rank_of(one) == promoRank) {
                add_promotions(from, one, false, list);
            } else {
                list.push(Move(from, one, MoveFlag::Quiet));
                const Square two = offset(one, up);
                if (rank_of(from) == startRank && (empty & square_bb(two))) {
                    list.push(Move(from, two, MoveFlag::DoublePush));
                }
            }
        }

        // Captures and en passant.
        const Bitboard attacks = pawn_attacks(us, from);
        Bitboard captures = attacks & enemies;
        while (captures) {
            const Square to = pop_lsb(captures);
            if (rank_of(to) == promoRank) {
                add_promotions(from, to, true, list);
            } else {
                list.push(Move(from, to, MoveFlag::Capture));
            }
        }
        if (pos.ep_square() != Square::None && (attacks & square_bb(pos.ep_square()))) {
            list.push(Move(from, pos.ep_square(), MoveFlag::EnPassant));
        }
    }
}

void generate_piece_moves(const Position& pos, Color us, MoveList& list) {
    const Bitboard occupied = pos.occupied();
    const Bitboard allowed = ~pos.pieces(us);
    const Bitboard enemies = pos.pieces(~us);

    for (const PieceType pt : {PieceType::Knight, PieceType::Bishop, PieceType::Rook,
                               PieceType::Queen, PieceType::King}) {
        Bitboard pieces = pos.pieces(us, pt);
        while (pieces) {
            const Square from = pop_lsb(pieces);
            add_targets(from, piece_attacks(pt, from, occupied) & allowed, enemies, list);
        }
    }
}

void generate_castling(const Position& pos, Color us, MoveList& list) {
    const Bitboard occupied = pos.occupied();
    const Color them = ~us;

    auto tryCastle = [&](CastlingRights right, Bitboard mustBeEmpty, Square kingFrom,
                         Square via, Square kingTo, MoveFlag flag) {
        if ((pos.castling_rights() & right) && !(occupied & mustBeEmpty) &&
            !pos.is_attacked(kingFrom, them) && !pos.is_attacked(via, them) &&
            !pos.is_attacked(kingTo, them)) {
            list.push(Move(kingFrom, kingTo, flag));
        }
    };

    if (us == Color::White) {
        tryCastle(kWhiteKingside, square_bb(Square::F1) | square_bb(Square::G1),
                  Square::E1, Square::F1, Square::G1, MoveFlag::KingCastle);
        tryCastle(kWhiteQueenside,
                  square_bb(Square::B1) | square_bb(Square::C1) | square_bb(Square::D1),
                  Square::E1, Square::D1, Square::C1, MoveFlag::QueenCastle);
    } else {
        tryCastle(kBlackKingside, square_bb(Square::F8) | square_bb(Square::G8),
                  Square::E8, Square::F8, Square::G8, MoveFlag::KingCastle);
        tryCastle(kBlackQueenside,
                  square_bb(Square::B8) | square_bb(Square::C8) | square_bb(Square::D8),
                  Square::E8, Square::D8, Square::C8, MoveFlag::QueenCastle);
    }
}

}  // namespace

void generate_pseudo_legal(const Position& pos, MoveList& list) {
    const Color us = pos.side_to_move();
    generate_pawn_moves(pos, us, list);
    generate_piece_moves(pos, us, list);
    generate_castling(pos, us, list);
}

}  // namespace KnightLift
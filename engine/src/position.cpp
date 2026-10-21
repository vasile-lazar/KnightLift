#include <charconv>
#include <sstream>
#include <system_error>
#include <cassert>

#include "position.hpp"
#include "attacks.hpp"

namespace KnightLift {
    namespace {
        // Same order as the Piece enum: white pawn...king, then black pawn...king.
        constexpr std::string_view kPieceChars = "PNBRQKpnbrqk";

        constexpr CastlingRights kAllCastling =
                kWhiteKingside | kWhiteQueenside | kBlackKingside | kBlackQueenside;

        constexpr Square square_at(int file, int rank) {
            return make_square(static_cast<File>(file), static_cast<Rank>(rank));
        }

        bool parse_int(const std::string &text, int &out) {
            const char *first = text.data();
            const char *last = first + text.size();
            const auto [ptr, ec] = std::from_chars(first, last, out);
            return ec == std::errc{} && ptr == last;
        }

        constexpr std::array<CastlingRights, kNumSquares> make_castling_mask() {
            std::array<CastlingRights, kNumSquares> mask{};
            for (CastlingRights &m: mask) m = kAllCastling;
            mask[to_index(Square::A1)] = kAllCastling & ~kWhiteQueenside;
            mask[to_index(Square::H1)] = kAllCastling & ~kWhiteKingside;
            mask[to_index(Square::E1)] = kAllCastling & ~(kWhiteKingside | kWhiteQueenside);
            mask[to_index(Square::A8)] = kAllCastling & ~kBlackQueenside;
            mask[to_index(Square::H8)] = kAllCastling & ~kBlackKingside;
            mask[to_index(Square::E8)] = kAllCastling & ~(kBlackKingside | kBlackQueenside);
            return mask;
        }

        constexpr std::array<CastlingRights, kNumSquares> kCastlingMask = make_castling_mask();
    } // namespace

    void Position::clear() {
        pieceBB_.fill(0);
        colorBB_.fill(0);
        occupied_ = 0;
        board_.fill(Piece::None);
        sideToMove_ = Color::White;
        castling_ = 0;
        epSquare_ = Square::None;
        halfmoveClock_ = 0;
        fullmoveNumber_ = 1;
        historyPly_ = 0;
        key_ = 0;
    }

    void Position::put_piece(Piece p, Square s) {
        const Bitboard bb = square_bb(s);
        pieceBB_[to_index(p)] |= bb;
        colorBB_[to_index(color_of(p))] |= bb;
        occupied_ |= bb;
        key_ ^= Zobrist::kKeys.pieceSquare[to_index(p)][to_index(s)];
        board_[to_index(s)] = p;
    }

    bool Position::set_from_fen(std::string_view fen) {
        clear();

        auto fail = [this] {
            clear();
            return false;
        };

        std::istringstream in{std::string{fen}};
        std::string placement, side, castling, ep, halfStr, fullStr;
        if (!(in >> placement >> side >> castling >> ep)) {
            return fail();
        }
        in >> halfStr >> fullStr; // optional; stay empty if missing

        // 1. Piece placement, rank 8 first.
        int rank = 7;
        int file = 0;
        for (const char ch: placement) {
            if (ch == '/') {
                if (file != 8 || rank == 0) return fail();
                --rank;
                file = 0;
            } else if (ch >= '1' && ch <= '8') {
                file += ch - '0';
                if (file > 8) return fail();
            } else {
                const std::size_t idx = kPieceChars.find(ch);
                if (idx == std::string_view::npos || file >= 8) return fail();
                put_piece(static_cast<Piece>(idx), square_at(file, rank));
                ++file;
            }
        }
        if (rank != 0 || file != 8) return fail();

        // 2. Side to move.
        if (side == "w") {
            sideToMove_ = Color::White;
        } else if (side == "b") {
            sideToMove_ = Color::Black;
        } else {
            return fail();
        }

        // 3. Castling rights.
        if (castling != "-") {
            for (const char ch: castling) {
                switch (ch) {
                    case 'K': castling_ |= kWhiteKingside;
                        break;
                    case 'Q': castling_ |= kWhiteQueenside;
                        break;
                    case 'k': castling_ |= kBlackKingside;
                        break;
                    case 'q': castling_ |= kBlackQueenside;
                        break;
                    default: return fail();
                }
            }
        }

        // 4. En passant target square.
        if (ep != "-") {
            if (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h' ||
                (ep[1] != '3' && ep[1] != '6')) {
                return fail();
            }
            epSquare_ = square_at(ep[0] - 'a', ep[1] - '1');
        }

        // 5. Halfmove clock and fullmove number (optional).
        if (!halfStr.empty() && (!parse_int(halfStr, halfmoveClock_) || halfmoveClock_ < 0)) {
            return fail();
        }
        if (!fullStr.empty() && (!parse_int(fullStr, fullmoveNumber_) || fullmoveNumber_ < 1)) {
            return fail();
        }

        // 6. Exactly one king per side.
        if (popcount(pieces(Color::White, PieceType::King)) != 1 ||
            popcount(pieces(Color::Black, PieceType::King)) != 1) {
            return fail();
        }

        // 7. No pawns on the first or last rank.
        if (pieces(PieceType::Pawn) & (rank_bb(Rank::R1) | rank_bb(Rank::R8))) {
            return fail();
        }
        key_ = compute_key();

        return true;
    }

    std::string Position::to_fen() const {
        std::string out;

        for (int r = 7; r >= 0; --r) {
            int empty = 0;
            for (int f = 0; f < 8; ++f) {
                const Piece p = piece_on(square_at(f, r));
                if (p == Piece::None) {
                    ++empty;
                } else {
                    if (empty > 0) {
                        out += static_cast<char>('0' + empty);
                        empty = 0;
                    }
                    out += kPieceChars[to_index(p)];
                }
            }
            if (empty > 0) out += static_cast<char>('0' + empty);
            if (r > 0) out += '/';
        }

        out += (sideToMove_ == Color::White) ? " w " : " b ";

        if (castling_ == 0) {
            out += '-';
        } else {
            if (castling_ & kWhiteKingside) out += 'K';
            if (castling_ & kWhiteQueenside) out += 'Q';
            if (castling_ & kBlackKingside) out += 'k';
            if (castling_ & kBlackQueenside) out += 'q';
        }

        out += ' ';
        if (epSquare_ == Square::None) {
            out += '-';
        } else {
            out += static_cast<char>('a' + to_index(file_of(epSquare_)));
            out += static_cast<char>('1' + to_index(rank_of(epSquare_)));
        }

        out += ' ';
        out += std::to_string(halfmoveClock_);
        out += ' ';
        out += std::to_string(fullmoveNumber_);
        return out;
    }

    std::string Position::to_string() const {
        std::string out;
        for (int r = 7; r >= 0; --r) {
            out += static_cast<char>('1' + r);
            out += ' ';
            for (int f = 0; f < 8; ++f) {
                const Piece p = piece_on(square_at(f, r));
                out += (p == Piece::None) ? '.' : kPieceChars[to_index(p)];
                out += ' ';
            }
            out += '\n';
        }
        out += "  a b c d e f g h\n";
        out += "FEN: " + to_fen() + '\n';
        return out;
    }

    Bitboard Position::attackers_to(Square s, Bitboard occupied) const {
        const Bitboard bishopsQueens = pieces(PieceType::Bishop) | pieces(PieceType::Queen);
        const Bitboard rooksQueens = pieces(PieceType::Rook) | pieces(PieceType::Queen);

        return (pawn_attacks(Color::Black, s) & pieces(Color::White, PieceType::Pawn)) |
               (pawn_attacks(Color::White, s) & pieces(Color::Black, PieceType::Pawn)) |
               (knight_attacks(s) & pieces(PieceType::Knight)) |
               (king_attacks(s) & pieces(PieceType::King)) |
               (bishop_attacks(s, occupied) & bishopsQueens) |
               (rook_attacks(s, occupied) & rooksQueens);
    }

    bool Position::is_attacked(Square s, Color by) const {
        return (attackers_to(s, occupied_) & pieces(by)) != 0;
    }

    void Position::remove_piece(Square s) {
        const Piece p = board_[to_index(s)];
        const Bitboard bb = square_bb(s);
        pieceBB_[to_index(p)] ^= bb;
        colorBB_[to_index(color_of(p))] ^= bb;
        occupied_ ^= bb;
        key_ ^= Zobrist::kKeys.pieceSquare[to_index(board_[to_index(s)])][to_index(s)];
        board_[to_index(s)] = Piece::None;
    }

    // Precondition: `to` is empty.
    void Position::move_piece(Square from, Square to) {
        const Piece p = board_[to_index(from)];
        const Bitboard fromTo = square_bb(from) | square_bb(to);
        pieceBB_[to_index(p)] ^= fromTo;
        colorBB_[to_index(color_of(p))] ^= fromTo;
        occupied_ ^= fromTo;
        {
            const std::size_t pi = to_index(board_[to_index(from)]);
            key_ ^= Zobrist::kKeys.pieceSquare[pi][to_index(from)] ^
                    Zobrist::kKeys.pieceSquare[pi][to_index(to)];
        }
        board_[to_index(from)] = Piece::None;
        board_[to_index(to)] = p;
    }

    void Position::make_move(Move m) {
        assert(historyPly_ < kMaxGamePly);
        const Square from = m.from();
        const Square to = m.to();
        const Color us = sideToMove_;
        const Piece moving = board_[to_index(from)];

        State &st = history_[historyPly_++];
        st.captured = Piece::None;
        st.castling = castling_;
        st.epSquare = epSquare_;
        st.halfmoveClock = halfmoveClock_;
        st.key = key_;

        key_ ^= ep_key();
        key_ ^= Zobrist::kKeys.castling[castling_];

        ++halfmoveClock_;
        epSquare_ = Square::None;

        if (m.flag() == MoveFlag::EnPassant) {
            const Square victim = offset(to, us == Color::White ? -8 : 8);
            st.captured = board_[to_index(victim)];
            remove_piece(victim);
            halfmoveClock_ = 0;
        } else if (m.is_capture()) {
            st.captured = board_[to_index(to)];
            remove_piece(to);
            halfmoveClock_ = 0;
        }

        move_piece(from, to);

        if (type_of(moving) == PieceType::Pawn) {
            halfmoveClock_ = 0;
            if (m.flag() == MoveFlag::DoublePush) {
                epSquare_ = offset(from, us == Color::White ? 8 : -8);
            } else if (m.is_promotion()) {
                remove_piece(to);
                put_piece(make_piece(us, m.promotion_type()), to);
            }
        }

        if (m.flag() == MoveFlag::KingCastle) {
            move_piece(offset(to, 1), offset(to, -1));
        } else if (m.flag() == MoveFlag::QueenCastle) {
            move_piece(offset(to, -2), offset(to, 1));
        }

        castling_ &= kCastlingMask[to_index(from)] & kCastlingMask[to_index(to)];
        key_ ^= Zobrist::kKeys.castling[castling_];
        if (us == Color::Black) ++fullmoveNumber_;
        sideToMove_ = ~us;
        key_ ^= Zobrist::kKeys.side;
        key_ ^= ep_key();
    }

    void Position::unmake_move(Move m) {
        assert(historyPly_ > 0);
        sideToMove_ = ~sideToMove_;
        const Color us = sideToMove_;
        const Square from = m.from();
        const Square to = m.to();
        const State &st = history_[--historyPly_];

        if (us == Color::Black) --fullmoveNumber_;

        if (m.is_promotion()) {
            remove_piece(to);
            put_piece(make_piece(us, PieceType::Pawn), to);
        }
        move_piece(to, from);

        if (m.flag() == MoveFlag::EnPassant) {
            put_piece(st.captured, offset(to, us == Color::White ? -8 : 8));
        } else if (m.is_capture()) {
            put_piece(st.captured, to);
        } else if (m.flag() == MoveFlag::KingCastle) {
            move_piece(offset(to, -1), offset(to, 1));
        } else if (m.flag() == MoveFlag::QueenCastle) {
            move_piece(offset(to, 1), offset(to, -2));
        }

        castling_ = st.castling;
        epSquare_ = st.epSquare;
        halfmoveClock_ = st.halfmoveClock;
        key_ = st.key;
    }

    bool Position::consistent() const {
        std::array<Bitboard, kNumPieces> bb{};
        if (key_ != compute_key()) return false;

        for (std::size_t i = 0; i < kNumSquares; ++i) {
            const Piece p = board_[i];
            if (p != Piece::None) bb[to_index(p)] |= square_bb(static_cast<Square>(i));
        }
        Bitboard white = 0;
        Bitboard black = 0;
        for (std::size_t t = 0; t < kNumPieceTypes; ++t) {
            white |= bb[t];
            black |= bb[t + kNumPieceTypes];
        }
        return bb == pieceBB_ && white == colorBB_[0] && black == colorBB_[1] &&
               (white | black) == occupied_;
    }

    Key Position::ep_key() const {
        if (epSquare_ == Square::None) return 0;
        if (!(pawn_attacks(~sideToMove_, epSquare_) & pieces(sideToMove_, PieceType::Pawn))) return 0;
        return Zobrist::kKeys.epFile[to_index(file_of(epSquare_))];
    }

    Key Position::compute_key() const {
        Key k = 0;
        for (std::size_t s = 0; s < kNumSquares; ++s) {
            const Piece p = board_[s];
            if (p != Piece::None) k ^= Zobrist::kKeys.pieceSquare[to_index(p)][s];
        }
        k ^= Zobrist::kKeys.castling[castling_];
        k ^= ep_key();
        if (sideToMove_ == Color::Black) k ^= Zobrist::kKeys.side;
        return k;
    }
} // namespace KnightLift

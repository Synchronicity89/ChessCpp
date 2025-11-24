#include <cstdint>
#include <vector>
#include <functional>
#include <algorithm>
#include <string>
#include <cctype>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include "EngineBase.h"
#include "ChessEngine2.hpp"

namespace engine {

    void ChessEngine2::flipPosition(){ EngineBase::flipPosition(); }
    std::string ChessEngine2::buildFen() const { return EngineBase::buildFen(*this); }

    std::string ChessEngine2::choose_move(const std::string& fen, int depth) {
        loadFEN(fen);
        return getBestMove(depth);
    }

    std::vector<std::pair<std::string, int>> ChessEngine2::root_search_scores(const std::string& fen, int depth) {
        loadFEN(fen);
        std::vector<Move> moves;
        generateLegalMoves(depth, moves);
        std::vector<std::pair<std::string, int>> out;
        for (auto& m : moves) {
            ChessEngine2 save = *this;
            makeMove(m);
            flipPosition();
            int score = -alphaBeta(depth - 1, -INF, INF, depth - 1);
            *this = save;
            out.emplace_back(moveToUci(m), score);
        }
        return out;
    }

    std::vector<std::string> ChessEngine2::legal_moves_uci(const std::string& fen) {
        loadFEN(fen);
        std::vector<Move> moves;
        generateLegalMoves(1, moves);
        std::vector<std::string> r;
        for (auto& m : moves) r.push_back(moveToUci(m));
        return r;
    }

    std::string ChessEngine2::apply_move(const std::string& fen, const std::string& uci) {
        loadFEN(fen);
        if (uci.size() < 4) return {};
        std::vector<Move> moves; generateLegalMoves(1, moves);
        Move chosen{}; bool found = false;
        for (auto& m : moves) { if (moveToUci(m) == uci) { chosen = m; found = true; break; } }
        if (!found) return {};
        makeMove(chosen);
        if (side_to_move == 0) fullmove_number++;
        flipPosition();
        return buildFen(); /* depth logic not here */
    }

    const int ChessEngine2::PIECE_VALUES[6] = { 100,300,300,500,900,10000 };

    void ChessEngine2::parseCastling(const std::string& c) { /* TODO: implement proper castling parsing */ }

    std::string ChessEngine2::getBestMove(int max_depth) {
        std::vector<Move> moves;
        generateLegalMoves(max_depth, moves);
        Move best{}; int best_score = -INF;
        for (auto& m : moves) {
            ChessEngine2 save = *this;
            makeMove(m);
            flipPosition();
            // Depth decrease occurs here when calling alphaBeta with (max_depth - 1)
            int score = -alphaBeta(max_depth - 1, -INF, INF, max_depth - 1);
            *this = save;
            if (score > best_score) { best_score = score; best = m; }
        }
        return moveToUci(best);
    }

    int ChessEngine2::alphaBeta(int depth, int alpha, int beta, int ply) {
        // Depth termination check
        if (depth == 0)
            return evaluate();

        std::vector<Move> moves;
        generateLegalMoves(ply, moves);
        if (moves.empty()) {
            int king_sq = ctz64(pieces[5]);
            return isSquareAttacked(king_sq) ? -10000 - (4 - depth) : 0;
        }
        for (auto& m : moves) {
            ChessEngine2 save = *this;
            makeMove(m);
            flipPosition();
            int score = -alphaBeta(depth - 1, -beta, -alpha, ply - 1);
            *this = save;
            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        }
        return alpha;
    }

    int ChessEngine2::evaluate() {
        int score = 0;
        for (int i = 0; i < 6; ++i) {
            score += popcount64(pieces[i]) * PIECE_VALUES[i];
            score -= popcount64(pieces[i + 6]) * PIECE_VALUES[i];
        }
        return score;
    }

    void ChessEngine2::generateLegalMoves(int ply, std::vector<Move>& moves) {
        std::vector<Move> pseudo;
        generatePseudoMoves(pseudo);
        addCastlingMoves(ply, pseudo);
        int king_sq = ctz64(pieces[5]);
        for (auto& m : pseudo) {
            ChessEngine2 save = *this;
            makeMove(m);
            int new_king = (m.from == king_sq) ? m.to : king_sq;
            if (!isSquareAttacked(new_king)) moves.push_back(m);
            *this = save;
        }
    }

    void ChessEngine2::addCastlingMoves(int ply, std::vector<Move>& pseudo) { /* TODO: implement castling legality */ }
    bool ChessEngine2::isCastlingLegal(int king_src, int king_dest, int rook_src) { /* TODO: implement full legality */ return true; }
    std::vector<int> ChessEngine2::getKingPathSquares(int src, int dest) { std::vector<int> path{ src }; int step = (dest > src) ? 1 : -1; for (int s = src + step; s != dest; s += step) path.push_back(s); path.push_back(dest); return path; }

    bool ChessEngine2::isSquareAttacked(int sq) {
        ChessEngine2 copy = *this;
        copy.flipPosition();
        std::vector<Move> opp;
        copy.generatePseudoMoves(opp);
        int flipped_sq = sq ^ 56; /* TODO: verify square flip mapping (sq ^ 56) correctness; incorrect mapping may cause missing attacks/captures */
        for (auto& m : opp)
            if (m.to == flipped_sq)
                return true;
        return false;
    }

    void ChessEngine2::generatePseudoMoves(std::vector<Move>& moves) {
        uint64_t friendly = 0, enemy = 0, occupied = 0;
        for (int i = 0; i < 6; ++i) friendly |= pieces[i];
        for (int i = 6; i < 12; ++i) enemy |= pieces[i];
        occupied = friendly | enemy;
        uint64_t pawns = pieces[0];
        while (pawns) {
            int from = ctz64(pawns);
            uint64_t attacks = pawnAttacksWhite(from) & enemy;
            uint64_t pushes = pawnPushesWhite(from) & ~occupied;
            while (pushes) {
                int to = ctz64(pushes);
                if ((to / 8) == 7) { for (int p = 1; p <= 4; ++p) moves.push_back({ from,to,p }); }
                else { moves.push_back({ from,to,0 }); }
                pushes &= pushes - 1;
            }
            while (attacks) {
                int to = ctz64(attacks);
                if ((to / 8) == 7) { for (int p = 1; p <= 4; ++p) moves.push_back({ from,to,p }); }
                else { moves.push_back({ from,to,0 }); }
                attacks &= attacks - 1;
            }
            if (ep_square != -1 && (pawnAttacksWhite(from) & (1ULL << ep_square))) moves.push_back({ from, ep_square,0 });
            pawns &= pawns - 1;
        }
        uint64_t knights = pieces[1];
        while (knights) {
            int from = ctz64(knights);
            uint64_t targets = knightAttacks(from) & ~friendly;
            while (targets) {
                int to = ctz64(targets);
                moves.push_back({ from,to,0 });
                targets &= targets - 1;
            }
            knights &= knights - 1;
        }
        uint64_t bishops = pieces[2];
        while (bishops) {
            int from = ctz64(bishops);
            addSliderMoves(from, moves, { 7,9,-7,-9 }, occupied, friendly);
            bishops &= bishops - 1;
        }
        uint64_t rooks = pieces[3];
        while (rooks) {
            int from = ctz64(rooks);
            addSliderMoves(from, moves, { 1,-1,8,-8 }, occupied, friendly);
            rooks &= rooks - 1;
        }
        uint64_t queens = pieces[4];
        while (queens) {
            int from = ctz64(queens);
            addSliderMoves(from, moves, { 7,9,-7,-9,1,-1,8,-8 }, occupied, friendly);
            queens &= queens - 1;
        }
        int king_sq = ctz64(pieces[5]);
        uint64_t k_attacks = kingAttacks(king_sq) & ~friendly;
        while (k_attacks) {
            int to = ctz64(k_attacks);
            moves.push_back({ king_sq,to,0 });
            k_attacks &= k_attacks - 1;
        }
    }

    void ChessEngine2::addSliderMoves(int from, std::vector<Move>& moves, const std::vector<int>& dirs, uint64_t occupied, uint64_t friendly) {
        for (int d : dirs) {
            int to = from + d;
            while (to >= 0 && to < 64 && std::abs((to % 8) - (from % 8)) <= std::abs(d % 8)) {
                if ((1ULL << to) & friendly) break;
                moves.push_back({ from,to,0 });
                if ((1ULL << to) & (occupied ^ friendly)) break;
                to += d;
            }
        }
    }

    void ChessEngine2::makeMove(const Move& m) {
        uint64_t from_bit = 1ULL << m.from;
        uint64_t to_bit = 1ULL << m.to;
        int ptype = getPieceType(m.from);
        int piece_idx = ptype;
        pieces[piece_idx] ^= from_bit;
        if (m.prom_piece) piece_idx = m.prom_piece;
        pieces[piece_idx] |= to_bit;
        int enemy_ptype = getPieceType(m.to, true);
        if (enemy_ptype != -1) { pieces[enemy_ptype + 6] ^= to_bit; halfmove_clock = 0; }
        else if (ptype == 0) halfmove_clock = 0; else halfmove_clock++;
        if (m.is_castling) { uint64_t r_from_bit = 1ULL << m.rook_from; uint64_t r_to_bit = 1ULL << m.rook_to; pieces[3] ^= r_from_bit; pieces[3] |= r_to_bit; }
        if (ptype == 0 && m.to == ep_square) { int enemy_pawn_sq = m.to - 8; pieces[6] ^= (1ULL << enemy_pawn_sq); halfmove_clock = 0; }
        if (ptype == 0 && (m.to - m.from == 16)) ep_square = m.from + 8; else ep_square = -1;
        if (ptype == 5) { white_kingside_rook_file = -1; white_queenside_rook_file = -1; }
        if (ptype == 3) { if (m.from % 8 == white_kingside_rook_file) white_kingside_rook_file = -1; if (m.from % 8 == white_queenside_rook_file) white_queenside_rook_file = -1; }
    }

    int ChessEngine2::getPieceType(int sq, bool enemy) { uint64_t bit = 1ULL << sq; int offset = enemy ? 6 : 0; for (int i = 0; i < 6; ++i) if (pieces[i + offset] & bit) return i; return -1; }
    std::string ChessEngine2::squareToAlg(int sq) { char file = 'a' + (sq % 8); char rank = '1' + (sq / 8); return { file,rank }; }
    uint64_t ChessEngine2::knightAttacks(int sq) { uint64_t a = 0; int f = sq % 8, r = sq / 8; const int ofs[8][2] = { {1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1} }; for (auto& o : ofs) { int nf = f + o[0], nr = r + o[1]; if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) a |= (1ULL << (nr * 8 + nf)); } return a; }
    uint64_t ChessEngine2::kingAttacks(int sq) { uint64_t a = 0; int f = sq % 8, r = sq / 8; for (int dr = -1; dr <= 1; ++dr) for (int df = -1; df <= 1; ++df) { if (!dr && !df) continue; int nf = f + df, nr = r + dr; if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) a |= (1ULL << (nr * 8 + nf)); } return a; }
    uint64_t ChessEngine2::pawnPushesWhite(int sq) { uint64_t push = 0; int r = sq / 8; int one = sq + 8; if (r < 7) push |= (1ULL << one); if (r == 1) push |= (1ULL << (sq + 16)); return push; }
    uint64_t ChessEngine2::pawnAttacksWhite(int sq) { uint64_t a = 0; int f = sq % 8, r = sq / 8; if (f > 0 && r < 7) a |= (1ULL << (sq + 7)); if (f < 7 && r < 7) a |= (1ULL << (sq + 9)); return a; }
} // namespace engine

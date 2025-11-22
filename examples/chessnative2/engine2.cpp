#include <cstdint>
#include <vector>
#include <functional>
#include <algorithm>
#include <string>
#include <cctype>  // for toupper/isupper
//#include <bitintrinsics>
#include <bit> // for std::byteswap (C++23) or provide a fallback for older standards
#if defined(_MSC_VER)
#include <intrin.h>
#endif

using uint64 = uint64_t;

// Portable wrappers for ctz/popcount
#if defined(_MSC_VER)
inline int ctz64(uint64_t x) { unsigned long idx; _BitScanForward64(&idx, x); return (int)idx; }
inline int popcount64(uint64_t x) { return (int)__popcnt64(x); }
#else
inline int ctz64(uint64_t x) { return __builtin_ctzll(x); }
inline int popcount64(uint64_t x) { return __builtin_popcountll(x); }
#endif

const int INF = 2000000;
const int PIECE_VALUES[6] = { 100, 300, 300, 500, 900, 10000 };  // P N B R Q K

struct Move {
    int from;
    int to;
    int prom_piece;  // 0 none, 1 N, 2 B, 3 R, 4 Q
    bool is_castling = false;
    int rook_from = -1;
    int rook_to = -1;
};

#if __cplusplus < 202302L
inline uint64_t byteswap64(uint64_t x) {
#if defined(_MSC_VER)
    return _byteswap_uint64(x);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_bswap64(x);
#else
    return ((x & 0x00000000000000FFULL) << 56) |
        ((x & 0x000000000000FF00ULL) << 40) |
        ((x & 0x0000000000FF0000ULL) << 24) |
        ((x & 0x00000000FF000000ULL) << 8) |
        ((x & 0x000000FF00000000ULL) >> 8) |
        ((x & 0x0000FF0000000000ULL) >> 24) |
        ((x & 0x00FF000000000000ULL) >> 40) |
        ((x & 0xFF00000000000000ULL) >> 56);
#endif
}
#endif

class ChessEngine {
public:
    uint64 pieces[12];
    int side_to_move = 0;  // Always 0 after load
    int white_kingside_rook_file = -1;
    int white_queenside_rook_file = -1;
    int black_kingside_rook_file = -1;
    int black_queenside_rook_file = -1;
    int ep_square = -1;
    int halfmove_clock = 0;
    int fullmove_number = 1;
    std::function<int(int, char)> kingDestCallback;

    ChessEngine() {}
    ChessEngine(const std::function<int(int, char)>& callback) : kingDestCallback(callback) {}

    void loadFEN(const std::string& fen) {
        // Simple FEN parser (assumes valid)
        std::fill(std::begin(pieces), std::end(pieces), 0ULL);
        size_t idx = 0;
        int sq = 56;  // Start a8 (63-56=7)
        while (fen[idx] != ' ') {
            char c = fen[idx++];
            if (c == '/') {
                sq -= 16;  // Next rank down
                continue;
            }
            if (isdigit(c)) {
                sq += (c - '0');
                continue;
            }
            int color_offset = isupper(c) ? 0 : 6;
            char uc = toupper(c);
            int ptype;
            if (uc == 'P') ptype = 0;
            else if (uc == 'N') ptype = 1;
            else if (uc == 'B') ptype = 2;
            else if (uc == 'R') ptype = 3;
            else if (uc == 'Q') ptype = 4;
            else if (uc == 'K') ptype = 5;
            pieces[ptype + color_offset] |= (1ULL << sq);
            sq++;
        }
        idx++;  // Skip space
        side_to_move = (fen[idx++] == 'w' ? 0 : 1);
        idx += 1;  // Space
        parseCastling(fen.substr(idx, fen.find(' ', idx) - idx));
        idx = fen.find(' ', idx) + 1;
        ep_square = (fen[idx] == '-' ? -1 : (fen[idx] - 'a') + (fen[idx + 1] - '1') * 8);
        idx = fen.find(' ', idx) + 1;
        halfmove_clock = std::stoi(fen.substr(idx, fen.find(' ', idx) - idx));
        idx = fen.find(' ', idx) + 1;
        fullmove_number = std::stoi(fen.substr(idx));

        if (side_to_move == 1) flipPosition();
    }

private:
    void parseCastling(const std::string& castling_str) {
        white_kingside_rook_file = -1;
        white_queenside_rook_file = -1;
        black_kingside_rook_file = -1;
        black_queenside_rook_file = -1;
        int w_king_file = ctz64(pieces[5]) % 8;
        int b_king_file = ctz64(pieces[11]) % 8;
        for (char c : castling_str) {
            if (c == '-') break;
            if (c == 'K') white_kingside_rook_file = 7;
            else if (c == 'Q') white_queenside_rook_file = 0;
            else if (c == 'k') black_kingside_rook_file = 7;
            else if (c == 'q') black_queenside_rook_file = 0;
            else if (isupper(c)) {
                int file = c - 'A';
                if (file > w_king_file) white_kingside_rook_file = std::max(white_kingside_rook_file, file);
                else white_queenside_rook_file = std::min(white_queenside_rook_file == -1 ? file : white_queenside_rook_file, file);
            }
            else if (islower(c)) {
                int file = c - 'a';
                if (file > b_king_file) black_kingside_rook_file = std::max(black_kingside_rook_file, file);
                else black_queenside_rook_file = std::min(black_queenside_rook_file == -1 ? file : black_queenside_rook_file, file);
            }
        }
    }

    void flipPosition() {
        for (int i = 0; i < 6; ++i) {
#if __cplusplus >= 202302L
            pieces[i] = std::byteswap(pieces[i]);
            pieces[i + 6] = std::byteswap(pieces[i + 6]);
#else
            pieces[i] = byteswap64(pieces[i]);
            pieces[i + 6] = byteswap64(pieces[i + 6]);
#endif
            std::swap(pieces[i], pieces[i + 6]);
        }
        if (ep_square != -1) ep_square ^= 56;
        std::swap(white_kingside_rook_file, black_kingside_rook_file);
        std::swap(white_queenside_rook_file, black_queenside_rook_file);
        side_to_move = 0;
    }

public:
    std::string getBestMove(int max_depth = 4) {  // Returns UCI "e2e4"
        std::vector<Move> moves;
        generateLegalMoves(max_depth, moves);
        Move best;
        int best_score = -INF;
        for (const auto& m : moves) {
            ChessEngine save = *this;  // Copy
            makeMove(m);
            flipPosition();
            int score = -alphaBeta(max_depth - 1, -INF, INF, max_depth - 1);
            *this = save;
            if (score > best_score) {
                best_score = score;
                best = m;
            }
        }
        // Convert to UCI
        std::string uci = squareToAlg(best.from) + squareToAlg(best.to);
        if (best.prom_piece) uci += "nbrq"[best.prom_piece - 1];
        return uci;
    }

private:
    int alphaBeta(int depth, int alpha, int beta, int ply_remaining) {
        if (depth == 0) return evaluate();

        std::vector<Move> moves;
        generateLegalMoves(ply_remaining, moves);
        if (moves.empty()) {
            int king_sq = ctz64(pieces[5]);
            return isSquareAttacked(king_sq) ? -10000 - (4 - depth) : 0;  // Adjust for max_depth=4 example
        }

        for (const auto& m : moves) {
            ChessEngine save = *this;
            makeMove(m);
            flipPosition();
            int score = -alphaBeta(depth - 1, -beta, -alpha, ply_remaining - 1);
            *this = save;
            if (score >= beta) return beta;
            alpha = std::max(alpha, score);
        }
        return alpha;
    }

    int evaluate() {
        int score = 0;
        for (int i = 0; i < 6; ++i) {
            score += popcount64(pieces[i]) * PIECE_VALUES[i];
            score -= popcount64(pieces[i + 6]) * PIECE_VALUES[i];
        }
        return score;
    }

    void generateLegalMoves(int ply_remaining, std::vector<Move>& moves) {
        std::vector<Move> pseudo;
        generatePseudoMoves(pseudo);
        addCastlingMoves(ply_remaining, pseudo);

        int king_sq = ctz64(pieces[5]);
        for (const auto& m : pseudo) {
            ChessEngine save = *this;
            makeMove(m);
            int new_king_sq = (m.from == king_sq) ? m.to : king_sq;
            if (!isSquareAttacked(new_king_sq)) {
                moves.push_back(m);
            }
            *this = save;
        }
    }

    void addCastlingMoves(int ply_remaining, std::vector<Move>& pseudo) {
        int king_sq = ctz64(pieces[5]);
        int king_file = king_sq % 8;
        int rank = king_sq / 8;  // Should be 0 for white

        auto tryCastle = [&](char side, int& rook_file_ref) {
            if (rook_file_ref == -1) return;
            int king_dest = kingDestCallback(ply_remaining, side);
            int dest_file = king_dest % 8;
            int rook_file = rook_file_ref;
            int rook_sq = rank * 8 + rook_file;
            int rook_dest = (dest_file > king_file) ? (king_dest - 1) : (king_dest + 1);

            if (isCastlingLegal(king_sq, king_dest, rook_sq)) {
                Move m{ king_sq, king_dest, 0 };
                m.is_castling = true;
                m.rook_from = rook_sq;
                m.rook_to = rook_dest;
                pseudo.push_back(m);
            }
            };

        tryCastle('K', white_kingside_rook_file);
        tryCastle('Q', white_queenside_rook_file);
    }

    bool isCastlingLegal(int king_src, int king_dest, int rook_src) {
        uint64 occupied = 0;
        for (int i = 0; i < 12; ++i) occupied |= pieces[i];
        int min_file = std::min({ king_src % 8, king_dest % 8, rook_src % 8 });
        int max_file = std::max({ king_src % 8, king_dest % 8, rook_src % 8 });
        uint64 between = 0;
        for (int f = min_file + 1; f < max_file; ++f) {
            between |= (1ULL << (0 * 8 + f));  // Rank 0
        }
        if ((occupied & between) != 0) return false;

        auto path = getKingPathSquares(king_src, king_dest);
        for (int sq : path) {
            ChessEngine temp = *this;
            if (sq != king_src) {
                temp.pieces[5] ^= (1ULL << king_src);
                temp.pieces[5] |= (1ULL << sq);
            }
            if (temp.isSquareAttacked(sq)) return false;
        }
        return true;
    }

    std::vector<int> getKingPathSquares(int src, int dest) {
        std::vector<int> path{ src };
        int step = (dest > src) ? 1 : -1;
        for (int s = src + step; s != dest; s += step) {
            path.push_back(s);
        }
        path.push_back(dest);
        return path;
    }

    bool isSquareAttacked(int sq) {
        ChessEngine copy = *this;
        copy.flipPosition();
        std::vector<Move> opp_pseudo;
        copy.generatePseudoMoves(opp_pseudo);
        int flipped_sq = sq ^ 56;
        for (const auto& m : opp_pseudo) {
            if (m.to == flipped_sq) return true;
        }
        return false;
    }

    void generatePseudoMoves(std::vector<Move>& moves) {
        uint64 occupied = 0, friendly = 0, enemy = 0;
        for (int i = 0; i < 6; ++i) friendly |= pieces[i];
        for (int i = 6; i < 12; ++i) enemy |= pieces[i];
        occupied = friendly | enemy;

        // Pawns
        uint64 pawns = pieces[0];
        while (pawns) {
            int from = ctz64(pawns);
            uint64 attacks = pawnAttacksWhite(from) & enemy;
            uint64 pushes = pawnPushesWhite(from) & ~occupied;
            // Pushes
            while (pushes) {
                int to = ctz64(pushes);
                if ((to / 8) == 7) {  // Prom
                    for (int p = 1; p <= 4; ++p) moves.push_back({ from, to, p });
                }
                else {
                    moves.push_back({ from, to, 0 });
                }
                pushes &= pushes - 1;
            }
            // Attacks
            while (attacks) {
                int to = ctz64(attacks);
                if ((to / 8) == 7) {
                    for (int p = 1; p <= 4; ++p) moves.push_back({ from, to, p });
                }
                else {
                    moves.push_back({ from, to, 0 });
                }
                attacks &= attacks - 1;
            }
            // EP
            if (ep_square != -1) {
                if ((pawnAttacksWhite(from) & (1ULL << ep_square))) {
                    moves.push_back({ from, ep_square, 0 });
                }
            }
            pawns &= pawns - 1;
        }

        // Knights
        uint64 knights = pieces[1];
        while (knights) {
            int from = ctz64(knights);
            uint64 targets = knightAttacks(from) & ~friendly;
            while (targets) {
                int to = ctz64(targets);
                moves.push_back({ from, to, 0 });
                targets &= targets - 1;
            }
            knights &= knights - 1;
        }

        // Bishops (simple loop, no magic)
        uint64 bishops = pieces[2];
        while (bishops) {
            int from = ctz64(bishops);
            addSliderMoves(from, moves, { 7, 9, -7, -9 }, occupied, friendly);  // Diags
            bishops &= bishops - 1;
        }

        // Rooks
        uint64 rooks = pieces[3];
        while (rooks) {
            int from = ctz64(rooks);
            addSliderMoves(from, moves, { 1, -1, 8, -8 }, occupied, friendly);  // Ortho
            rooks &= rooks - 1;
        }

        // Queens
        uint64 queens = pieces[4];
        while (queens) {
            int from = ctz64(queens);
            addSliderMoves(from, moves, { 7, 9, -7, -9, 1, -1, 8, -8 }, occupied, friendly);
            queens &= queens - 1;
        }

        // King
        int king_sq = ctz64(pieces[5]);
        uint64 k_attacks = kingAttacks(king_sq) & ~friendly;
        while (k_attacks) {
            int to = ctz64(k_attacks);
            moves.push_back({ king_sq, to, 0 });
            k_attacks &= k_attacks - 1;
        }
    }

    void addSliderMoves(int from, std::vector<Move>& moves, const std::vector<int>& dirs, uint64 occupied, uint64 friendly) {
        for (int d : dirs) {
            int to = from + d;
            while (to >= 0 && to < 64 && std::abs((to % 8) - (from % 8)) <= std::abs(d % 8)) {  // On board
                if ((1ULL << to) & friendly) break;
                moves.push_back({ from, to, 0 });
                if ((1ULL << to) & (occupied ^ friendly)) break;  // Capture stop
                to += d;
            }
        }
    }

    void makeMove(const Move& m) {
        uint64 from_bit = 1ULL << m.from;
        uint64 to_bit = 1ULL << m.to;
        int ptype = getPieceType(m.from);
        int color_offset = 0;  // White
        int piece_idx = ptype + color_offset;
        pieces[piece_idx] ^= from_bit;
        if (m.prom_piece) {
            piece_idx = m.prom_piece + color_offset;  // 1=N etc.
        }
        pieces[piece_idx] |= to_bit;

        // Capture
        int enemy_ptype = getPieceType(m.to, true);  // true for enemy
        if (enemy_ptype != -1) {
            pieces[enemy_ptype + 6] ^= to_bit;
            halfmove_clock = 0;
        }
        else if (ptype == 0) halfmove_clock = 0;
        else halfmove_clock++;

        // Castling
        if (m.is_castling) {
            uint64 r_from_bit = 1ULL << m.rook_from;
            uint64 r_to_bit = 1ULL << m.rook_to;
            pieces[3] ^= r_from_bit;
            pieces[3] |= r_to_bit;
        }

        // EP
        if (ptype == 0 && m.to == ep_square) {
            int enemy_pawn_sq = m.to - 8;
            pieces[6] ^= (1ULL << enemy_pawn_sq);
            halfmove_clock = 0;
        }

        // Set new EP if double push
        if (ptype == 0 && (m.to - m.from == 16)) {
            ep_square = m.from + 8;
        }
        else {
            ep_square = -1;
        }

        // Lose castling
        if (ptype == 5) {
            white_kingside_rook_file = -1;
            white_queenside_rook_file = -1;
        }
        if (ptype == 3) {
            if (m.from % 8 == white_kingside_rook_file) white_kingside_rook_file = -1;
            if (m.from % 8 == white_queenside_rook_file) white_queenside_rook_file = -1;
        }
        if (enemy_ptype == 3) {  // Captured enemy rook, but since white POV, enemy is black, but lose? No, opponent rights not updated here
            // Flip will handle
        }
    }

    int getPieceType(int sq, bool enemy = false) {
        uint64 bit = 1ULL << sq;
        int offset = enemy ? 6 : 0;
        for (int i = 0; i < 6; ++i) {
            if (pieces[i + offset] & bit) return i;
        }
        return -1;
    }

    std::string squareToAlg(int sq) {
        char file = 'a' + (sq % 8);
        char rank = '1' + (sq / 8);
        return { file, rank };
    }

    // Precomputed attacks (implement as arrays or functions)
    uint64 knightAttacks(int sq) {
        // Standard knight moves, boundary check
        uint64 attacks = 0ULL;
        std::vector<int> offsets = { 17, 15, 10, 6, -6, -10, -15, -17 };
        for (int off : offsets) {
            int to = sq + off;
            if (to >= 0 && to < 64 && std::abs((to % 8) - (sq % 8)) <= 2) {
                attacks |= (1ULL << to);
            }
        }
        return attacks;
    }

    uint64 kingAttacks(int sq) {
        uint64 attacks = 0ULL;
        std::vector<int> offsets = { 1, 7, 8, 9, -1, -7, -8, -9 };
        for (int off : offsets) {
            int to = sq + off;
            if (to >= 0 && to < 64 && std::abs((to % 8) - (sq % 8)) <= 1) {
                attacks |= (1ULL << to);
            }
        }
        return attacks;
    }

    uint64 pawnPushesWhite(int sq) {
        uint64 pushes = (1ULL << (sq + 8));
        if ((sq / 8) == 1) pushes |= (1ULL << (sq + 16));  // Double
        return pushes;
    }

    uint64 pawnAttacksWhite(int sq) {
        uint64 attacks = 0ULL;
        if ((sq % 8) > 0) attacks |= (1ULL << (sq + 7));
        if ((sq % 8) < 7) attacks |= (1ULL << (sq + 9));
        return attacks;
    }
};



#pragma once
#include "EngineBase.h"
#include <cstdint>
#include <vector>
#include <functional>
#include <string>
namespace engine
{
    class ChessEngine2 : public EngineBase {
    public:
        uint64_t pieces[12];
        int side_to_move = 0;
        int white_kingside_rook_file = -1;
        int white_queenside_rook_file = -1;
        int black_kingside_rook_file = -1;
        int black_queenside_rook_file = -1;
        int ep_square = -1;
        int halfmove_clock = 0;
        int fullmove_number = 1;
        std::function<int(int, char)> kingDestCallback;

        ChessEngine2() = default;
        ChessEngine2(const std::function<int(int, char)>& cb) : kingDestCallback(cb) {}

        void loadFEN(const std::string& fen);

        std::string choose_move(const std::string& fen, int depth) override;
        std::vector<std::pair<std::string, int>> root_search_scores(const std::string& fen, int depth) override;
        std::vector<std::string> legal_moves_uci(const std::string& fen) override;
        std::string apply_move(const std::string& fen, const std::string& uci) override;

        std::string getBestMove(int max_depth = 4);

    private:
        struct Move { int from; int to; int prom_piece; bool is_castling = false; int rook_from = -1; int rook_to = -1; };
        static constexpr int INF = 2000000;
        static const int PIECE_VALUES[6];
#if defined(_MSC_VER)
        inline int ctz64(uint64_t x) { unsigned long idx; _BitScanForward64(&idx, x); return (int)idx; }
        inline int popcount64(uint64_t x) { return (int)__popcnt64(x); }
#else
        inline int ctz64(uint64_t x) { return __builtin_ctzll(x); }
        inline int popcount64(uint64_t x) { return __builtin_popcountll(x); }
#endif
        void parseCastling(const std::string& s);
        void flipPosition();
        int alphaBeta(int depth, int alpha, int beta, int ply_remaining);
        int evaluate();
        void generateLegalMoves(int ply_remaining, std::vector<Move>& moves);
        void addCastlingMoves(int ply_remaining, std::vector<Move>& pseudo);
        bool isCastlingLegal(int king_src, int king_dest, int rook_src);
        std::vector<int> getKingPathSquares(int src, int dest);
        bool isSquareAttacked(int sq);
        void generatePseudoMoves(std::vector<Move>& moves);
        void addSliderMoves(int from, std::vector<Move>& moves, const std::vector<int>& dirs, uint64_t occupied, uint64_t friendly);
        void makeMove(const Move& m);
        int getPieceType(int sq, bool enemy = false);
        std::string squareToAlg(int sq); std::string moveToUci(const Move& m) { std::string u = squareToAlg(m.from) + squareToAlg(m.to); if (m.prom_piece) u += "nbrq"[m.prom_piece - 1]; return u; }
        uint64_t knightAttacks(int sq); uint64_t kingAttacks(int sq); uint64_t pawnPushesWhite(int sq); uint64_t pawnAttacksWhite(int sq);
    };
}


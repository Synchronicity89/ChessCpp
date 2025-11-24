#pragma once
#include "EngineBase.h"
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <array>

namespace engine {

class ChessEngine1 : public EngineBase {
public:
    ChessEngine1() = default;
    using U64 = std::uint64_t;
    struct Bitboards { U64 WP{},WN{},WB{},WR{},WQ{},WK{}; U64 BP{},BN{},BB{},BR{},BQ{},BK{}; U64 occWhite{},occBlack{},occAll{}; };
    struct Position { Bitboards bb; int sideToMove=0; int castleRights=0; int epSquare=-1; int halfmoveClock=0; int fullmoveNumber=1; };
    struct Move { int from{}, to{}, promo{}; bool isCapture=false; bool isEnPassant=false; bool isCastle=false; bool isDoublePawnPush=false; };

    // EngineBase interface
    std::string choose_move(const std::string& fen, int depth) override;
    std::vector<std::pair<std::string,int>> root_search_scores(const std::string& fen, int depth) override;
    std::vector<std::string> legal_moves_uci(const std::string& fen) override;
    std::string apply_move(const std::string& fen, const std::string& uci) override;

private:
    static std::array<U64,64> pawnAttW;
    static std::array<U64,64> pawnAttB;
    static std::array<U64,64> knightMask;
    static std::array<U64,64> kingMask;
    static bool masksInit;

    static inline U64 bb(int sq){ return 1ULL<<sq; }
    static inline int file_of(int sq){ return sq & 7; }
    static inline int rank_of(int sq){ return sq >> 3; }
#if defined(_MSC_VER)
    static inline int lsb_index(U64 x){ if(!x) return -1; unsigned long idx; _BitScanForward64(&idx,x); return (int)idx; }
    static inline int popcount64(U64 x){ return (int)__popcnt64(x); }
#else
    static inline int lsb_index(U64 x){ return x ? (int)__builtin_ctzll(x) : -1; }
    static inline int popcount64(U64 x){ return (int)__builtin_popcountll(x); }
#endif
    static void init_masks();
    static bool parse_fen(const std::string& fen, Position& out);
    static void generate_pseudo_moves(const Position& pos, std::vector<Move>& out);
    static void filter_legal(const Position& pos, const std::vector<Move>& pseudo, std::vector<Move>& legal);
    static U64 attackers_to(const Position& pos, int sq, int byWhite);
    static bool square_attacked(const Position& pos, int sq, int byWhite);
    static void apply_move(const Position& pos, const Move& m, Position& out);
    static int evaluate_material(const Position& pos);
    static int evaluate(const Position& pos);
    static int negamax(Position& pos,int depth,int alpha,int beta,std::vector<Move>& pv);
    static std::string move_to_uci(const Move& m);
    static U64 rook_attacks(int sq,U64 occ);
    static U64 bishop_attacks(int sq,U64 occ);
    static U64 can_castle(const Position& pos,bool white,bool kingside);

    std::string choose_move_internal(const std::string& fen,int depth);
    std::vector<std::pair<std::string,int>> root_scores_internal(const std::string& fen,int depth);
    std::vector<std::string> legal_moves_internal(const std::string& fen);
    static std::string build_fen(const Position& p);
};

} // namespace engine

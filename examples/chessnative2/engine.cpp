#include "engine.hpp"
#include "nnue.hpp"
#include <array>
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <random>
#include <limits>

namespace engine {

static const int pieceValue[6] = {100,320,330,500,900,0};
static std::array<uint64_t,64> pawnAttW{}; // squares attacked by white pawn from index
static std::array<uint64_t,64> pawnAttB{}; // squares attacked by black pawn from index
static std::array<uint64_t,64> knightMask{};
static std::array<uint64_t,64> kingMask{};
static bool masksInit=false;

inline uint64_t bb(int sq){ return 1ULL<<sq; }
inline int lsb_index(uint64_t x){
#if defined(_MSC_VER)
    unsigned long idx; _BitScanForward64(&idx,x); return (int)idx;
#else
    return (int)__builtin_ctzll(x);
#endif
}
inline int popcount64(uint64_t x){
#if defined(_MSC_VER)
    return (int)__popcnt64(x);
#else
    return (int)__builtin_popcountll(x);
#endif
}

void init_masks(){ if(masksInit) return; for(int sq=0;sq<64;++sq){ int r=rank_of(sq), f=file_of(sq);
        if(r<7){ if(f>0) pawnAttW[sq] |= bb(sq+8-1); if(f<7) pawnAttW[sq] |= bb(sq+8+1);} // white pawn attacks
        if(r>0){ if(f>0) pawnAttB[sq] |= bb(sq-8-1); if(f<7) pawnAttB[sq] |= bb(sq-8+1);} // black pawn attacks
        const int kd[8][2]={{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
        for(auto &d: kd){ int rr=r+d[0], ff=f+d[1]; if(rr>=0&&rr<8&&ff>=0&&ff<8) knightMask[sq] |= bb(rr*8+ff); }
        for(int dr=-1; dr<=1; ++dr) for(int dc=-1; dc<=1; ++dc){ if(!dr && !dc) continue; int rr=r+dr, ff=f+dc; if(rr>=0&&rr<8&&ff>=0&&ff<8) kingMask[sq] |= bb(rr*8+ff); }
    } masksInit=true; }

static uint64_t rook_attacks(int sq, uint64_t occ){ uint64_t a=0; int r=rank_of(sq), f=file_of(sq);
    for(int rr=r+1; rr<8; ++rr){ int s=rr*8+f; a|=bb(s); if(occ & bb(s)) break; }
    for(int rr=r-1; rr>=0; --rr){ int s=rr*8+f; a|=bb(s); if(occ & bb(s)) break; }
    for(int ff=f+1; ff<8; ++ff){ int s=r*8+ff; a|=bb(s); if(occ & bb(s)) break; }
    for(int ff=f-1; ff>=0; --ff){ int s=r*8+ff; a|=bb(s); if(occ & bb(s)) break; }
    return a; }
static uint64_t bishop_attacks(int sq, uint64_t occ){ uint64_t a=0; int r=rank_of(sq), f=file_of(sq);
    for(int rr=r+1, ff=f+1; rr<8 && ff<8; ++rr,++ff){ int s=rr*8+ff; a|=bb(s); if(occ & bb(s)) break; }
    for(int rr=r+1, ff=f-1; rr<8 && ff>=0; ++rr,--ff){ int s=rr*8+ff; a|=bb(s); if(occ & bb(s)) break; }
    for(int rr=r-1, ff=f+1; rr>=0 && ff<8; --rr,++ff){ int s=rr*8+ff; a|=bb(s); if(occ & bb(s)) break; }
    for(int rr=r-1, ff=f-1; rr>=0 && ff>=0; --rr,--ff){ int s=rr*8+ff; a|=bb(s); if(occ & bb(s)) break; }
    return a; }

bool parse_fen(const std::string& fen, Position& out){ init_masks(); out=Position(); std::array<char,64> sqArr; sqArr.fill('.');
    std::vector<std::string> tok; tok.reserve(6); std::string cur; for(char c: fen){ if(c==' '){ if(!cur.empty()){ tok.push_back(cur); cur.clear(); } } else cur.push_back(c);} if(!cur.empty()) tok.push_back(cur);
    if(tok.size()<2) return false; int idx=56; for(char c: tok[0]){ if(c=='/'){ idx -= 16; continue; } if(c>='1'&&c<='8'){ idx += (c-'0'); continue; } if(idx<0||idx>=64) return false; sqArr[idx++]=c; }
    auto add=[&](char pc,int s){ uint64_t b=bb(s); switch(pc){ case 'P': out.bb.WP|=b; break; case 'N': out.bb.WN|=b; break; case 'B': out.bb.WB|=b; break; case 'R': out.bb.WR|=b; break; case 'Q': out.bb.WQ|=b; break; case 'K': out.bb.WK|=b; break; case 'p': out.bb.BP|=b; break; case 'n': out.bb.BN|=b; break; case 'b': out.bb.BB|=b; break; case 'r': out.bb.BR|=b; break; case 'q': out.bb.BQ|=b; break; case 'k': out.bb.BK|=b; break; } };
    for(int s=0;s<64;++s) if(sqArr[s] != '.') add(sqArr[s], s);
    out.bb.occWhite = out.bb.WP|out.bb.WN|out.bb.WB|out.bb.WR|out.bb.WQ|out.bb.WK;
    out.bb.occBlack = out.bb.BP|out.bb.BN|out.bb.BB|out.bb.BR|out.bb.BQ|out.bb.BK;
    out.bb.occAll = out.bb.occWhite | out.bb.occBlack;
    out.sideToMove = (tok[1]=="w"?0:1);
    out.castleRights=0; if(tok.size()>=3){ std::string c=tok[2]; if(c.find('K')!=std::string::npos) out.castleRights|=1; if(c.find('Q')!=std::string::npos) out.castleRights|=2; if(c.find('k')!=std::string::npos) out.castleRights|=4; if(c.find('q')!=std::string::npos) out.castleRights|=8; }
    out.epSquare=-1; if(tok.size()>=4 && tok[3]!="-"){ std::string ep=tok[3]; if(ep.size()==2){ int f=ep[0]-'a', r=ep[1]-'1'; if(f>=0&&f<8&&r>=0&&r<8) out.epSquare=r*8+f; } }
    if(tok.size()>=5) out.halfmoveClock = std::atoi(tok[4].c_str()); if(tok.size()>=6) out.fullmoveNumber = std::atoi(tok[5].c_str());
    return true; }

std::string to_fen(const Position& pos){ auto pieceAt=[&](int sq){ uint64_t b=bb(sq); if(pos.bb.WP&b) return 'P'; if(pos.bb.WN&b) return 'N'; if(pos.bb.WB&b) return 'B'; if(pos.bb.WR&b) return 'R'; if(pos.bb.WQ&b) return 'Q'; if(pos.bb.WK&b) return 'K'; if(pos.bb.BP&b) return 'p'; if(pos.bb.BN&b) return 'n'; if(pos.bb.BB&b) return 'b'; if(pos.bb.BR&b) return 'r'; if(pos.bb.BQ&b) return 'q'; if(pos.bb.BK&b) return 'k'; return '.'; };
    std::string s; for(int rank=7; rank>=0; --rank){ int empty=0; for(int file=0; file<8; ++file){ int sq=rank*8+file; char pc=pieceAt(sq); if(pc=='.'){ ++empty; } else { if(empty){ s.push_back(char('0'+empty)); empty=0;} s.push_back(pc); } } if(empty) s.push_back(char('0'+empty)); if(rank) s.push_back('/'); }
    s += pos.sideToMove==0?" w ":" b "; std::string cast; if(pos.castleRights&1) cast+='K'; if(pos.castleRights&2) cast+='Q'; if(pos.castleRights&4) cast+='k'; if(pos.castleRights&8) cast+='q'; if(cast.empty()) cast="-"; s += cast + ' ';
    if(pos.epSquare>=0){ int f=file_of(pos.epSquare), r=rank_of(pos.epSquare); s.push_back(char('a'+f)); s.push_back(char('1'+r)); } else s+="-"; s += ' '+std::to_string(pos.halfmoveClock)+' '+std::to_string(pos.fullmoveNumber); return s; }

int evaluate_material(const Position& pos){ int w=0,b=0; auto add=[&](uint64_t bb,int val,bool white){ for(;bb; bb &= bb-1){ if(white) w+=val; else b+=val; } }; add(pos.bb.WP,pieceValue[0],true); add(pos.bb.BP,pieceValue[0],false); add(pos.bb.WN,pieceValue[1],true); add(pos.bb.BN,pieceValue[1],false); add(pos.bb.WB,pieceValue[2],true); add(pos.bb.BB,pieceValue[2],false); add(pos.bb.WR,pieceValue[3],true); add(pos.bb.BR,pieceValue[3],false); add(pos.bb.WQ,pieceValue[4],true); add(pos.bb.BQ,pieceValue[4],false); return w-b; }
int evaluate(const Position& pos){ return evaluate_material(pos) + nnue_eval(pos); }

// Return bitboard of pieces from side 'byWhite' attacking 'sq'.
uint64_t attackers_to(const Position& pos, int sq, int byWhite)
{
    uint64_t attackers = 0ULL;
    uint64_t occ = pos.bb.occAll;
    int f = file_of(sq);
    int r = rank_of(sq);

    // Pawns (reverse map)
    if (byWhite)
    {
        // White pawns that could attack sq came from sq-7 (file +1) or sq-9 (file -1)
        if (r > 0)
        {
            if (f < 7)
            {
                int p = sq - 7;
                if (p >= 0 && (pos.bb.WP & (1ULL << p))) attackers |= (1ULL << p);
            }
            if (f > 0)
            {
                int p = sq - 9;
                if (p >= 0 && (pos.bb.WP & (1ULL << p))) attackers |= (1ULL << p);
            }
        }
    }
    else
    {
        // Black pawns that could attack sq came from sq+7 (file -1) or sq+9 (file +1)
        if (r < 7)
        {
            if (f > 0)
            {
                int p = sq + 7;
                if (p < 64 && (pos.bb.BP & (1ULL << p))) attackers |= (1ULL << p);
            }
            if (f < 7)
            {
                int p = sq + 9;
                if (p < 64 && (pos.bb.BP & (1ULL << p))) attackers |= (1ULL << p);
            }
        }
    }

    // Knights
    const uint64_t knightSources = byWhite ? pos.bb.WN : pos.bb.BN;
    attackers |= knightMask[sq] & knightSources;

    // Kings
    const uint64_t kingSource = byWhite ? pos.bb.WK : pos.bb.BK;
    attackers |= kingMask[sq] & kingSource;

    // Sliding pieces (bishops / rooks / queens) via ray tracing from target outward:
    auto ray_scan = [&](const int* df, const int* dr, int dirCount, uint64_t pieceMask)
    {
        for (int d = 0; d < dirCount; ++d)
        {
            int cf = f;
            int cr = r;
            while (true)
            {
                cf += df[d];
                cr += dr[d];
                if (cf < 0 || cf > 7 || cr < 0 || cr > 7) break;
                int s = cr * 8 + cf;
                uint64_t b = (1ULL << s);
                if (occ & b)
                {
                    if (pieceMask & b)
                        attackers |= b;
                    break; // blocked
                }
            }
        }
    };

    // Bishop/diagonal directions
    const int bdf[4] = { 1, 1, -1, -1 };
    const int bdr[4] = { 1, -1, 1, -1 };
    uint64_t bishopsQueens = byWhite ? (pos.bb.WB | pos.bb.WQ) : (pos.bb.BB | pos.bb.BQ);
    ray_scan(bdf, bdr, 4, bishopsQueens);

    // Rook/orthogonal directions
    const int rdf[4] = { 1, -1, 0, 0 };
    const int rdr[4] = { 0, 0, 1, -1 };
    uint64_t rooksQueens = byWhite ? (pos.bb.WR | pos.bb.WQ) : (pos.bb.BR | pos.bb.BQ);
    ray_scan(rdf, rdr, 4, rooksQueens);

    return attackers;
}

bool square_attacked(const Position& pos, int sq, int byWhite)
{
    return attackers_to(pos, sq, byWhite) != 0ULL;
}

void apply_move(const Position& pos, const Move& m, Position& out){ out=pos; uint64_t fromB=bb(m.from), toB=bb(m.to); bool white= (pos.sideToMove==0); auto movePiece=[&](uint64_t &bbt){ if(bbt & fromB){ bbt ^= fromB; bbt |= toB; } };
    if(m.isCapture){ if(white){ out.bb.BP&=~toB; out.bb.BN&=~toB; out.bb.BB&=~toB; out.bb.BR&=~toB; out.bb.BQ&=~toB; out.bb.BK&=~toB; } else { out.bb.WP&=~toB; out.bb.WN&=~toB; out.bb.WB&=~toB; out.bb.WR&=~toB; out.bb.WQ&=~toB; out.bb.WK&=~toB; } }
    if(white){ movePiece(out.bb.WP); movePiece(out.bb.WN); movePiece(out.bb.WB); movePiece(out.bb.WR); movePiece(out.bb.WQ); movePiece(out.bb.WK);} else { movePiece(out.bb.BP); movePiece(out.bb.BN); movePiece(out.bb.BB); movePiece(out.bb.BR); movePiece(out.bb.BQ); movePiece(out.bb.BK);} if(m.promo){ if(white){ out.bb.WP&=~toB; switch(std::tolower((unsigned char)m.promo)){ case 'n': out.bb.WN|=toB; break; case 'b': out.bb.WB|=toB; break; case 'r': out.bb.WR|=toB; break; default: out.bb.WQ|=toB; break; } } else { out.bb.BP&=~toB; switch(std::tolower((unsigned char)m.promo)){ case 'n': out.bb.BN|=toB; break; case 'b': out.bb.BB|=toB; break; case 'r': out.bb.BR|=toB; break; default: out.bb.BQ|=toB; break; } } }
    if(m.isCastle){
        if(white){
            if(m.to==6){ // white O-O (e1->g1)
                out.bb.WR &= ~bb(7);
                out.bb.WR |= bb(5);
            } else if(m.to==2){ // white O-O-O (e1->c1)
                out.bb.WR &= ~bb(0);
                out.bb.WR |= bb(3);
            }
        } else {
            if(m.to==62){ // black O-O (e8->g8)
                out.bb.BR &= ~bb(63);
                out.bb.BR |= bb(61);
            } else if(m.to==58){ // black O-O-O (e8->c8)
                out.bb.BR &= ~bb(56);
                out.bb.BR |= bb(59);
            }
        }
    }
    out.bb.occWhite = out.bb.WP|out.bb.WN|out.bb.WB|out.bb.WR|out.bb.WQ|out.bb.WK; out.bb.occBlack = out.bb.BP|out.bb.BN|out.bb.BB|out.bb.BR|out.bb.BQ|out.bb.BK; out.bb.occAll = out.bb.occWhite | out.bb.occBlack;
    if(!white) out.fullmoveNumber++; out.sideToMove = white?1:0; out.epSquare=-1; auto strip=[&](int mask){ out.castleRights &= ~mask; }; // update castle rights only for affected side pieces
    if(m.from==4) strip(1|2); // white king moved
    if(m.from==60) strip(4|8); // black king moved
    if(m.from==0||m.to==0) strip(2); // white queenside rook moved/captured
    if(m.from==7||m.to==7) strip(1); // white kingside rook moved/captured
    if(m.from==56||m.to==56) strip(8); // black queenside rook moved/captured
    if(m.from==63||m.to==63) strip(4); // black kingside rook moved/captured
}

static void add_move(std::vector<Move>& v,int from,int to,bool cap=false,char promo=0,bool castle=false){ Move m; m.from=from; m.to=to; m.isCapture=cap; m.promo=promo; m.isCastle=castle; m.isEnPassant=false; m.isDoublePawnPush=false; v.push_back(m);} 
static bool can_castle(const Position& pos,bool white,bool kingside){ if(white){ if(kingside){ if(!(pos.castleRights&1)) return false; if(!(pos.bb.WK & bb(4))) return false; if(pos.bb.occAll & (bb(5)|bb(6))) return false; if(square_attacked(pos,4,false)||square_attacked(pos,5,false)||square_attacked(pos,6,false)) return false; return true;} else { if(!(pos.castleRights&2)) return false; if(!(pos.bb.WK & bb(4))) return false; if(pos.bb.occAll & (bb(1)|bb(2)|bb(3))) return false; if(square_attacked(pos,4,false)||square_attacked(pos,3,false)||square_attacked(pos,2,false)) return false; return true; } } else { if(kingside){ if(!(pos.castleRights&4)) return false; if(!(pos.bb.BK & bb(60))) return false; if(pos.bb.occAll & (bb(61)|bb(62))) return false; if(square_attacked(pos,60,true)||square_attacked(pos,61,true)||square_attacked(pos,62,true)) return false; return true;} else { if(!(pos.castleRights&8)) return false; if(!(pos.bb.BK & bb(60))) return false; if(pos.bb.occAll & (bb(57)|bb(58)|bb(59))) return false; if(square_attacked(pos,60,true)||square_attacked(pos,59,true)||square_attacked(pos,58,true)) return false; return true;} } }

void generate_pseudo_moves(const Position& pos, std::vector<Move>& out){ out.clear(); bool white= pos.sideToMove==0; uint64_t occOwn = white? pos.bb.occWhite : pos.bb.occBlack; uint64_t occEnemy = white? pos.bb.occBlack : pos.bb.occWhite; uint64_t occAll = pos.bb.occAll;
    auto slide=[&](uint64_t pieces,bool bishop){ while(pieces){ int from=lsb_index(pieces); pieces &= pieces-1; uint64_t att=(bishop? bishop_attacks(from,occAll):rook_attacks(from,occAll)) & ~occOwn; while(att){ int to=lsb_index(att); bool cap= occEnemy & bb(to); add_move(out,from,to,cap); att &= att-1; } } };
    uint64_t pawns = white? pos.bb.WP : pos.bb.BP; while(pawns){ int from=lsb_index(pawns); pawns &= pawns-1; int r=rank_of(from); int dir= white? +8 : -8; int one= from+dir; if(one>=0 && one<64 && !(occAll & bb(one))){ if((white && r==6)||(!white && r==1)) add_move(out,from,one,false,'q'); else add_move(out,from,one); int startRank= white?1:6; int two= from+2*dir; if(r==startRank && !(occAll & bb(two))) add_move(out,from,two); } int f=file_of(from); auto capTry=[&](int to){ if(to>=0&&to<64){ if(occEnemy & bb(to)){ int tr=rank_of(to); if((white&&tr==7)||(!white&&tr==0)) add_move(out,from,to,true,'q'); else add_move(out,from,to,true);} } }; if(white){ if(f>0) capTry(from+7); if(f<7) capTry(from+9);} else { if(f>0) capTry(from-9); if(f<7) capTry(from-7);} }
    uint64_t knights = white? pos.bb.WN : pos.bb.BN; while(knights){ int from=lsb_index(knights); knights &= knights-1; uint64_t att = knightMask[from] & ~occOwn; while(att){ int to=lsb_index(att); bool cap=occEnemy & bb(to); add_move(out,from,to,cap); att &= att-1; } }
    slide( white? pos.bb.WB : pos.bb.BB, true );
    slide( white? pos.bb.WR : pos.bb.BR, false );
    uint64_t queens = white? pos.bb.WQ : pos.bb.BQ; while(queens){ int from=lsb_index(queens); queens &= queens-1; uint64_t att=(bishop_attacks(from,occAll)|rook_attacks(from,occAll)) & ~occOwn; while(att){ int to=lsb_index(att); bool cap=occEnemy & bb(to); add_move(out,from,to,cap); att &= att-1; } }
    int kingSq = lsb_index( white? pos.bb.WK : pos.bb.BK ); uint64_t kAtt = kingMask[kingSq] & ~occOwn; while(kAtt){ int to=lsb_index(kAtt); bool cap=occEnemy & bb(to); add_move(out,kingSq,to,cap); kAtt &= kAtt-1; }
    if(can_castle(pos,white,true)) add_move(out,kingSq, white?6:62, false,0,true); // short castle (O-O)
    if(can_castle(pos,white,false)) add_move(out,kingSq, white?2:58, false,0,true); // long castle (O-O-O)
}

void filter_legal(const Position& pos, const std::vector<Move>& pseudo, std::vector<Move>& legal){
    legal.clear();
    Position tmp;
    bool white = pos.sideToMove == 0;
    uint64_t oppKing = white ? pos.bb.BK : pos.bb.WK;
    int ownKingSq = lsb_index(white ? pos.bb.WK : pos.bb.BK);
    if (ownKingSq < 0) return; // corrupted position safeguard
    for (const auto& m : pseudo){
        // Disallow capturing king square outright
        if ((oppKing & (1ULL << m.to)) != 0ULL) continue;
        apply_move(pos, m, tmp);
        int newKingSq = lsb_index(white ? tmp.bb.WK : tmp.bb.BK);
        if (newKingSq < 0) continue;
        if (!square_attacked(tmp, newKingSq, !white))
            legal.push_back(m);
    }
}

int negamax(Position& pos, int depth, int alpha, int beta, std::vector<Move>& pv){ if(depth==0) return evaluate(pos)*(pos.sideToMove==0?1:-1); std::vector<Move> pseudo; generate_pseudo_moves(pos,pseudo); std::vector<Move> legal; filter_legal(pos,pseudo,legal); if(legal.empty()) return evaluate(pos)*(pos.sideToMove==0?1:-1); int best=-std::numeric_limits<int>::max(); Move bestM; for(auto &m: legal){ Position next; apply_move(pos,m,next); int score = -negamax(next,depth-1,-beta,-alpha,pv); if(score>best){ best=score; bestM=m; } if(score>alpha) alpha=score; if(alpha>=beta) break; } pv.clear(); pv.push_back(bestM); return best; }

std::string move_to_uci(const Move& m){ std::string s; s.push_back(char('a'+file_of(m.from))); s.push_back(char('1'+rank_of(m.from))); s.push_back(char('a'+file_of(m.to))); s.push_back(char('1'+rank_of(m.to))); if(m.promo) s.push_back(std::tolower((unsigned char)m.promo)); return s; }

std::string choose_move(const std::string& fen, int depth){ Position p; if(!parse_fen(fen,p)) return std::string(); std::vector<Move> pseudo; generate_pseudo_moves(p,pseudo); std::vector<Move> legal; filter_legal(p,pseudo,legal); if(legal.empty()) return std::string(); int alpha=-100000,beta=100000; int best=-100000; Move bestM; std::vector<Move> pv; for(auto &m: legal){ Position next; apply_move(p,m,next); int score = -negamax(next, depth-1, -beta, -alpha, pv); if(score>best){ best=score; bestM=m; } if(score>alpha) alpha=score; } return move_to_uci(bestM); }

std::vector<std::string> legal_moves_uci(const std::string& fen){ Position p; std::vector<std::string> out; if(!parse_fen(fen,p)) return out; std::vector<Move> pseudo; generate_pseudo_moves(p,pseudo); std::vector<Move> legal; filter_legal(p,pseudo,legal); for(auto &m: legal) out.push_back(move_to_uci(m)); return out; }

uint64_t perft(Position& pos, int depth){ if(depth==0) return 1ULL; std::vector<Move> pseudo; generate_pseudo_moves(pos,pseudo); std::vector<Move> legal; filter_legal(pos,pseudo,legal); if(depth==1) return (uint64_t)legal.size(); uint64_t nodes=0; for(auto &m: legal){ Position next; apply_move(pos,m,next); nodes += perft(next, depth-1); } return nodes; }

} // namespace engine

extern "C" {
const char* engine_choose(const char* fen,int depth){ static std::string buf; buf = engine::choose_move(fen, depth); return buf.c_str(); }
void engine_set_avx2(int){ }
unsigned long long engine_perft(const char* fen,int depth){ engine::Position p; if(!engine::parse_fen(fen,p)) return 0ULL; return engine::perft(p, depth); }
const char* engine_legal_moves(const char* fen){ static std::string joined; auto v=engine::legal_moves_uci(fen); joined.clear(); for(size_t i=0;i<v.size();++i){ if(i) joined.push_back(' '); joined += v[i]; } return joined.c_str(); }
}

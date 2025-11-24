#include "ChessEngine1.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace engine
{

std::array< ChessEngine1::U64, 64 > ChessEngine1::pawnAttW{};
std::array< ChessEngine1::U64, 64 > ChessEngine1::pawnAttB{};
std::array< ChessEngine1::U64, 64 > ChessEngine1::knightMask{};
std::array< ChessEngine1::U64, 64 > ChessEngine1::kingMask{};
bool ChessEngine1::masksInit = false;

// Public overrides
std::string ChessEngine1::choose_move( const std::string& fen, int depth )
{
    return choose_move_internal( fen, depth );
}
std::vector< std::pair< std::string, int > > ChessEngine1::root_search_scores( const std::string& fen, int depth )
{
    return root_scores_internal( fen, depth );
}
std::vector< std::string > ChessEngine1::legal_moves_uci( const std::string& fen )
{
    return legal_moves_internal( fen );
}
std::string ChessEngine1::apply_move( const std::string& fen, const std::string& uci )
{
    Position p;
    if ( !parse_fen( fen, p ) )
        return {};
    if ( uci.size() < 4 )
        return {};
    std::vector< Move > pseudo;
    generate_pseudo_moves( p, pseudo );
    std::vector< Move > legal;
    filter_legal( p, pseudo, legal );
    Move chosen{};
    bool found = false;
    for ( auto& m : legal )
    {
        std::string mv = move_to_uci( m );
        if ( mv == uci )
        {
            chosen = m;
            found = true;
            break;
        }
    }
    if ( !found )
        return {};
    Position out;
    apply_move( p, chosen, out );
    return build_fen( out );
}

// Init attack masks
void ChessEngine1::init_masks()
{
    if ( masksInit )
        return;
    for ( int sq = 0; sq < 64; ++sq )
    {
        int r = rank_of( sq ), f = file_of( sq );
        if ( r < 7 )
        {
            if ( f > 0 )
                pawnAttW[ sq ] |= bb( sq + 8 - 1 );
            if ( f < 7 )
                pawnAttW[ sq ] |= bb( sq + 8 + 1 );
        }
        if ( r > 0 )
        {
            if ( f > 0 )
                pawnAttB[ sq ] |= bb( sq - 8 - 1 );
            if ( f < 7 )
                pawnAttB[ sq ] |= bb( sq - 8 + 1 );
        }
        const int kd[ 8 ][ 2 ] = { { -2, -1 }, { -2, 1 }, { -1, -2 }, { -1, 2 }, { 1, -2 }, { 1, 2 }, { 2, -1 }, { 2, 1 } };
        for ( auto& d : kd )
        {
            int rr = r + d[ 0 ], ff = f + d[ 1 ];
            if ( rr >= 0 && rr < 8 && ff >= 0 && ff < 8 )
                knightMask[ sq ] |= bb( rr * 8 + ff );
        }
        for ( int dr = -1; dr <= 1; ++dr )
            for ( int dc = -1; dc <= 1; ++dc )
            {
                if ( !dr && !dc )
                    continue;
                int rr = r + dr, ff = f + dc;
                if ( rr >= 0 && rr < 8 && ff >= 0 && ff < 8 )
                    kingMask[ sq ] |= bb( rr * 8 + ff );
            }
    }
    masksInit = true;
}

ChessEngine1::U64 ChessEngine1::rook_attacks( int sq, U64 occ )
{
    U64 a = 0;
    int r = rank_of( sq ), f = file_of( sq );
    for ( int rr = r + 1; rr < 8; ++rr )
    {
        int s = rr * 8 + f;
        a |= bb( s );
        if ( occ & bb( s ) )
            break;
    }
    for ( int rr = r - 1; rr >= 0; --rr )
    {
        int s = rr * 8 + f;
        a |= bb( s );
        if ( occ & bb( s ) )
            break;
    }
    for ( int ff = f + 1; ff < 8; ++ff )
    {
        int s = r * 8 + ff;
        a |= bb( s );
        if ( occ & bb( s ) )
            break;
    }
    for ( int ff = f - 1; ff >= 0; --ff )
    {
        int s = r * 8 + ff;
        a |= bb( s );
        if ( occ & bb( s ) )
            break;
    }
    return a;
}
ChessEngine1::U64 ChessEngine1::bishop_attacks( int sq, U64 occ )
{
    U64 a = 0;
    int r = rank_of( sq ), f = file_of( sq );
    for ( int rr = r + 1, ff = f + 1; rr < 8 && ff < 8; ++rr, ++ff )
    {
        int s = rr * 8 + ff;
        a |= bb( s );
        if ( occ & bb( s ) )
            break;
    }
    for ( int rr = r + 1, ff = f - 1; rr < 8 && ff >= 0; ++rr, --ff )
    {
        int s = rr * 8 + ff;
        a |= bb( s );
        if ( occ & bb( s ) )
            break;
    }
    for ( int rr = r - 1, ff = f + 1; rr >= 0 && ff < 8; --rr, ++ff )
    {
        int s = rr * 8 + ff;
        a |= bb( s );
        if ( occ & bb( s ) )
            break;
    }
    for ( int rr = r - 1, ff = f - 1; rr >= 0 && ff >= 0; --rr, --ff )
    {
        int s = rr * 8 + ff;
        a |= bb( s );
        if ( occ & bb( s ) )
            break;
    }
    return a;
}

bool ChessEngine1::parse_fen( const std::string& fen, Position& out )
{
    init_masks();
    out = Position();
    std::array< char, 64 > sqArr;
    sqArr.fill( '.' );
    std::vector< std::string > tok;
    tok.reserve( 6 );
    std::string cur;
    for ( char c : fen )
    {
        if ( c == ' ' )
        {
            if ( !cur.empty() )
            {
                tok.push_back( cur );
                cur.clear();
            }
        }
        else
            cur.push_back( c );
    }
    if ( !cur.empty() )
        tok.push_back( cur );
    if ( tok.size() < 2 )
        return false;
    int idx = 56;
    for ( char c : tok[ 0 ] )
    {
        if ( c == '/' )
        {
            idx -= 16;
            continue;
        }
        if ( c >= '1' && c <= '8' )
        {
            idx += ( c - '0' );
            continue;
        }
        if ( idx < 0 || idx >= 64 )
            return false;
        sqArr[ idx++ ] = c;
    }
    auto add = [ & ]( char pc, int s )
    { U64 b=bb(s); switch(pc){ case 'P': out.bb.WP|=b; break; case 'N': out.bb.WN|=b; break; case 'B': out.bb.WB|=b; break; case 'R': out.bb.WR|=b; break; case 'Q': out.bb.WQ|=b; break; case 'K': out.bb.WK|=b; break; case 'p': out.bb.BP|=b; break; case 'n': out.bb.BN|=b; break; case 'b': out.bb.BB|=b; break; case 'r': out.bb.BR|=b; break; case 'q': out.bb.BQ|=b; break; case 'k': out.bb.BK|=b; break; } };
    for ( int s = 0; s < 64; ++s )
        if ( sqArr[ s ] != '.' )
            add( sqArr[ s ], s );
    out.bb.occWhite = out.bb.WP | out.bb.WN | out.bb.WB | out.bb.WR | out.bb.WQ | out.bb.WK;
    out.bb.occBlack = out.bb.BP | out.bb.BN | out.bb.BB | out.bb.BR | out.bb.BQ | out.bb.BK;
    out.bb.occAll = out.bb.occWhite | out.bb.occBlack;
    out.sideToMove = ( tok[ 1 ] == "w" ? 0 : 1 );
    out.castleRights = 0;
    if ( tok.size() >= 3 )
    {
        std::string c = tok[ 2 ];
        if ( c.find( 'K' ) != std::string::npos )
            out.castleRights |= 1;
        if ( c.find( 'Q' ) != std::string::npos )
            out.castleRights |= 2;
        if ( c.find( 'k' ) != std::string::npos )
            out.castleRights |= 4;
        if ( c.find( 'q' ) != std::string::npos )
            out.castleRights |= 8;
    }
    out.epSquare = -1;
    if ( tok.size() >= 4 && tok[ 3 ] != "-" )
    {
        std::string ep = tok[ 3 ];
        if ( ep.size() == 2 )
        {
            int f = ep[ 0 ] - 'a', r = ep[ 1 ] - '1';
            if ( f >= 0 && f < 8 && r >= 0 && r < 8 )
                out.epSquare = r * 8 + f;
        }
    }
    if ( tok.size() >= 5 )
        out.halfmoveClock = std::atoi( tok[ 4 ].c_str() );
    if ( tok.size() >= 6 )
        out.fullmoveNumber = std::atoi( tok[ 5 ].c_str() );
    // TODO: ensure both kings exist; fail parse if missing
    if ( lsb_index( out.bb.WK ) < 0 || lsb_index( out.bb.BK ) < 0 )
        return false;
    return true;
}

ChessEngine1::U64 ChessEngine1::attackers_to( const Position& pos, int sq, int byWhite )
{
    U64 attackers = 0ULL;
    U64 occ = pos.bb.occAll;
    int f = file_of( sq ), r = rank_of( sq );
    if ( byWhite )
    {
        if ( r > 0 )
        {
            if ( f < 7 )
            {
                int p = sq - 7;
                if ( p >= 0 && ( pos.bb.WP & bb( p ) ) )
                    attackers |= bb( p );
            }
            if ( f > 0 )
            {
                int p = sq - 9;
                if ( p >= 0 && ( pos.bb.WP & bb( p ) ) )
                    attackers |= bb( p );
            }
        }
    }
    else
    {
        if ( r < 7 )
        {
            if ( f > 0 )
            {
                int p = sq + 7;
                if ( p < 64 && ( pos.bb.BP & bb( p ) ) )
                    attackers |= bb( p );
            }
            if ( f < 7 )
            {
                int p = sq + 9;
                if ( p < 64 && ( pos.bb.BP & bb( p ) ) )
                    attackers |= bb( p );
            }
        }
    }
    attackers |= knightMask[ sq ] & ( byWhite ? pos.bb.WN : pos.bb.BN );
    attackers |= kingMask[ sq ] & ( byWhite ? pos.bb.WK : pos.bb.BK );
    auto ray_scan = [ & ]( const int* df, const int* dr, int c, U64 mask )
    { for(int d=0; d<c; ++d){ int cf=f, cr=r; while(true){ cf+=df[d]; cr+=dr[d]; if(cf<0||cf>7||cr<0||cr>7) break; int s=cr*8+cf; U64 b=bb(s); if(occ & b){ if(mask & b) attackers |= b; break; } } } };
    const int bdf[ 4 ] = { 1, 1, -1, -1 };
    const int bdr[ 4 ] = { 1, -1, 1, -1 };
    U64 bq = byWhite ? ( pos.bb.WB | pos.bb.WQ ) : ( pos.bb.BB | pos.bb.BQ );
    ray_scan( bdf, bdr, 4, bq );
    const int rdf[ 4 ] = { 1, -1, 0, 0 };
    const int rdr[ 4 ] = { 0, 0, 1, -1 };
    U64 rq = byWhite ? ( pos.bb.WR | pos.bb.WQ ) : ( pos.bb.BR | pos.bb.BQ );
    ray_scan( rdf, rdr, 4, rq );
    return attackers;
}
bool ChessEngine1::square_attacked( const Position& pos, int sq, int byWhite )
{
    return attackers_to( pos, sq, byWhite ) != 0ULL;
}

void ChessEngine1::apply_move( const Position& pos, const Move& m, Position& out )
{
    out = pos;
    U64 fromB = bb( m.from ), toB = bb( m.to );
    bool white = ( pos.sideToMove == 0 );
    if ( m.isCapture )
    {
        if ( white )
        {
            out.bb.BP &= ~toB;
            out.bb.BN &= ~toB;
            out.bb.BB &= ~toB;
            out.bb.BR &= ~toB;
            out.bb.BQ &= ~toB;
            out.bb.BK &= ~toB;
        }
        else
        {
            out.bb.WP &= ~toB;
            out.bb.WN &= ~toB;
            out.bb.WB &= ~toB;
            out.bb.WR &= ~toB;
            out.bb.WQ &= ~toB;
            out.bb.WK &= ~toB;
        }
    }
    auto movePiece = [ & ]( U64& bbt )
    { if(bbt & fromB){ bbt ^= fromB; bbt |= toB; } };
    movePiece( white ? out.bb.WP : out.bb.BP );
    movePiece( white ? out.bb.WN : out.bb.BN );
    movePiece( white ? out.bb.WB : out.bb.BB );
    movePiece( white ? out.bb.WR : out.bb.BR );
    movePiece( white ? out.bb.WQ : out.bb.BQ );
    movePiece( white ? out.bb.WK : out.bb.BK );
    if ( m.isCastle )
    {
        if ( white )
        {
            if ( m.to == 6 )
            {
                out.bb.WR &= ~bb( 7 );
                out.bb.WR |= bb( 5 );
            }
            else if ( m.to == 2 )
            {
                out.bb.WR &= ~bb( 0 );
                out.bb.WR |= bb( 3 );
            }
        }
        else
        {
            if ( m.to == 62 )
            {
                out.bb.BR &= ~bb( 63 );
                out.bb.BR |= bb( 61 );
            }
            else if ( m.to == 58 )
            {
                out.bb.BR &= ~bb( 56 );
                out.bb.BR |= bb( 59 );
            }
        }
    }
    out.bb.occWhite = out.bb.WP | out.bb.WN | out.bb.WB | out.bb.WR | out.bb.WQ | out.bb.WK;
    out.bb.occBlack = out.bb.BP | out.bb.BN | out.bb.BB | out.bb.BR | out.bb.BQ | out.bb.BK;
    out.bb.occAll = out.bb.occWhite | out.bb.occBlack;
    if ( !white )
        out.fullmoveNumber++;
    out.sideToMove = white ? 1 : 0;
    out.epSquare = -1;
    auto strip = [ & ]( int mask )
    { out.castleRights &= ~mask; };
    if ( m.from == 4 )
        strip( 1 | 2 );
    if ( m.from == 60 )
        strip( 4 | 8 );
    if ( m.from == 0 || m.to == 0 )
        strip( 2 );
    if ( m.from == 7 || m.to == 7 )
        strip( 1 );
    if ( m.from == 56 || m.to == 56 )
        strip( 8 );
    if ( m.from == 63 || m.to == 63 )
        strip( 4 );
}

int ChessEngine1::evaluate_material( const Position& pos )
{
    static const int pieceValue[ 6 ] = { 100, 320, 330, 500, 900, 0 };
    int w = 0, b = 0;
    auto add = [ & ]( U64 bb, int val, bool white )
    { while(bb){ bb &= bb-1; if(white) w+=val; else b+=val; } };
    add( pos.bb.WP, pieceValue[ 0 ], true );
    add( pos.bb.BP, pieceValue[ 0 ], false );
    add( pos.bb.WN, pieceValue[ 1 ], true );
    add( pos.bb.BN, pieceValue[ 1 ], false );
    add( pos.bb.WB, pieceValue[ 2 ], true );
    add( pos.bb.BB, pieceValue[ 2 ], false );
    add( pos.bb.WR, pieceValue[ 3 ], true );
    add( pos.bb.BR, pieceValue[ 3 ], false );
    add( pos.bb.WQ, pieceValue[ 4 ], true );
    add( pos.bb.BQ, pieceValue[ 4 ], false );
    return w - b;
}
int ChessEngine1::evaluate( const Position& pos )
{
    int mat = evaluate_material( pos );
    return ( pos.sideToMove == 0 ) ? mat : -mat;
}

ChessEngine1::U64 ChessEngine1::can_castle( const Position& pos, bool white, bool kingside )
{
    return 0;
}

void ChessEngine1::generate_pseudo_moves( const Position& pos, std::vector< Move >& out )
{
    out.clear();
    bool white = pos.sideToMove == 0;
    U64 occOwn = white ? pos.bb.occWhite : pos.bb.occBlack;
    U64 occEnemy = white ? pos.bb.occBlack : pos.bb.occWhite;
    U64 occAll = pos.bb.occAll;
    auto add = [ & ]( int f, int t, bool cap = false, int promo = 0, bool castle = false )
    { Move m; m.from=f; m.to=t; m.isCapture=cap; m.promo=promo; m.isCastle=castle; out.push_back(m); };
    auto slide = [ & ]( U64 pieces, bool bishop )
    { while(pieces){ int from=lsb_index(pieces); if(from<0) break; pieces &= pieces-1; U64 att=(bishop? bishop_attacks(from,occAll):rook_attacks(from,occAll)) & ~occOwn; while(att){ int to=lsb_index(att); if(to<0) break; bool cap= occEnemy & bb(to); add(from,to,cap); att &= att-1; } } };
    U64 pawns = white ? pos.bb.WP : pos.bb.BP;
    while ( pawns )
    {
        int from = lsb_index( pawns );
        if ( from < 0 )
            break;
        pawns &= pawns - 1;
        int r = rank_of( from );
        int dir = white ? +8 : -8;
        int one = from + dir;
        if ( one >= 0 && one < 64 && !( occAll & bb( one ) ) )
        {
            if ( ( white && r == 6 ) || ( !white && r == 1 ) )
                add( from, one, false, 'q' );
            else
                add( from, one );
            int startRank = white ? 1 : 6;
            int two = from + 2 * dir;
            if ( r == startRank && !( occAll & bb( two ) ) )
                add( from, two );
        }
        int f = file_of( from );
        auto capTry = [ & ]( int to )
        { if(to>=0&&to<64){ if(occEnemy & bb(to)){ int tr=rank_of(to); if((white&&tr==7)||(!white&&tr==0)) add(from,to,true,'q'); else add(from,to,true);} } };
        if ( white )
        {
            if ( f > 0 )
                capTry( from + 7 );
            if ( f < 7 )
                capTry( from + 9 );
        }
        else
        {
            if ( f > 0 )
                capTry( from - 9 );
            if ( f < 7 )
                capTry( from - 7 );
        }
    }
    U64 knights = white ? pos.bb.WN : pos.bb.BN;
    while ( knights )
    {
        int from = lsb_index( knights );
        if ( from < 0 )
            break;
        knights &= knights - 1;
        U64 att = knightMask[ from ] & ~occOwn;
        while ( att )
        {
            int to = lsb_index( att );
            if ( to < 0 )
                break;
            bool cap = occEnemy & bb( to );
            add( from, to, cap );
            att &= att - 1;
        }
    }
    slide( white ? pos.bb.WB : pos.bb.BB, true );
    slide( white ? pos.bb.WR : pos.bb.BR, false );
    U64 queens = white ? pos.bb.WQ : pos.bb.BQ;
    while ( queens )
    {
        int from = lsb_index( queens );
        if ( from < 0 )
            break;
        queens &= queens - 1;
        U64 att = ( bishop_attacks( from, occAll ) | rook_attacks( from, occAll ) ) & ~occOwn;
        while ( att )
        {
            int to = lsb_index( att );
            if ( to < 0 )
                break;
            bool cap = occEnemy & bb( to );
            add( from, to, cap );
            att &= att - 1;
        }
    }
    int kingSq = lsb_index( white ? pos.bb.WK : pos.bb.BK );
    if ( kingSq >= 0 )
    {
        U64 kAtt = kingMask[ kingSq ] & ~occOwn;
        while ( kAtt )
        {
            int to = lsb_index( kAtt );
            if ( to < 0 )
                break;
            bool cap = occEnemy & bb( to );
            add( kingSq, to, cap );
            kAtt &= kAtt - 1;
        }
    }
    auto canCastle = [ & ]( bool kside )
    { if(white){ if(kside){ if(!(pos.castleRights&1)) return false; if(!(pos.bb.WK & bb(4))) return false; if(pos.bb.occAll & (bb(5)|bb(6))) return false; if(square_attacked(pos,4,false)||square_attacked(pos,5,false)||square_attacked(pos,6,false)) return false; return true;} else { if(!(pos.castleRights&2)) return false; if(!(pos.bb.WK & bb(4))) return false; if(pos.bb.occAll & (bb(1)|bb(2)|bb(3))) return false; if(square_attacked(pos,4,false)||square_attacked(pos,3,false)||square_attacked(pos,2,false)) return false; return true; }} else { if(kside){ if(!(pos.castleRights&4)) return false; if(!(pos.bb.BK & bb(60))) return false; if(pos.bb.occAll & (bb(61)|bb(62))) return false; if(square_attacked(pos,60,true)||square_attacked(pos,61,true)||square_attacked(pos,62,true)) return false; return true;} else { if(!(pos.castleRights&8)) return false; if(!(pos.bb.BK & bb(60))) return false; if(pos.bb.occAll & (bb(57)|bb(58)|bb(59))) return false; if(square_attacked(pos,60,true)||square_attacked(pos,59,true)||square_attacked(pos,58,true)) return false; return true; }} };
    if ( canCastle( true ) )
        add( kingSq, white ? 6 : 62, false, 0, true );
    if ( canCastle( false ) )
        add( kingSq, white ? 2 : 58, false, 0, true );
}

int ChessEngine1::negamax( Position& pos, int depth, int alpha, int beta, std::vector< Move >& pv )
{
    if ( depth == 0 )
        return evaluate( pos );
    std::vector< Move > pseudo;
    generate_pseudo_moves( pos, pseudo );
    std::vector< Move > legal;
    filter_legal( pos, pseudo, legal );
    if ( legal.empty() )
        return evaluate( pos );
    int best = -10000000;
    Move bestM{};
    for ( auto& m : legal )
    {
        Position next;
        apply_move( pos, m, next );
        int score = -negamax( next, depth - 1, -beta, -alpha, pv );
        if ( score > best )
        {
            best = score;
            bestM = m;
        }
        if ( score > alpha )
            alpha = score;
        if ( alpha >= beta )
            break;
    }
    pv.clear();
    pv.push_back( bestM );
    return best;
}

std::string ChessEngine1::move_to_uci( const Move& m )
{
    std::string s;
    s.push_back( char( 'a' + file_of( m.from ) ) );
    s.push_back( char( '1' + rank_of( m.from ) ) );
    s.push_back( char( 'a' + file_of( m.to ) ) );
    s.push_back( char( '1' + rank_of( m.to ) ) );
    if ( m.promo )
        s.push_back( std::tolower( ( unsigned char )m.promo ) );
    return s;
}

std::string ChessEngine1::choose_move_internal( const std::string& fen, int depth )
{
    Position p;
    if ( !parse_fen( fen, p ) )
        return {};
    std::vector< Move > pseudo;
    generate_pseudo_moves( p, pseudo );
    std::vector< Move > legal;
    filter_legal( p, pseudo, legal );
    if ( legal.empty() )
        return {};
    int alpha = -1000000, beta = 1000000;
    int best = -1000000;
    Move bestM{};
    std::vector< Move > pv;
    for ( auto& m : legal )
    {
        Position next;
        apply_move( p, m, next );
        int score = -negamax( next, depth - 1, -beta, -alpha, pv );
        if ( score > best )
        {
            best = score;
            bestM = m;
        }
        if ( score > alpha )
            alpha = score;
    }
    return move_to_uci( bestM );
}
std::vector< std::pair< std::string, int > > ChessEngine1::root_scores_internal( const std::string& fen, int depth )
{
    Position p;
    std::vector< std::pair< std::string, int > > out;
    if ( !parse_fen( fen, p ) )
        return out;
    std::vector< Move > pseudo;
    generate_pseudo_moves( p, pseudo );
    std::vector< Move > legal;
    filter_legal( p, pseudo, legal );
    if ( legal.empty() )
        return out;
    int alpha = -1000000, beta = 1000000;
    std::vector< Move > pv;
    for ( auto& m : legal )
    {
        Position next;
        apply_move( p, m, next );
        int score = -negamax( next, depth - 1, -beta, -alpha, pv );
        if ( score > alpha )
            alpha = score;
        out.emplace_back( move_to_uci( m ), score );
    }
    return out;
}
std::vector< std::string > ChessEngine1::legal_moves_internal( const std::string& fen )
{
    Position p;
    std::vector< std::string > out;
    if ( !parse_fen( fen, p ) )
        return out;
    std::vector< Move > pseudo;
    generate_pseudo_moves( p, pseudo );
    std::vector< Move > legal;
    filter_legal( p, pseudo, legal );
    for ( auto& m : legal )
        out.push_back( move_to_uci( m ) );
    return out;
}

std::string ChessEngine1::build_fen( const Position& p )
{
    std::string board;
    for ( int rank = 7; rank >= 0; --rank )
    {
        int empty = 0;
        for ( int file = 0; file < 8; ++file )
        {
            int idx = rank * 8 + file;
            U64 b = bb( idx );
            char pc = '.';
            if ( p.bb.WP & b )
                pc = 'P';
            else if ( p.bb.WN & b )
                pc = 'N';
            else if ( p.bb.WB & b )
                pc = 'B';
            else if ( p.bb.WR & b )
                pc = 'R';
            else if ( p.bb.WQ & b )
                pc = 'Q';
            else if ( p.bb.WK & b )
                pc = 'K';
            else if ( p.bb.BP & b )
                pc = 'p';
            else if ( p.bb.BN & b )
                pc = 'n';
            else if ( p.bb.BB & b )
                pc = 'b';
            else if ( p.bb.BR & b )
                pc = 'r';
            else if ( p.bb.BQ & b )
                pc = 'q';
            else if ( p.bb.BK & b )
                pc = 'k';
            if ( pc == '.' )
            {
                ++empty;
            }
            else
            {
                if ( empty )
                {
                    board.push_back( char( '0' + empty ) );
                    empty = 0;
                }
                board.push_back( pc );
            }
        }
        if ( empty )
            board.push_back( char( '0' + empty ) );
        if ( rank )
            board.push_back( '/' );
    }
    std::string cast = "";
    if ( p.castleRights & 1 )
        cast += "K";
    if ( p.castleRights & 2 )
        cast += "Q";
    if ( p.castleRights & 4 )
        cast += "k";
    if ( p.castleRights & 8 )
        cast += "q";
    if ( cast.empty() )
        cast = "-";
    std::string ep = "-";
    if ( p.epSquare >= 0 )
    {
        int f = file_of( p.epSquare );
        int r = rank_of( p.epSquare );
        ep = std::string( 1, char( 'a' + f ) ) + char( '1' + r );
    }
    return board + ( p.sideToMove == 0 ? " w " : " b " ) + cast + " " + ep + " " + std::to_string( p.halfmoveClock ) + " " + std::to_string( p.fullmoveNumber );
}

void ChessEngine1::filter_legal( const Position& pos, const std::vector< Move >& pseudo, std::vector< Move >& legal )
{
    legal.clear();
    Position tmp;
    bool white = pos.sideToMove == 0;
    U64 oppKing = white ? pos.bb.BK : pos.bb.WK;
    int ownKingSq = lsb_index( white ? pos.bb.WK : pos.bb.BK );
    if ( ownKingSq < 0 )
        return;
    for ( const auto& m : pseudo )
    {
        if ( oppKing & bb( m.to ) )
            continue;
        apply_move( pos, m, tmp );
        int newKingSq = lsb_index( white ? tmp.bb.WK : tmp.bb.BK );
        if ( newKingSq < 0 )
            continue;
        if ( !square_attacked( tmp, newKingSq, !white ) )
            legal.push_back( m );
    }
}

} // namespace engine

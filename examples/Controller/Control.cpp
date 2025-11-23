// This file contains the implementation of the controller for the chess game.

#include "Control.hpp"
#include <cstring>
#include <sstream>
#include <algorithm>

namespace controller {

static void parse_board_portion(const std::string& fen, char out[64]){
    for(int i=0;i<64;++i) out[i]='.';
    const char* p=fen.c_str(); int sq=56; // start at rank8 file a
    while(*p && *p!=' '){
        if(*p=='/') { sq -= 16; ++p; continue; }
        if(*p>='1' && *p<='8'){ sq += (*p - '0'); ++p; continue; }
        if(sq>=0 && sq<64) out[sq++] = *p;
        ++p;
    }
}

GameController::GameController(IChessEngine& engine) : eng(&engine){
    reset();
}

void GameController::set_engine(IChessEngine& engine){
    eng = &engine; // keep current position, no reset
}

void GameController::reset(){
    const char* startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    currentFEN = startFEN;
    parse_board_portion(currentFEN, boardSquares);
    whiteToMove = true;
    fullmoveNumber = 1;
    fenHistory.clear();
    fenHistory.push_back(currentFEN);
    pgnString.clear();
}

bool GameController::load_fen(const std::string& fen){
    // Basic token parsing similar to main.cpp
    std::istringstream iss(fen); std::vector<std::string> tokens; std::string tk; while(iss >> tk) tokens.push_back(tk);
    if(tokens.size() < 2) return false;
    parse_board_portion(fen, boardSquares);
    whiteToMove = (tokens[1] == "w");
    // Find last numeric token for fullmove
    int newFull = 1;
    for(int i=(int)tokens.size()-1;i>=0;--i){ bool digits = !tokens[i].empty() && std::all_of(tokens[i].begin(), tokens[i].end(), [](char c){ return std::isdigit((unsigned char)c); }); if(digits){ newFull = std::atoi(tokens[i].c_str()); break; } }
    fullmoveNumber = newFull;
    currentFEN = fen;
    fenHistory.clear(); fenHistory.push_back(currentFEN);
    pgnString.clear();
    return true;
}

bool GameController::undo(){
    if(fenHistory.size() < 2) return false;
    fenHistory.pop_back();
    std::string prev = fenHistory.back();
    return load_fen(prev);
}

std::vector<std::string> GameController::legal_moves(){ return eng? eng->legal_moves(currentFEN): std::vector<std::string>(); }

std::string GameController::engine_move(int depth){
    if(!whiteToMove || !eng) return std::string(); // engine only plays white per current design
    std::string mv = eng->choose_move(currentFEN, depth);
    if(!mv.empty()){
        std::string san = build_san(mv);
        if(!san.empty()){ if(!pgnString.empty()) pgnString += ' '; pgnString += std::to_string(fullmoveNumber) + '.' + san; }
        apply_uci_move_to_board(mv);
        whiteToMove = false; // switch to black
        // fullmove increments after black move, so not here
        push_fen();
    }
    return mv;
}

bool GameController::apply_human_move(const std::string& uci){
    if(whiteToMove || !eng) return false; // human is black
    // Validate move is in legal list
    auto moves = eng->legal_moves(currentFEN);
    std::string found;
    for(auto &m: moves){ if(m.rfind(uci,0)==0){ found=m; break; } }
    if(found.empty()) return false;
    std::string san = build_san(found);
    if(!san.empty()){ if(!pgnString.empty()) pgnString += ' '; pgnString += san; }
    apply_uci_move_to_board(found);
    whiteToMove = true;
    fullmoveNumber++; // after black move
    push_fen();
    return true;
}

void GameController::parse_board_from_fen(const std::string& fen){ parse_board_portion(fen, boardSquares); }

std::string GameController::build_fen() const{
    std::string s; for(int rank=7; rank>=0; --rank){ int empty=0; for(int file=0; file<8; ++file){ int idx=rank*8+file; char pc=boardSquares[idx]; if(pc=='.'){ empty++; } else { if(empty){ s.push_back(char('0'+empty)); empty=0;} s.push_back(pc);} } if(empty) s.push_back(char('0'+empty)); if(rank) s.push_back('/'); }
    s += whiteToMove? " w " : " b "; s += "KQkq - 0 "; s += std::to_string(fullmoveNumber); return s;
}

std::string GameController::index_to_alg(int idx) const{ int f=idx%8; int r=idx/8; return std::string(1,char('a'+f))+char('1'+r); }
int GameController::algebraic_to_index(const char* s) const{ if(!s||s[0]<'a'||s[0]>'h'||s[1]<'1'||s[1]>'8') return -1; int file=s[0]-'a'; int rank=s[1]-'1'; return rank*8+file; }

void GameController::apply_uci_move_to_board(const std::string& uci){ if(uci.size()<4) return; int from=algebraic_to_index(uci.c_str()); int to=algebraic_to_index(uci.c_str()+2); if(from<0||to<0) return; char piece=boardSquares[from]; if(piece=='.') return; boardSquares[from]='.'; if(uci.size()>=5){ char p=uci[4]; if(piece>='A'&&piece<='Z') p=(char)toupper((unsigned char)p); boardSquares[to]=p; } else { boardSquares[to]=piece; }
    if(piece=='K'){ if(uci=="e1g1"){ boardSquares[5]='R'; boardSquares[7]='.'; } else if(uci=="e1c1"){ boardSquares[3]='R'; boardSquares[0]='.'; } }
    else if(piece=='k'){ if(uci=="e8g8"){ boardSquares[61]='r'; boardSquares[63]='.'; } else if(uci=="e8c8"){ boardSquares[59]='r'; boardSquares[56]='.'; } }
}

std::string GameController::build_san(const std::string& uci) const{
    if(uci.size()<4) return std::string(); int from=algebraic_to_index(uci.c_str()); char piece = (from>=0)? boardSquares[from] : '.'; bool isPawn = (piece=='P'||piece=='p'); std::string toSq = uci.substr(2,2); std::string san; if(!isPawn){ san.push_back((char)toupper((unsigned char)piece)); san += toSq; } else { san += toSq; if(uci.size()==5) san.push_back((char)toupper((unsigned char)uci[4])); } return san; }

void GameController::push_fen(){ currentFEN = build_fen(); fenHistory.push_back(currentFEN); }

} // namespace controller

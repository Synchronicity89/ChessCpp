#include "EngineBase.h"
#include <bit>
namespace engine {
    void EngineBase::flipPosition()
    {
        uint64_t new_pieces[12];
        for (int i = 0; i < 6; ++i) {
            new_pieces[i] = byteswap(pieces[i + 6]);
            new_pieces[i + 6] = byteswap(pieces[i]);
        }
        std::copy(std::begin(new_pieces), std::end(new_pieces), std::begin(pieces));
        side_to_move = 1 - side_to_move;
    }

    std::string EngineBase::buildFen(const EngineBase& e)
    {
        auto pieceAt = [&](int sq)->char { uint64_t b = 1ULL << sq; for (int i = 0; i < 6; ++i) { if (e.pieces[i] & b) { return "PNBRQK"[i]; } if (e.pieces[i + 6] & b) { return "pnbrqk"[i]; } } return '.'; }; std::string board; for (int rank = 7; rank >= 0; --rank) { int empty = 0; for (int file = 0; file < 8; ++file) { int idx = rank * 8 + file; char pc = pieceAt(idx); if (pc == '.') { ++empty; } else { if (empty) { board.push_back(char('0' + empty)); empty = 0; } board.push_back(pc); } } if (empty) board.push_back(char('0' + empty)); if (rank) board.push_back('/'); } std::string cast = "KQkq"; if (cast == "KQkq") {} // placeholder always full rights for simplicity
        std::string ep = (e.ep_square >= 0 ? std::string(1, char('a' + (e.ep_square % 8))) + char('1' + (e.ep_square / 8)) : "-"); return board + (e.side_to_move == 0 ? " w " : " b ") + cast + " " + ep + " " + std::to_string(e.halfmove_clock) + " " + std::to_string(e.fullmove_number);
    }

    void EngineBase::loadFEN(const std::string& fen) {
        std::fill(std::begin(pieces), std::end(pieces), 0ULL);
        size_t idx = 0;
        int sq = 56;
        while (fen[idx] != ' ') {
            char c = fen[idx++];
            if (c == '/') { sq -= 16; continue; }
            if (isdigit(c)) { sq += (c - '0'); continue; }
            int color_offset = isupper(c) ? 0 : 6;
            char uc = toupper(c);
            int ptype = (uc == 'P' ? 0 : uc == 'N' ? 1 : uc == 'B' ? 2 : uc == 'R' ? 3 : uc == 'Q' ? 4 : 5);
            pieces[ptype + color_offset] |= (1ULL << sq); sq++;
        }
        idx++;
        side_to_move = (fen[idx++] == 'w' ? 0 : 1); idx += 1;
        // skip castling (simplified, keep existing flags default)
        idx = fen.find(' ', idx) + 1;
        // ep square
        ep_square = (fen[idx] == '-' ? -1 : (fen[idx] - 'a') + (fen[idx + 1] - '1') * 8); idx = fen.find(' ', idx) + 1;
        halfmove_clock = std::stoi(fen.substr(idx, fen.find(' ', idx) - idx)); idx = fen.find(' ', idx) + 1;
        fullmove_number = std::stoi(fen.substr(idx));
        if (side_to_move == 1) flipPosition();
    }
}

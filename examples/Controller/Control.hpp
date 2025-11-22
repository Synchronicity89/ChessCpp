#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace controller {

struct IChessEngine {
    virtual ~IChessEngine() = default;
    virtual std::vector<std::string> legal_moves(const std::string& fen) = 0;
    virtual std::string choose_move(const std::string& fen, int depth) = 0;
};

class GameController {
public:
    explicit GameController(IChessEngine& engine);
    void reset();
    bool load_fen(const std::string& fen); // returns false if parse failed
    bool undo(); // revert to previous position if available
    const std::string& current_fen() const { return currentFEN; }
    bool white_to_move() const { return whiteToMove; }
    int fullmove_number() const { return fullmoveNumber; }
    const std::vector<std::string>& fen_history() const { return fenHistory; }
    const std::string& pgn() const { return pgnString; }

    // Engine interaction
    std::vector<std::string> legal_moves();
    std::string engine_move(int depth); // apply engine chosen move, return UCI or empty
    bool apply_human_move(const std::string& uci); // black move when engine is white

    // Board access
    const char* board() const { return boardSquares; }
    char piece_at(int sq) const { return (sq>=0 && sq<64)? boardSquares[sq] : '.'; }

private:
    IChessEngine& eng;
    char boardSquares[64];
    bool whiteToMove = true;
    int fullmoveNumber = 1;
    std::string currentFEN;
    std::vector<std::string> fenHistory;
    std::string pgnString;

    void parse_board_from_fen(const std::string& fen);
    std::string build_fen() const;
    std::string index_to_alg(int idx) const;
    int algebraic_to_index(const char* s) const;
    void apply_uci_move_to_board(const std::string& uci);
    std::string build_san(const std::string& uci) const;
    void push_fen();
};

} // namespace controller

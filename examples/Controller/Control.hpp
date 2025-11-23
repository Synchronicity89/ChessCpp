#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../chessnative2/EngineBase.h" // use shared abstract base

namespace controller {


    
class GameController {
public:
    explicit GameController(engine::EngineBase& engine);
    void set_engine(engine::EngineBase& engine);
    void reset();
    bool load_fen(const std::string& fen);
    bool undo();
    const std::string& current_fen() const { return currentFEN; }
    bool white_to_move() const { return whiteToMove; }
    int fullmove_number() const { return fullmoveNumber; }
    const std::vector<std::string>& fen_history() const { return fenHistory; }
    const std::string& pgn() const { return pgnString; }

    std::vector<std::string> legal_moves_uci();
    std::vector<std::string> legal_moves() { return legal_moves_uci(); }
    std::string engine_move(int depth);
    bool apply_human_move(const std::string& uci);

    const char* board() const { return boardSquares; }
    char piece_at(int sq) const { return (sq>=0 && sq<64)? boardSquares[sq] : '.'; }

    // Utility static methods for tests
    static std::vector<std::string> splitStringBySpace(const std::string& s);
    static std::string flip_fen(const std::string& fen); // new helper

private:
    engine::EngineBase* eng{};
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

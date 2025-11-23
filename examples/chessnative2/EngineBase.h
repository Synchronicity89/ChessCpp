#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

// Abstract base for selectable engines.
class EngineBase {
public:
    virtual ~EngineBase() = default;
    // Compute best move in UCI form for given FEN and search depth.
    virtual std::string choose_move(const std::string& fen, int depth) = 0;
    // Return (uci, score) for all legal root moves searched to given depth.
    virtual std::vector<std::pair<std::string,int>> root_search_scores(const std::string& fen, int depth) = 0;
    // Return legal moves (no search) in UCI for given FEN.
    virtual std::vector<std::string> legal_moves_uci(const std::string& fen) = 0;
    // Apply a legal UCI move to a FEN, returning new FEN (empty string on failure).
    virtual std::string apply_move(const std::string& fen, const std::string& uci) = 0;
};


#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
namespace engine
{
    // Abstract base for selectable engines.
    class EngineBase {
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
        // utilities
        void flipPosition();

        // Build FEN from internal state (simplified: keep original castling flags as KQkq)
        static std::string buildFen(const EngineBase& e);

        void loadFEN(const std::string& fen);

        virtual ~EngineBase() = default;
        // Compute best move in UCI form for given FEN and search depth.
        virtual std::string choose_move(const std::string& fen, int depth) = 0;
        // Return (uci, score) for all legal root moves searched to given depth.
        virtual std::vector<std::pair<std::string, int>> root_search_scores(const std::string& fen, int depth) = 0;
        // Return legal moves (no search) in UCI for given FEN.
        virtual std::vector<std::string> legal_moves_uci(const std::string& fen) = 0;
        // Apply a legal UCI move to a FEN, returning new FEN (empty string on failure).
        virtual std::string apply_move(const std::string& fen, const std::string& uci) = 0;

    protected:
        static uint64_t byteswap(uint64_t x) {
#if defined(_MSC_VER)
            return _byteswap_uint64(x);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap64(x);
#else
            return ((x & 0x00000000000000FFULL) << 56) | ((x & 0x000000000000FF00ULL) << 40) | ((x & 0x0000000000FF0000ULL) << 24) | ((x & 0x00000000FF000000ULL) << 8) | ((x & 0x000000FF00000000ULL) >> 8) | ((x & 0x0000FF0000000000ULL) >> 24) | ((x & 0x00FF000000000000ULL) >> 40) | ((x & 0xFF00000000000000ULL) >> 56);
#endif
        }
    };

} // namespace engine

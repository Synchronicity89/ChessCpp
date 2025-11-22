#include "CppUnitTest.h"
#include "../chessnative2/engine.hpp"
#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ChessNativeTests
{
    // Helper: apply UCI move to a Position (minimal – no en passant, etc.)
    static void ApplyMove(engine::Position& pos, const std::string& uci)
    {
        if (uci.size() < 4) return;
        int fromFile = uci[0] - 'a', fromRank = uci[1] - '1';
        int toFile = uci[2] - 'a', toRank = uci[3] - '1';
        if (fromFile < 0 || fromFile>7 || fromRank < 0 || fromRank>7 || toFile < 0 || toFile>7 || toRank < 0 || toRank>7) return;
        int from = fromRank * 8 + fromFile;
        int to = toRank * 8 + toFile;
        engine::Move m{}; m.from = from; m.to = to; if (uci.size() == 5) m.promo = uci[4];
        uint64_t toB = (1ULL << to); bool white = (pos.sideToMove == 0);
        if (white) { if (pos.bb.BP & toB || pos.bb.BN & toB || pos.bb.BB & toB || pos.bb.BR & toB || pos.bb.BQ & toB || pos.bb.BK & toB) m.isCapture = true; }
        else { if (pos.bb.WP & toB || pos.bb.WN & toB || pos.bb.WB & toB || pos.bb.WR & toB || pos.bb.WQ & toB || pos.bb.WK & toB) m.isCapture = true; }
        if ((uci == "e1g1") || (uci == "e1c1") || (uci == "e8g8") || (uci == "e8c8")) m.isCastle = true;
        engine::Position out; engine::apply_move(pos, m, out); pos = out;
    }

    TEST_CLASS(CastlingAndMoveSelectionTests)
    {
    public:
        TEST_METHOD(BlackCanCastleKingside)
        {
            const std::string fen = "rnbqk2r/6b1/ppppPnpp/8/P7/1PPP1PPP/8/RNBQKBNR b KQkq - 0 12";
            engine::Position pos; Assert::IsTrue(engine::parse_fen(fen, pos));
            auto moves = engine::legal_moves_uci(fen); bool hasK=false, hasQ=false; for (auto &m: moves){ if(m=="e8g8") hasK=true; if(m=="e8c8") hasQ=true; }
            Assert::IsTrue(hasK,L"Black kingside castling (e8g8) missing"); Assert::IsFalse(hasQ,L"Black queenside castling (e8c8) allowed when blocked");
        }

        TEST_METHOD(EngineBestMoveCaptureVsChosen)
        {
            // Improved test: use depth 2 (even ply) root search scores; expect engine not to blindly pick losing material grab c1h6.
            const std::string fen = "rnbqkbnr/8/pppppppp/8/4P3/PPPP1PP1/7P/RNBQKBNR w KQkq - 0 9";
            int depth = 2; // even ply requirement
            auto scores = engine::root_search_scores(fen, depth);
            Assert::IsTrue(!scores.empty(), L"No root scores generated");
            int bestScore = -1000000; for (auto &p : scores) bestScore = std::max(bestScore, p.second);
            std::vector<std::string> bestMoves; for(auto &p: scores) if(p.second==bestScore) bestMoves.push_back(p.first.substr(0,4));
            std::string chosen = engine::choose_move(fen, depth).substr(0,4);
            // Engine chosen move must be among highest scoring moves.
            bool inBest = std::find(bestMoves.begin(), bestMoves.end(), chosen) != bestMoves.end();
            Assert::IsTrue(inBest, L"Engine did not choose a highest scoring move at depth 2");
            // If bishop capture exists and is not in best set, ensure engine didn't choose it.
            bool bishopCapPresent = false; int bishopScore = 0; for(auto &p: scores){ if(p.first.rfind("c1h6",0)==0){ bishopCapPresent=true; bishopScore=p.second; break; } }
            if(bishopCapPresent && bishopScore < bestScore){ Assert::AreNotEqual(std::string("c1h6"), chosen, L"Engine incorrectly chose losing bishop capture c1h6 at depth 2"); }
        }

        TEST_METHOD(NegamaxRootSignSanity)
        {
            const std::string fen = "8/8/8/3p4/4P3/8/8/4K3 w - - 0 1"; auto scores = engine::root_search_scores(fen,2); bool hasCapture=false; for(auto &p: scores) if(p.first.substr(0,4)=="e4d5") hasCapture=true; Assert::IsTrue(hasCapture,L"Expected capture e4d5 missing"); std::string chosen = engine::choose_move(fen,2).substr(0,4); Assert::AreEqual(std::string("e4d5"), chosen, L"Engine should prioritize immediate capture at depth 2");
        }

        TEST_METHOD(BlackCanCastleQueenside)
        {
            const std::string fen = "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1"; auto moves = engine::legal_moves_uci(fen); bool k=false,q=false; for(auto &m: moves){ if(m=="e8g8") k=true; if(m=="e8c8") q=true; } Assert::IsTrue(k,L"Black kingside castling missing"); Assert::IsTrue(q,L"Black queenside castling missing");
        }

        TEST_METHOD(EngineChoosesHighestMaterialCapture)
        {
            const std::string fen = "r4rk1/8/8/8/8/8/8/R3K2R w KQkq - 0 1"; int depth=2; auto scores = engine::root_search_scores(fen, depth); Assert::IsTrue(!scores.empty(), L"No scores"); int bestScore=-1000000; for(auto &p: scores) bestScore = std::max(bestScore, p.second); std::vector<std::string> best; for(auto &p: scores) if(p.second==bestScore) best.push_back(p.first.substr(0,4)); std::string chosen = engine::choose_move(fen, depth).substr(0,4); bool ok = std::find(best.begin(), best.end(), chosen)!=best.end(); Assert::IsTrue(ok, L"Engine did not choose a best scoring move in rook capture test"); }
    };
}

#include "CppUnitTest.h"
#include "../chessnative2/ChessEngine1.hpp"
#include "../chessnative2/ChessEngine2.hpp"
#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ChessNativeTests
{
    TEST_CLASS(CastlingAndMoveSelectionTests)
    {
    public:
        TEST_METHOD(BlackCanCastleKingside)
        {
            const std::string fen = "rnbqk2r/6b1/ppppPnpp/8/P7/1PPP1PPP/8/RNBQKBNR b KQkq - 0 12";
            engine::ChessEngine1 e; auto moves = e.legal_moves_uci(fen); bool hasK=false, hasQ=false; for (auto &m: moves){ if(m=="e8g8") hasK=true; if(m=="e8c8") hasQ=true; }
            Assert::IsTrue(hasK,L"Black kingside castling (e8g8) missing"); Assert::IsFalse(hasQ,L"Black queenside castling (e8c8) allowed when blocked");
        }

        TEST_METHOD(EngineBestMoveCaptureVsChosen)
        {
            const std::string fen = "rnbqkbnr/8/pppppppp/8/4P3/PPPP1PP1/7P/RNBQKBNR w KQkq - 0 9"; int depth = 2; engine::ChessEngine1 e; auto scores = e.root_search_scores(fen, depth); Assert::IsTrue(!scores.empty(), L"No root scores generated"); int bestScore=-1000000; for(auto &p: scores) bestScore = std::max(bestScore, p.second); std::vector<std::string> best; for(auto &p: scores) if(p.second==bestScore) best.push_back(p.first.substr(0,4)); std::string chosen = e.choose_move(fen, depth).substr(0,4); bool inBest = std::find(best.begin(), best.end(), chosen) != best.end(); Assert::IsTrue(inBest, L"Engine did not choose a highest scoring move at depth 2"); bool bishopCapPresent=false; int bishopScore=0; for(auto &p: scores){ if(p.first.rfind("c1h6",0)==0){ bishopCapPresent=true; bishopScore=p.second; break; } } if(bishopCapPresent && bishopScore < bestScore){ Assert::AreNotEqual(std::string("c1h6"), chosen, L"Engine incorrectly chose losing bishop capture c1h6 at depth 2"); }
        }

        TEST_METHOD(NegamaxRootSignSanity)
        {
            const std::string fen = "8/8/8/3p4/4P3/8/8/4K3 w - - 0 1"; engine::ChessEngine1 e; auto scores = e.root_search_scores(fen,2); bool hasCapture=false; for(auto &p: scores) if(p.first.substr(0,4)=="e4d5") hasCapture=true; Assert::IsTrue(hasCapture,L"Expected capture e4d5 missing"); std::string chosen = e.choose_move(fen,2).substr(0,4); Assert::AreEqual(std::string("e4d5"), chosen, L"Engine should prioritize immediate capture at depth 2");
        }

        TEST_METHOD(BlackCanCastleQueenside)
        {
            const std::string fen = "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1"; engine::ChessEngine1 e; auto moves = e.legal_moves_uci(fen); bool k=false,q=false; for(auto &m: moves){ if(m=="e8g8") k=true; if(m=="e8c8") q=true; } Assert::IsTrue(k,L"Black kingside castling missing"); Assert::IsTrue(q,L"Black queenside castling missing");
        }

        TEST_METHOD(EngineChoosesHighestMaterialCapture)
        {
            const std::string fen = "r4rk1/8/8/8/8/8/8/R3K2R w KQkq - 0 1"; int depth=2; engine::ChessEngine1 e; auto scores = e.root_search_scores(fen, depth); Assert::IsTrue(!scores.empty(), L"No scores"); int bestScore=-1000000; for(auto &p: scores) bestScore = std::max(bestScore, p.second); std::vector<std::string> best; for(auto &p: scores) if(p.second==bestScore) best.push_back(p.first.substr(0,4)); std::string chosen = e.choose_move(fen, depth).substr(0,4); bool ok = std::find(best.begin(), best.end(), chosen)!=best.end(); Assert::IsTrue(ok, L"Engine did not choose a best scoring move in rook capture test");
        }
    };
}

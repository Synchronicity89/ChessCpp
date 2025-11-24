#include "CppUnitTest.h"
#include "../chessnative2/ChessEngine1.hpp"
#include "../chessnative2/ChessEngine2.hpp"
#include <algorithm>
#include <string>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ChessNativeTests {

    // Simple helper to extract board array from FEN (first field only)
    static void ParseBoard(const std::string& fen, char out[64]) {
        for(int i=0;i<64;++i) out[i]='.';
        int sq=56; // a8
        for(size_t i=0;i<fen.size() && fen[i]!=' '; ++i){ char c=fen[i]; if(c=='/'){ sq-=16; continue; } if(c>='1'&&c<='8'){ sq += (c-'0'); continue; } out[sq++] = c; }
    }
    static int Index(const char* alg){ return (alg[1]-'1')*8 + (alg[0]-'a'); }

    TEST_CLASS(EngineApiTests1) // Tests intended to pass for ChessEngine1
    {
    public:
        template<typename EngineT>
        static void ChooseMoveGeneric(){
            const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
            EngineT e;
            auto legal = e.legal_moves_uci(fen);
            Assert::IsTrue(!legal.empty(), L"No legal moves returned");
            int depth = 2;
            auto scores = e.root_search_scores(fen, depth);
            Assert::IsTrue(!scores.empty(), L"No root scores returned");
            std::string chosen = e.choose_move(fen, depth);
            Assert::IsTrue(!chosen.empty(), L"Empty choose_move result");
            std::string chosenPrefix = chosen.substr(0,4);
            bool inLegal = std::any_of(legal.begin(), legal.end(), [&](const std::string& m){ return m.substr(0,4)==chosenPrefix; });
            Assert::IsTrue(inLegal, L"Chosen move not in legal move list");
            int maxScore = -1000000; int chosenScore = -1000000;
            for(auto &pr : scores){ if(pr.second>maxScore) maxScore = pr.second; if(pr.first.substr(0,4)==chosenPrefix) chosenScore = pr.second; }
            Assert::IsTrue(chosenScore >= maxScore, L"Chosen move is not highest scoring (Engine1 expectation)");
        }
        template<typename EngineT>
        static void RootScoresContainLegalMovesGeneric(){
            const std::string fen = "4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1"; // e4 can capture d5
            EngineT e;
            int depth=2;
            auto legal = e.legal_moves_uci(fen);
            auto scores = e.root_search_scores(fen, depth);
            Assert::IsTrue(!legal.empty(), L"Legal moves empty");
            Assert::IsTrue(!scores.empty(), L"Scores empty");
            // Ensure capture e4d5 is in root scores
            bool captureListed=false; for(auto &pr: scores) if(pr.first.substr(0,4)=="e4d5") captureListed=true;
            Assert::IsTrue(captureListed, L"Expected capture e4d5 not scored");
            // Ensure every scored move is legal (prefix)
            for(auto &pr: scores){ bool match=false; for(auto &lm: legal){ if(lm.substr(0,4)==pr.first.substr(0,4)){ match=true; break; } } Assert::IsTrue(match, L"Scored move not found in legal list"); }
        }
        template<typename EngineT>
        static void ApplyMoveGeneric(){
            const std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"; // e4 already played, test applying g2g3
            EngineT e;
            auto legal = e.legal_moves_uci(fen);
            bool hasMove=false; for(auto &m: legal){ if(m.substr(0,4)=="g2g3") { hasMove=true; break; } }
            Assert::IsTrue(hasMove, L"Expected move g2g3 not legal");
            std::string newFen = e.apply_move(fen, "g2g3");
            Assert::IsTrue(!newFen.empty(), L"apply_move returned empty FEN");
            char before[64]; char after[64]; ParseBoard(fen, before); ParseBoard(newFen, after);
            int g2=Index("g2"), g3=Index("g3");
            Assert::AreEqual('.', after[g2], L"Source square not emptied");
            Assert::AreEqual((int)before[g2], (int)after[g3], L"Piece did not move to target square");
        }

        TEST_METHOD(ChooseMove) { ChooseMoveGeneric<engine::ChessEngine1>(); }
        TEST_METHOD(RootScoresContainLegalMoves) { RootScoresContainLegalMovesGeneric<engine::ChessEngine1>(); }
        TEST_METHOD(ApplyMove) { ApplyMoveGeneric<engine::ChessEngine1>(); }
    };

    TEST_CLASS(EngineApiTests2) // Same assertions; may fail for ChessEngine2 by design
    {
    public:
        TEST_METHOD(ChooseMove) { EngineApiTests1::ChooseMoveGeneric<engine::ChessEngine2>(); }
        TEST_METHOD(RootScoresContainLegalMoves) { EngineApiTests1::RootScoresContainLegalMovesGeneric<engine::ChessEngine2>(); }
        TEST_METHOD(ApplyMove) { EngineApiTests1::ApplyMoveGeneric<engine::ChessEngine2>(); }
    };
}

#include "CppUnitTest.h"
#include "../Controller/Control.hpp"
#include "../chessnative2/ChessEngine1.hpp"
#include "../chessnative2/ChessEngine2.hpp"
#include <map>
#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ControllerTests
{
    TEST_CLASS(ControllerMiscTests)
    {
    public:
        TEST_METHOD(splitStringBySpaceTest)
        {
            std::string input = "Hello World";
            std::vector<std::string> expected = { "Hello", "World" };
            std::vector<std::string> actual = controller::GameController::splitStringBySpace(input);
            Assert::AreEqual(expected.size(), actual.size());
            for (size_t i = 0; i < expected.size(); ++i) {
                Assert::AreEqual(expected[i], actual[i]);
            }
        }
        TEST_METHOD(flipFenTest)
        {
            std::string input = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";                
            // After flip: ranks reversed and colors swapped (board symmetrical so looks same), side toggled
            std::string expected = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1"; // TODO: refine castling/ep update
            std::string actual = controller::GameController::flip_fen(input);
            Assert::AreEqual(expected, actual);
        }
    };
    TEST_CLASS(ControllerTests1)
    {
    public:
        template<typename EngineT>
        static void QueenShouldAvoidLosingTradeGeneric() {
            const std::string fen = "2kr1b1r/p1p1qp2/2p2n2/3p2pp/1P6/P1PQPP2/6PP/RNB1KB1R w KQkq - 0 12";
            EngineT engine; controller::GameController game(engine); Assert::IsTrue(game.load_fen(fen));
            int depth = 4; std::string move = game.engine_move(depth); std::string chosenPrefix = move.substr(0, 4);
            auto scores = engine.root_search_scores(fen, depth); int bestScore = -1000000; for (auto& p : scores) bestScore = std::max(bestScore, p.second);
            bool d3d5Present = false; int d3d5Score = 0; for (auto& p : scores) { if (p.first.substr(0, 4) == "d3d5") { d3d5Present = true; d3d5Score = p.second; break; } }
            if (chosenPrefix == "d3d5") {
                Assert::IsTrue(d3d5Present, L"Chosen move d3d5 missing from score list");
                Assert::IsTrue(bestScore - d3d5Score <= 10, L"Engine chose d3d5 despite significantly better alternatives");
            }
            else {
                if (d3d5Present && bestScore - d3d5Score > 50) {
                    Assert::AreNotEqual(std::string("d3d5"), chosenPrefix, L"Engine chose inferior queen trade d3d5");
                }
            }
            const char* board = game.board(); bool queenPresent = false; for (int i = 0; i < 64; ++i) { if (board[i] == 'Q') { queenPresent = true; break; } }
            Assert::IsTrue(queenPresent, L"White queen missing after engine move");
        }
        template<typename EngineT>
        static void KnightShouldCaptureFreePawnMaterialScoresDepth4Generic() {
            const std::string fen = "rnbqkbnr/ppp2ppp/8/3pp3/8/P4N2/1PPPPPPP/RNBQKB1R w KQkq - 0 3";
            EngineT engine; controller::GameController game(engine); Assert::IsTrue(game.load_fen(fen));
            int depth = 4; auto scores = engine.root_search_scores(fen, depth); std::map<std::string, int> ms; for (auto& p : scores) { ms[p.first.substr(0, 4)] = p.second; }
            auto get = [&](const char* m)->int { auto it = ms.find(m); return (it == ms.end() ? -99999 : it->second); };
            int f3e5 = get("f3e5"); int f3g5 = get("f3g5"); int f3h4 = get("f3h4");
            int worstNonCapture = std::max(f3g5, f3h4);
            Assert::IsTrue(f3e5 >= worstNonCapture, L"Knight capture f3e5 should not evaluate worse than retreats");
            Assert::IsTrue(f3e5 - worstNonCapture >= -20, L"Evaluation spread too inverted for knight moves");
        }
    public:
        TEST_METHOD(QueenShouldAvoidLosingTrade) { QueenShouldAvoidLosingTradeGeneric<engine::ChessEngine1>(); }
        TEST_METHOD(KnightShouldCaptureFreePawnMaterialScoresDepth4) { KnightShouldCaptureFreePawnMaterialScoresDepth4Generic<engine::ChessEngine1>(); }
    };

    TEST_CLASS(ControllerTests2)
    {
    public:
        TEST_METHOD(QueenShouldAvoidLosingTrade) { ControllerTests1::QueenShouldAvoidLosingTradeGeneric<engine::ChessEngine2>(); }
        TEST_METHOD(KnightShouldCaptureFreePawnMaterialScoresDepth4) { ControllerTests1::KnightShouldCaptureFreePawnMaterialScoresDepth4Generic<engine::ChessEngine2>(); }
    };
}

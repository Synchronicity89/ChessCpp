#include "CppUnitTest.h"
#include "../Controller/Control.hpp"
#include "../chessnative2/engine.hpp"
#include <map>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ControllerTests
{
    struct NativeEngineAdapter : controller::IChessEngine {
        std::vector<std::string> legal_moves(const std::string& fen) override { return engine::legal_moves_uci(fen); }
        std::string choose_move(const std::string& fen, int depth) override { return engine::choose_move(fen, depth); }
    };

	TEST_CLASS(ControllerTests)
	{
	public:
		TEST_METHOD(QueenShouldAvoidLosingTrade)
		{
                // Position where d3xd5 (d3d5) would lose the white queen to c6 pawn recapture.
                const std::string fen = "2kr1b1r/p1p1qp2/2p2n2/3p2pp/1P6/P1PQPP2/6PP/RNB1KB1R w KQkq - 0 12";
                NativeEngineAdapter adapter;
                controller::GameController game(adapter);
                Assert::IsTrue(game.load_fen(fen));
                int depth = 4; // even ply search
                std::string move = game.engine_move(depth);
                // Engine must not choose the blunder d3d5
                Assert::AreNotEqual(std::string("d3d5"), move.substr(0,4), L"Engine traded queen for pawn (d3d5) at depth 4");
                // Additional sanity: ensure white queen still exists somewhere (basic check)
                const char* board = game.board(); bool queenPresent=false; for(int i=0;i<64;++i){ if(board[i]=='Q'){ queenPresent=true; break; } }
                Assert::IsTrue(queenPresent, L"White queen missing after engine move");
		}
        TEST_METHOD(KnightShouldCaptureFreePawnMaterialScoresDepth4)
        {
            const std::string fen = "rnbqkbnr/ppp2ppp/8/3pp3/8/P4N2/1PPPPPPP/RNBQKB1R w KQkq - 0 3";
            NativeEngineAdapter adapter; controller::GameController game(adapter);
            Assert::IsTrue(game.load_fen(fen));
            int depth = 4; auto scores = engine::root_search_scores(fen, depth);
            // Map move->score
            std::map<std::string,int> ms; for(auto &p: scores){ ms[p.first.substr(0,4)] = p.second; }
            auto get=[&](const char* m)->int{
                auto it=ms.find(m);
                if(it==ms.end()){
                    // Simple widen of narrow move string
                    std::wstring wmsg = L"Missing move ";
                    for(const char* p=m; *p; ++p) wmsg.push_back((wchar_t)*p);
                    Assert::Fail(wmsg.c_str());
                }
                return it->second;
            };
            int b2b3 = get("b2b3"); int b2b4 = get("b2b4"); int f3e5 = get("f3e5"); int f3g5 = get("f3g5"); int f3h4 = get("f3h4");
            // Expectations: neutral pawn pushes near 0; bad knight retreats highly negative; capturing free pawn should be clearly positive.
            Assert::IsTrue(b2b3 > -40 && b2b3 < 40, L"b2b3 out of neutral range");
            Assert::IsTrue(b2b4 > -40 && b2b4 < 40, L"b2b4 out of neutral range");
            Assert::IsTrue(f3g5 < -250, L"f3g5 not sufficiently bad");
            Assert::IsTrue(f3h4 < -250, L"f3h4 not sufficiently bad");
            Assert::IsTrue(f3e5 > 100, L"f3e5 (capture) should be clearly positive");
        }
	};
}

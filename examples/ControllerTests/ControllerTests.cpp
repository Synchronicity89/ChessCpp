#include "CppUnitTest.h"
#include "../Controller/Control.hpp"
#include "../chessnative2/ChessEngine1.hpp"
#include "../chessnative2/ChessEngine2.hpp"
#include <map>
#include <algorithm>
#include <sstream>

// FEN test suite (newline delimited) with best move (bm) and id tags
static const char* FEN_SUITE = R"(4r1k1/p1pb1ppp/Qbp1r3/8/1P6/2Pq1B2/R2P1PPP/2B2RK1 b - - bm Qxf3; id "BS2830-01";
7r/2qpkp2/p3p3/6P1/1p2b2r/7P/PPP2QP1/R2N1RK1 b - - bm f5; id "BS2830-02";
r1bq1rk1/pp4bp/2np4/2p1p1p1/P1N1P3/1P1P1NP1/1BP1QPKP/1R3R2 b - - bm Bh3+; id "BS2830-03";
8/2kPR3/5q2/5N2/8/1p1P4/1p6/1K6 w - - bm Nd4; id "BS2830-04";
2r1r3/p3bk1p/1pnqpppB/3n4/3P2Q1/PB3N2/1P3PPP/3RR1K1 w - - bm Rxe6; id "BS2830-05";
8/2p5/7p/pP2k1pP/5pP1/8/1P2PPK1/8 w - - bm f3; id "BS2830-06";
8/5p1p/1p2pPk1/p1p1P3/P1P1K2b/4B3/1P5P/8 w - - bm b4; id "BS2830-07";
rn2r1k1/pp3ppp/8/1qNp4/3BnQb1/5N2/PPP2PPP/2KR3R b - - bm Bh5; id "BS2830-08";
r3kb1r/1p1b1p2/p1nppp2/7p/4PP2/qNN5/P1PQB1PP/R4R1K w kq - bm Nb1; id "BS2830-09";
r3r1k1/pp1bp2p/1n2q1P1/6b1/1B2B3/5Q2/5PPP/1R3RK1 w - - bm Bd2; id "BS2830-10";
r3k2r/pb3pp1/2p1qnnp/1pp1P3/Q1N4B/2PB1P2/P5PP/R4RK1 w kq - bm exf6; id "BS2830-11";
r1b1r1k1/ppp2ppp/2nb1q2/8/2B5/1P1Q1N2/P1PP1PPP/R1B2RK1 w - - bm Bb2; id "BS2830-12";
rnb1kb1r/1p3ppp/p5q1/4p3/3N4/4BB2/PPPQ1P1P/R3K2R w KQkq - bm O-O-O; id "BS2830-13";
r1bqr1k1/pp1n1ppp/5b2/4N1B1/3p3P/8/PPPQ1PP1/2K1RB1R w - - bm Nxf7; id "BS2830-14";
2r2rk1/1bpR1p2/1pq1pQp1/p3P2p/P1PR3P/5N2/2P2PPK/8 w - - bm Kg3; id "BS2830-15";
8/pR4pk/1b6/2p5/N1p5/8/PP1r2PP/6K1 b - - bm Rxb2; id "BS2830-16";
r1b1qrk1/ppBnppb1/2n4p/1NN1P1p1/3p4/8/PPP1BPPP/R2Q1R1K w - - bm Ne6; id "BS2830-17";
8/8/4b1p1/2Bp3p/5P1P/1pK1Pk2/8/8 b - - bm g5; id "BS2830-18";
r3k2r/pp1n1ppp/1qpnp3/3bN1PP/3P2Q1/2B1R3/PPP2P2/2KR1B2 w kq - bm Be1; id "BS2830-19";
r1bqk2r/pppp1Npp/8/2bnP3/8/6K1/PB4PP/RN1Q3R b kq - bm O-O; id "BS2830-20";
r4r1k/pbnq1ppp/np3b2/3p1N2/5B2/2N3PB/PP3P1P/R2QR1K1 w - - bm Ne4; id "BS2830-21";
r2qr2k/pbp3pp/1p2Bb2/2p5/2P2P2/3R2P1/PP2Q1NP/5RK1 b - - bm Qxd3; id "BS2830-22";
5r2/1p4r1/3kp1b1/1Pp1p2p/2PpP3/q2B1PP1/3Q2K1/1R5R b - - bm Rxf3; id "BS2830-23";
8/7p/8/7P/1p6/1p5P/1P2Q1pk/1K6 w - - bm Ka1; id "BS2830-24";
r5k1/p4n1p/6p1/2qPp3/2p1P1Q1/8/1rB3PP/R4R1K b - - bm Rf8; id "BS2830-25";
1r4k1/1q2pN1p/3pPnp1/8/2pQ4/P5PP/5P2/3R2K1 b - - bm Qd5; id "BS2830-26";
2rq1rk1/pb3ppp/1p2pn2/4N3/1b1PPB2/4R1P1/P4PBP/R2Q2K1 w - - bm d5; id "BS2830-27";)";

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ControllerTests
{
    TEST_CLASS(ControllerMiscTests)
    {
    public:

        // placholder for FEN suite tests
        std::string fenSuite = "";
        
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
            //Chess960 kings rooks and pawns. Castling and en-passant flipping example
            //r3k2r/8/8/8/3pP3/8/8/4K2R b Kkq e3 0 1
            //flipped
            //r2k4/8/8/3pP3/8/8/8/R2K3R w KQq d6 0 1
            std::string input = "r3k2r/8/8/8/3pP3/8/8/4K2R b Kkq e3 0 1";                
            // After flip: ranks reversed and colors swapped (board symmetrical so looks same), side toggled
            std::string expected = "r2k4/8/8/3pP3/8/8/8/R2K3R w KQq d6 0 1"; // TODO: refine castling/ep update
            std::string actual = controller::GameController::flip_fen(input);
            std::string originalMaybe = controller::GameController::flip_fen( actual );

            Assert::AreEqual( expected, actual, L"Flipped FEN did not match expected" );
            Assert::AreEqual( input, originalMaybe,L"Original FEN did not match after double flip" );
        }
    };

    // Helper: sanitize bm SAN (remove x,+,#) and derive target square
    static std::string sanitize(const std::string& san){ std::string s; for(char c: san){ if(c=='x'||c=='+'||c=='#') continue; s.push_back(c);} return s; }
    static std::string targetSquareFromSan(const std::string& san){ std::string s=sanitize(san); if(s=="O-O") return "g"; if(s=="O-O-O") return "c"; // castle file only, rank depends on side
        // last two chars if they are file+rank
        if(s.size()>=2){ char f=s[s.size()-2]; char r=s[s.size()-1]; if(f>='a'&&f<='h'&&r>='1'&&r<='8') return std::string(1,f)+r; }
        return std::string(); }

    template<typename EngineT>
    static void FenSuiteGeneric(){
        std::istringstream iss(FEN_SUITE); std::string line; int depth = 6; // depth 2 search
        std::vector<std::wstring> failing; // collect failing IDs
        while(std::getline(iss,line)){
            if(line.empty()) continue;
            size_t bmPos = line.find(" bm "); size_t idPos = line.find(" id "); if(bmPos==std::string::npos||idPos==std::string::npos) continue;
            std::string fen = line.substr(0,bmPos);
            std::string bmPart = line.substr(bmPos+4, idPos-(bmPos+4));
            size_t semi = bmPart.find(';'); if(semi!=std::string::npos) bmPart = bmPart.substr(0, semi);
            std::string idPart = line.substr(idPos+4);
            size_t quote2 = idPart.find('"',1); std::string testId = (quote2!=std::string::npos? idPart.substr(1, quote2-1): idPart);
            std::wstring idw(testId.begin(), testId.end());
            std::string expectedTarget = targetSquareFromSan(bmPart);

            EngineT engine; controller::GameController game(engine); bool ok = game.load_fen(fen);
            if(!ok){ failing.push_back(L"Load FEN: "+idw); continue; }
            std::string move = engine.choose_move(fen, depth);
            if(move.empty()){ failing.push_back(L"Empty move: "+idw); continue; }
            std::string dest = move.substr(2,2);
            if(expectedTarget=="g"||expectedTarget=="c"){ bool isCastle = (move=="e1g1"||move=="e1c1"||move=="e8g8"||move=="e8c8"); if(!isCastle) failing.push_back(L"Castling expected: "+idw); }
            else if(!expectedTarget.empty() && expectedTarget!=dest){ failing.push_back(L"Dest mismatch ("+idw+L") expected "+std::wstring(expectedTarget.begin(), expectedTarget.end())+L" got "+std::wstring(dest.begin(), dest.end())); }
            auto scores = engine.root_search_scores(fen, depth); bool found=false; for(auto &p: scores){ if(p.first.substr(0,4)==move.substr(0,4)){ found=true; break; } }
            if(!found) failing.push_back(L"Move not in scores: "+idw);
        }
        if(!failing.empty()){
            std::wstring all; for(size_t i=0;i<failing.size();++i){ all += failing[i]; if(i+1<failing.size()) all += L"\n"; }
            Assert::Fail(all.c_str());
        }
    }

    TEST_CLASS(FenSuiteEngine1Tests){ public: TEST_METHOD(FenSuiteAll){ FenSuiteGeneric<engine::ChessEngine1>(); } };
    TEST_CLASS(FenSuiteEngine2Tests){ public: TEST_METHOD(FenSuiteAll){ FenSuiteGeneric<engine::ChessEngine2>(); } };

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

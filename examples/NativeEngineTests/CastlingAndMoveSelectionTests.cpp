#include "CppUnitTest.h"
#include "../chessnative2/engine.hpp"

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

        engine::Move m{};
        m.from = from;
        m.to = to;
        if (uci.size() == 5)
            m.promo = uci[4];
        uint64_t toB = (1ULL << to);
        bool white = (pos.sideToMove == 0);
        if (white) {
            if (pos.bb.BP & toB || pos.bb.BN & toB || pos.bb.BB & toB || pos.bb.BR & toB || pos.bb.BQ & toB || pos.bb.BK & toB)
                m.isCapture = true;
        }
        else {
            if (pos.bb.WP & toB || pos.bb.WN & toB || pos.bb.WB & toB || pos.bb.WR & toB || pos.bb.WQ & toB || pos.bb.WK & toB)
                m.isCapture = true;
        }
        if ((uci == "e1g1") || (uci == "e1c1") || (uci == "e8g8") || (uci == "e8c8"))
            m.isCastle = true;

        engine::Position out;
        engine::apply_move(pos, m, out);
        pos = out;
    }

    TEST_CLASS(CastlingAndMoveSelectionTests)
    {
    public:
        TEST_METHOD(BlackCanCastleKingside)
        {
            const std::string fen = "rnbqk2r/6b1/ppppPnpp/8/P7/1PPP1PPP/8/RNBQKBNR b KQkq - 0 12";
            engine::Position pos;
            Assert::IsTrue(engine::parse_fen(fen, pos), L"FEN parse failed");
            auto moves = engine::legal_moves_uci(fen);
            bool hasKingside = false;
            bool hasQueenside = false;
            for (auto& m : moves) {
                if (m == "e8g8") hasKingside = true;
                if (m == "e8c8") hasQueenside = true;
            }
            Assert::IsTrue(hasKingside, L"Black kingside castling (e8g8) missing");
            Assert::IsFalse(hasQueenside, L"Black queenside castling (e8c8) allowed when blocked");
        }

        TEST_METHOD(EngineBestMoveCaptureVsChosen)
        {
            const std::string fen = "rnbqkbnr/8/pppppppp/8/4P3/PPPP1PP1/7P/RNBQKBNR w KQkq - 0 9";
            engine::Position pos;
            Assert::IsTrue(engine::parse_fen(fen, pos), L"FEN parse failed");
            auto legal = engine::legal_moves_uci(fen);
            Assert::IsTrue(!legal.empty(), L"No legal moves generated");
            std::string expectedBest;
            int bestEval = -999999;
            for (auto& mv : legal)
            {
                engine::Position tmp = pos;
                ApplyMove(tmp, mv);
                int eval = engine::evaluate(tmp); // already white minus black
                if (eval > bestEval) {
                    bestEval = eval;
                    expectedBest = mv;
                }
            }
            bool foundC1H6 = false;
            for (auto& mv : legal) if (mv.rfind("c1h6", 0) == 0) { foundC1H6 = true; break; }
            Assert::IsTrue(foundC1H6, L"c1h6 (bishop capture) not in legal moves; test assumption broken");
            int depth = 4;
            std::string chosen = engine::choose_move(fen, depth);
            Assert::IsTrue(!chosen.empty(), L"Engine returned empty move");
            Assert::AreEqual(expectedBest.substr(0,4), chosen.substr(0,4), L"Engine did not choose the highest static material gain move");
        }

        TEST_METHOD(NegamaxRootSignSanity)
        {
            const std::string fen = "8/8/8/3p4/4P3/8/8/4K3 w - - 0 1";
            engine::Position pos;
            Assert::IsTrue(engine::parse_fen(fen, pos));
            auto legal = engine::legal_moves_uci(fen);
            bool hasCapture = false;
            for (auto& mv : legal) if (mv == "e4d5") { hasCapture = true; break; }
            Assert::IsTrue(hasCapture, L"Expected capture move e4d5 not found");
            std::string chosen = engine::choose_move(fen, 2);
            Assert::AreEqual(std::string("e4d5"), chosen.substr(0,4), L"Engine should prioritize immediate capture");
        }

        TEST_METHOD(BlackCanCastleQueenside)
        {
            const std::string fen = "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1";
            auto moves = engine::legal_moves_uci(fen);
            bool found = false;
            bool hasKingside = false;
            bool hasQueenside = false;
            for (auto& m : moves) {
                if (m == "e8c8") { found = true; hasQueenside = true; }
                if (m == "e8g8") hasKingside = true;
            }
            Assert::IsTrue(found, L"Black queenside castling (e8c8) missing");
            Assert::IsTrue(hasKingside, L"Black kingside castling (e8g8) missing");
            Assert::IsTrue(hasQueenside, L"Black queenside castling (e8c8) missing");
        }

        TEST_METHOD(EngineChoosesHighestMaterialCapture)
        {
            const std::string fen = "r4rk1/8/8/8/8/8/8/R3K2R w KQkq - 0 1";
            engine::Position pos;
            Assert::IsTrue(engine::parse_fen(fen, pos), L"FEN parse failed");
            auto legal = engine::legal_moves_uci(fen);
            Assert::IsTrue(!legal.empty(), L"No legal moves");
            std::string expectedBest; int bestEval = -1000000;
            for (auto &mv : legal)
            {
                engine::Position tmp = pos;
                ApplyMove(tmp, mv);
                int eval = engine::evaluate(tmp); // white minus black
                if (eval > bestEval) { bestEval = eval; expectedBest = mv; }
            }
            bool hasCaptureA1A8 = false; for (auto &mv : legal) if (mv.rfind("a1a8",0)==0) hasCaptureA1A8 = true;
            Assert::IsTrue(hasCaptureA1A8, L"Expected capture a1a8 missing from legal moves");
            Assert::AreEqual(std::string("a1a8"), expectedBest.substr(0,4), L"Static best should be a1a8 (highest material gain)");
            std::string chosen = engine::choose_move(fen, 2);
            Assert::IsTrue(!chosen.empty(), L"Engine returned empty move");
            Assert::AreEqual(std::string("a1a8"), chosen.substr(0,4), L"Engine did not choose highest material capture a1a8");
        }
    };
}

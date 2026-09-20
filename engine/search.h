#ifndef CHINESE_CHESS_SEARCH_H
#define CHINESE_CHESS_SEARCH_H

// 引擎层：搜索引擎（迭代加深 + PVS Alpha-Beta + 置换表 + 静态搜索）
// 从 ChineseChess.cpp 拆分而来，与平台无关。

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "chess.h"

const int INF_SCORE = 1000000000;
const int MATE_SCORE = 10000000;
const int MAX_SEARCH_PLY = 128;
const int TT_EXACT = 0;
const int TT_LOWER = 1;
const int TT_UPPER = 2;
const int DEFAULT_TIME_MS = 800;

// 全局思考时限（毫秒）。Botzone 版由 parseTimeLimitMs 赋值；本地服务可用 setTimeLimitMs 覆盖。
extern int GLOBAL_TIME_LIMIT_MS;

// 搜索结果附加信息（分数、完成深度、节点数、耗时），供界面显示
struct SearchInfo {
    int score;
    int depth;
    long long nodes;
    int elapsedMs;
    SearchInfo() : score(0), depth(0), nodes(0), elapsedMs(0) {}
};

class SearchEngine {
private:
    static const int TT_BITS = 19;
    static const int TT_SIZE = 1 << TT_BITS;
    static const int TT_MASK = TT_SIZE - 1;

    struct SearchTimeout {};

    struct TTEntry {
        uint64_t key;
        int depth;
        int value;
        int flag;
        Move bestMove;
        TTEntry() : key(0), depth(-1), value(0), flag(TT_EXACT), bestMove() {}
    };

    Chess* chess;
    std::vector<TTEntry> transTable;
    Move killer[MAX_SEARCH_PLY][2];
    int historyScore[2][8][BOARDWIDTH * BOARDHEIGHT];
    std::chrono::steady_clock::time_point startTime;
    int timeLimitMs;
    long long nodes;
    int completedDepth;

public:
    SearchEngine()
        : chess(nullptr), transTable(TT_SIZE), timeLimitMs(GLOBAL_TIME_LIMIT_MS), nodes(0), completedDepth(0) {
        std::memset(historyScore, 0, sizeof(historyScore));
        const char* envLimit = std::getenv("BOTZONE_AI_TIME_MS");
        if (envLimit != nullptr) {
            int parsed = std::atoi(envLimit);
            if (parsed >= 30 && parsed <= 10000) timeLimitMs = parsed;
        }
    }

    // 兼容原 Botzone 接口：只返回最佳着法
    Move findBestMove(Chess& board);

    // 扩展接口：同时返回分数/深度/节点数等信息
    Move findBestMove(Chess& board, SearchInfo& info);

    void setTimeLimitMs(int ms) { timeLimitMs = ms; }

private:
    Move makeCoordMove(const char* source, const char* target) const;
    bool containsMove(const std::vector<Move>& moves, const Move& candidate) const;
    bool hasMajorCapture(const std::vector<Move>& moves) const;
    Move findOpeningBook(const std::vector<Move>& moves) const;
    void checkTime();
    int scoreToTT(int score, int ply) const;
    int scoreFromTT(int score, int ply) const;
    bool probeTT(uint64_t key, int depth, int alpha, int beta, int ply, int& value, Move& bestMove);
    void storeTT(uint64_t key, int depth, int value, int flag, const Move& bestMove, int ply);
    std::pair<int, Move> searchRoot(std::vector<Move>& rootMoves, int depth, const Move& previousBest, int alpha, int beta);
    int negamax(int depth, int alpha, int beta, int ply);
    int quiescence(int alpha, int beta, int ply, int qply);
    void saveKiller(int ply, const Move& move);
    void updateHistory(const Move& move, int depth, colorType side);
    void orderMoves(std::vector<Move>& moves, const Move& hashMove, int ply);
    int mobilityAt(int x, int y, const Grid& grid) const;
    int blockedKnightFeet(int x, int y) const;
    bool findKing(colorType color, int& kx, int& ky) const;
    int filePressurePenalty(colorType side, int kx, int ky) const;
    int sideEvaluation(colorType side) const;
    int evaluate();
};

#endif // CHINESE_CHESS_SEARCH_H

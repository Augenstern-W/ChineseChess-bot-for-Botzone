// 引擎自检：AI 自对弈若干步，验证拆分重构后的引擎完整可用
// 编译: g++ -O2 -std=c++14 -Iengine -Ithird_party/jsoncpp_stub test/selftest.cpp engine/chess.cpp engine/search.cpp -o bin/selftest.exe
#include <cstdio>
#include <cstdlib>

#include "search.h"

int main(int argc, char** argv) {
    int maxPlies = argc > 1 ? std::atoi(argv[1]) : 30;
    Chess chess;
    SearchEngine engine;
    engine.setTimeLimitMs(200);

    for (int i = 0; i < maxPlies; ++i) {
        std::vector<Move> legal;
        chess.generateMovesWithForbidden(legal, true);
        if (legal.empty()) {
            std::printf("ply %d: %s 无合法着法，对局结束\n", i, chess.currentColor() == RED ? "红方" : "黑方");
            break;
        }
        SearchInfo info;
        Move m = engine.findBestMove(chess, info);
        std::printf("ply %2d: %c%c-%c%c  score=%7d depth=%2d nodes=%-9lld time=%dms\n",
            i,
            pgnint2char(m.source_x), int2char(m.source_y),
            pgnint2char(m.target_x), int2char(m.target_y),
            info.score, info.depth, info.nodes, info.elapsedMs);
        chess.makeMoveAssumeLegal(m, true);
    }
    std::printf("selftest OK\n");
    return 0;
}

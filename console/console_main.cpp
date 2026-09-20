// 控制台人机对弈版：不依赖浏览器/网络，运行 chess_console.exe 直接在终端对弈
// 与 web 服务共用同一套引擎（engine/chess.cpp + engine/search.cpp）
// 编译: g++ -O2 -std=c++14 -static -static-libgcc -static-libstdc++ -Iengine console/console_main.cpp engine/chess.cpp engine/search.cpp -o bin/chess_console.exe
//
// 用法:
//   chess_console.exe            交互式选择模式
//   chess_console.exe red        人机（玩家执红先手）
//   chess_console.exe black      人机（玩家执黑后手，电脑先走）
//   chess_console.exe pvp        双人轮流走子
// 指令: h2e2 走棋 | undo 悔棋 | new 新对局 | quit 退出

#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "search.h"

// 棋子中文名（下标对应 stoneType: None/King/Bishop/Knight/Rook/Pawn/Cannon/Assistant）
static const char* const PIECE_RED[]   = { "", "帥", "相", "傌", "俥", "兵", "炮", "仕" };
static const char* const PIECE_BLACK[] = { "", "將", "象", "馬", "車", "卒", "砲", "士" };
static const char* const EMPTY_CELL = "．";

static std::string cellText(const Chess& chess, int x, int y) {
    Grid g = chess.getGrid(x, y);
    if (g.color == EMPTY || g.type == None) return EMPTY_CELL;
    return g.color == RED ? PIECE_RED[g.type] : PIECE_BLACK[g.type];
}

// 打印棋盘。flip=false 红下黑上（执红视角）；flip=true 红上黑下（执黑视角）
static void printBoard(const Chess& chess, bool flip) {
    std::string files = flip ? "i h g f e d c b a" : "a b c d e f g h i";
    std::cout << "\n     " << files << "\n";
    for (int i = 0; i < BOARDHEIGHT; ++i) {
        int y = flip ? i : BOARDHEIGHT - 1 - i;
        std::cout << "  " << y << "  ";
        for (int j = 0; j < BOARDWIDTH; ++j) {
            int x = flip ? BOARDWIDTH - 1 - j : j;
            std::cout << cellText(chess, x, y) << " ";
        }
        std::cout << y << "\n";
    }
    std::cout << "     " << files << "\n";
}

static std::string coordStr(const Move& m) {
    std::string s;
    s += pgnint2char(m.source_x);
    s += int2char(m.source_y);
    s += '-';
    s += pgnint2char(m.target_x);
    s += int2char(m.target_y);
    return s;
}

// 解析 h2e2 形式输入
static bool parseMove(const std::string& text, Move& out) {
    if (text.size() != 4) return false;
    Move m(text.substr(0, 2), text.substr(2, 2));
    if (m.source_x < 0 || m.target_x < 0) return false;
    out = m;
    return true;
}

static const char* sideName(colorType c) { return c == RED ? "红方" : "黑方"; }

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // 终端 UTF-8 输出中文棋子
#endif
    std::ios::sync_with_stdio(false);

    // 模式：命令行参数或交互菜单
    std::string arg = argc > 1 ? argv[1] : "";
    int mode = 0;   // 0 未定, 1 pve_red, 2 pve_black, 3 pvp
    if (arg == "red") mode = 1;
    else if (arg == "black") mode = 2;
    else if (arg == "pvp") mode = 3;

    if (mode == 0) {
        std::cout << "中国象棋 - 控制台版（共用引擎，无需浏览器）\n"
                  << "  1. 人机对战 - 我执红先手\n"
                  << "  2. 人机对战 - 我执黑后手（电脑先走）\n"
                  << "  3. 双人对战\n"
                  << "请选择 [1/2/3]: ";
        std::string line;
        std::getline(std::cin, line);
        if (line == "1") mode = 1;
        else if (line == "2") mode = 2;
        else if (line == "3") mode = 3;
        else { std::cout << "无效选择，默认执红先手\n"; mode = 1; }
    }

    Chess chess;
    SearchEngine engine;
    std::vector<Move> fullHistory;
    bool gameEnded = false;
    std::string endText;

    const colorType playerColor = (mode == 2) ? BLACK : RED;
    const bool isPvp = (mode == 3);

    std::cout << "\n指令：h2e2 走棋 | undo 悔棋 | new 新对局 | quit 退出\n";

    while (true) {
        printBoard(chess, !isPvp && playerColor == BLACK);

        // 终局判定（与本地 web 版一致）
        std::vector<Move> legal;
        chess.generateMovesWithForbidden(legal, true);
        if (legal.empty()) {
            endText = std::string(sideName(oppositeColor(chess.currentColor()))) + "胜！";
            gameEnded = true;
        } else if (chess.exceedMaxPeaceState()) {
            endText = "60 回合无吃子，双方言和。";
            gameEnded = true;
        } else if (chess.positionRepeatCount() >= 3) {
            endText = "三次重复局面，双方言和。";
            gameEnded = true;
        }
        if (gameEnded) {
            std::cout << "\n*** " << endText << " ***\n输入 new 再来一局，quit 退出: ";
            std::string cmd;
            while (std::cin >> cmd) {
                if (cmd == "new") { chess.resetBoard(); fullHistory.clear(); gameEnded = false; break; }
                if (cmd == "quit") return 0;
                std::cout << "输入 new 或 quit: ";
            }
            continue;
        }

        colorType turn = chess.currentColor();
        bool humanTurn = isPvp || turn == playerColor;

        if (!humanTurn) {
            std::cout << "\n电脑（" << sideName(turn) << "）思考中...\n";
            SearchInfo info;
            Move m = engine.findBestMove(chess, info);
            bool givesCheck = chess.moveGivesCheck(m);
            chess.makeMoveAssumeLegal(m, true);
            fullHistory.push_back(m);
            std::cout << "电脑走棋: " << coordStr(m)
                      << "  (score=" << info.score << " depth=" << info.depth
                      << " nodes=" << info.nodes << " " << info.elapsedMs << "ms)\n";
            if (givesCheck) std::cout << "*** 将军！ ***\n";
            continue;
        }

        std::cout << "\n轮到 " << sideName(turn) << " 走棋: ";
        std::string cmd;
        if (!(std::cin >> cmd)) break;

        if (cmd == "quit") return 0;
        if (cmd == "new") { chess.resetBoard(); fullHistory.clear(); continue; }
        if (cmd == "undo") {
            if (isPvp) {
                if (fullHistory.empty()) { std::cout << "还没有着法可悔。\n"; continue; }
                chess.undoMove();
                fullHistory.pop_back();
            } else {
                if (fullHistory.size() < 2) { std::cout << "还没有可悔的着法。\n"; continue; }
                chess.undoMove();   // 撤电脑应手
                chess.undoMove();   // 撤玩家上一步
                fullHistory.pop_back();
                fullHistory.pop_back();
            }
            continue;
        }

        Move m;
        if (!parseMove(cmd, m)) { std::cout << "格式错误，示例: h2e2\n"; continue; }
        if (!chess.isLegalMoveWithForbidden(m)) { std::cout << "非法着法: " << coordStr(m) << "\n"; continue; }
        bool givesCheck = chess.moveGivesCheck(m);
        chess.makeMoveAssumeLegal(m, true);
        fullHistory.push_back(m);
        if (givesCheck) std::cout << "*** 将军！ ***\n";
    }
    return 0;
}

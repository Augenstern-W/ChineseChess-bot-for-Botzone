// 图形界面版：Win32 GDI 原生窗口，单 exe 静态链接，无需浏览器/网络/第三方库
// 与 web/控制台版共用同一套引擎（engine/chess.cpp + engine/search.cpp）
// 编译: g++ -O2 -std=c++14 -mwindows -static -static-libgcc -static-libstdc++ -Iengine gui/gui_main.cpp engine/chess.cpp engine/search.cpp -o bin/chess_gui.exe
//
// 功能: 人机(执红/执黑)/双人三种模式，鼠标选子走子、合法落点提示、悔棋、
//       再来一局、重置、走子记录、AI 思考信息、将军与终局提示。执黑时棋盘自动翻转。

#include <windows.h>
#include <windowsx.h>
#include <cwchar>
#include <string>
#include <vector>

#include "search.h"

// ---------------- 布局常量 ----------------
static const int CELL = 56;                 // 交叉点间距
static const int MARGIN = 40;               // 棋盘边距
static const int BOARD_W = MARGIN * 2 + (BOARDWIDTH - 1) * CELL;
static const int BOARD_H = MARGIN * 2 + (BOARDHEIGHT - 1) * CELL;
static const int SIDE_W = 250;              // 侧栏宽
static const int CLIENT_W = BOARD_W + SIDE_W;
static const int CLIENT_H = BOARD_H;
static const int PIECE_R = 26;              // 棋子半径

// ---------------- 全局状态 ----------------
static HWND g_hwnd;
static HWND g_btn[6];                       // 执红/执黑/双人/悔棋/再来一局/重置
static Chess g_chess;
static int g_mode = 1;                      // 1 人机执红 2 人机执黑 3 双人
static bool g_flip = false;                 // 执黑时翻转棋盘
static bool g_aiThinking = false;
static bool g_gameOver = false;
static std::wstring g_status = L"轮到红方走棋";
static std::wstring g_engineText;           // 最近一次 AI 搜索信息
static int g_selX = -1, g_selY = -1;        // 选中格
static std::vector<Move> g_targets;         // 选中子的合法落点

struct HistoryItem { std::wstring text; Move move; };
static std::vector<HistoryItem> g_history;  // 走子记录
static bool g_lastCheck = false;            // 上一步是否将军

static HFONT g_fontPiece, g_fontUi, g_fontTitle, g_fontSmall, g_fontMark;

static const wchar_t* const PIECE_W_RED[8]   = { L"", L"帥", L"相", L"傌", L"俥", L"兵", L"炮", L"仕" };
static const wchar_t* const PIECE_W_BLACK[8] = { L"", L"將", L"象", L"馬", L"車", L"卒", L"砲", L"士" };

// ---------------- 搜索引擎（仅 AI 线程访问） ----------------
static SearchEngine g_engine;

// ---------------- 坐标换算 ----------------
static int pxX(int x) { return MARGIN + (g_flip ? (BOARDWIDTH - 1 - x) : x) * CELL; }
static int pxY(int y) { return MARGIN + (g_flip ? y : (BOARDHEIGHT - 1 - y)) * CELL; }

static bool pixelToCell(int px, int py, int& cx, int& cy) {
    int col = (px - MARGIN + CELL / 2) / CELL;
    int row = (py - MARGIN + CELL / 2) / CELL;
    if (col < 0 || col >= BOARDWIDTH || row < 0 || row >= BOARDHEIGHT) return false;
    cx = g_flip ? (BOARDWIDTH - 1 - col) : col;
    cy = g_flip ? row : (BOARDHEIGHT - 1 - row);
    return true;
}

// ---------------- 文本绘制 ----------------
static void drawTextC(HDC hdc, int cx, int cy, const wchar_t* s, COLORREF color, HFONT font) {
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    SetTextColor(hdc, color);
    SIZE sz;
    GetTextExtentPoint32W(hdc, s, static_cast<int>(lstrlenW(s)), &sz);
    TextOutW(hdc, cx - sz.cx / 2, cy - sz.cy / 2, s, static_cast<int>(lstrlenW(s)));
    SelectObject(hdc, old);
}

static void drawTextL(HDC hdc, int x, int y, const wchar_t* s, COLORREF color, HFONT font) {
    HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, s, static_cast<int>(lstrlenW(s)));
    SelectObject(hdc, old);
}

// ---------------- AI 后台线程 ----------------
static const UINT WM_AI_DONE = WM_APP + 1;
struct AiResult { Move move; SearchInfo info; };

static DWORD WINAPI aiThread(LPVOID param) {
    Chess* snap = static_cast<Chess*>(param);
    SearchInfo info;
    Move m = g_engine.findBestMove(*snap, info);
    delete snap;
    AiResult* r = new AiResult;
    r->move = m;
    r->info = info;
    PostMessage(g_hwnd, WM_AI_DONE, 0, reinterpret_cast<LPARAM>(r));
    return 0;
}

static void maybeStartAI() {
    if (g_gameOver || g_mode == 3 || g_aiThinking) return;
    colorType player = (g_mode == 2) ? BLACK : RED;
    if (g_chess.currentColor() == player) return;
    g_aiThinking = true;
    g_status = L"电脑思考中…";
    for (int i = 0; i < 6; ++i) EnableWindow(g_btn[i], FALSE);
    InvalidateRect(g_hwnd, nullptr, FALSE);
    Chess* snap = new Chess(g_chess);          // 值拷贝，线程与主界面无并发
    HANDLE h = CreateThread(nullptr, 0, aiThread, snap, 0, nullptr);
    if (h) CloseHandle(h);
}

static void finishGame(const std::wstring& text) {
    g_gameOver = true;
    g_status = L"对局结束：" + text;
    EnableWindow(g_btn[4], TRUE);              // 终局后可点“再来一局”
    MessageBoxW(g_hwnd, text.c_str(), L"对局结束", MB_OK | MB_ICONINFORMATION);
}

// 主线程内执行走子并推进流程
static void applyMove(const Move& m) {
    colorType side = g_chess.currentColor();
    Grid src = g_chess.getGrid(m.source_x, m.source_y);
    const wchar_t* name = side == RED ? PIECE_W_RED[src.type] : PIECE_W_BLACK[src.type];
    // 用 wstring 拼接而非 swprintf：%s 在 MinGW ANSI 宽字符 printf 中按窄字符串解析，中文会乱码
    HistoryItem item;
    item.move = m;
    item.text = std::to_wstring(static_cast<int>(g_history.size()) + 1) + L". " + name + L" ";
    item.text += static_cast<wchar_t>(L'a' + m.source_x);
    item.text += static_cast<wchar_t>(L'0' + m.source_y);
    item.text += L'-';
    item.text += static_cast<wchar_t>(L'a' + m.target_x);
    item.text += static_cast<wchar_t>(L'0' + m.target_y);
    g_history.push_back(item);

    bool check = g_chess.moveGivesCheck(m);
    g_chess.makeMoveAssumeLegal(m, true);
    g_selX = g_selY = -1;
    g_targets.clear();
    g_lastCheck = check;

    std::vector<Move> legal;
    g_chess.generateMovesWithForbidden(legal, true);
    if (legal.empty()) {
        finishGame(side == RED ? L"红方胜！" : L"黑方胜！");
    } else if (g_chess.exceedMaxPeaceState()) {
        finishGame(L"60 回合无吃子，双方言和");
    } else if (g_chess.positionRepeatCount() >= 3) {
        finishGame(L"三次重复局面，双方言和");
    } else {
        g_status = std::wstring(L"轮到") + (g_chess.currentColor() == RED ? L"红方" : L"黑方") + L"走棋";
        if (check) g_status += L"（将军）";
        maybeStartAI();
    }
}

static void newGame(int mode) {
    g_mode = mode;
    g_flip = (mode == 2);
    g_chess.resetBoard();
    g_history.clear();
    g_selX = g_selY = -1;
    g_targets.clear();
    g_gameOver = false;
    g_engineText.clear();
    g_lastCheck = false;
    g_status = L"轮到红方走棋";
    for (int i = 0; i < 4; ++i) EnableWindow(g_btn[i], TRUE);
    EnableWindow(g_btn[4], FALSE);             // “再来一局”仅终局后可用
    EnableWindow(g_btn[5], TRUE);              // “重置”随时可用
    InvalidateRect(g_hwnd, nullptr, TRUE);
    maybeStartAI();                            // 执黑时电脑（红方）先走
}

static void undoMove() {
    if (g_aiThinking) return;
    if (g_mode == 3) {
        if (g_history.empty()) return;
        g_chess.undoMove();
        g_history.pop_back();
    } else {
        if (g_history.size() < 2) return;      // 执黑且电脑仅走先手一步时不可悔
        g_chess.undoMove();                    // 撤电脑应手
        g_chess.undoMove();                    // 撤玩家上一步
        g_history.pop_back();
        g_history.pop_back();
    }
    g_gameOver = false;
    g_selX = g_selY = -1;
    g_targets.clear();
    g_lastCheck = false;
    EnableWindow(g_btn[4], FALSE);
    g_status = std::wstring(L"轮到") + (g_chess.currentColor() == RED ? L"红方" : L"黑方") + L"走棋";
    InvalidateRect(g_hwnd, nullptr, TRUE);
}

// ---------------- 鼠标走子 ----------------
static void onMouseDown(int px, int py) {
    if (g_gameOver || g_aiThinking) return;
    colorType player = (g_mode == 2) ? BLACK : RED;
    if (g_mode != 3 && g_chess.currentColor() != player) return;   // 轮到电脑

    int x, y;
    if (!pixelToCell(px, py, x, y)) { g_selX = g_selY = -1; g_targets.clear(); }
    else {
        Move chosen;
        bool isTarget = false;
        if (g_selX >= 0) {
            for (const Move& t : g_targets)
                if (t.source_x == g_selX && t.source_y == g_selY &&
                    t.target_x == x && t.target_y == y) { chosen = t; isTarget = true; break; }
        }
        if (isTarget) {
            applyMove(chosen);
        } else {
            Grid g = g_chess.getGrid(x, y);
            if (g.color != EMPTY && g.color == g_chess.currentColor()) {   // 选中己方子
                g_selX = x; g_selY = y;
                g_targets.clear();
                std::vector<Move> legal;
                g_chess.generateMovesWithForbidden(legal, true);
                for (const Move& t : legal)
                    if (t.source_x == x && t.source_y == y) g_targets.push_back(t);
            } else { g_selX = g_selY = -1; g_targets.clear(); }
        }
    }
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

// ---------------- 棋盘绘制 ----------------
static void drawPiece(HDC hdc, int px, int py, colorType color, int type) {
    COLORREF face = color == RED ? RGB(255, 241, 224) : RGB(238, 236, 228);
    COLORREF ring = color == RED ? RGB(172, 44, 32) : RGB(48, 44, 40);
    COLORREF text = color == RED ? RGB(178, 28, 18) : RGB(28, 28, 28);
    HBRUSH brush = CreateSolidBrush(face);
    HPEN penOuter = CreatePen(PS_SOLID, 2, ring);
    HPEN penInner = CreatePen(PS_SOLID, 1, ring);
    HBRUSH ob = static_cast<HBRUSH>(SelectObject(hdc, brush));
    HPEN op = static_cast<HPEN>(SelectObject(hdc, penOuter));
    Ellipse(hdc, px - PIECE_R, py - PIECE_R, px + PIECE_R + 1, py + PIECE_R + 1);
    SelectObject(hdc, penInner);
    int r = PIECE_R - 5;
    Ellipse(hdc, px - r, py - r, px + r + 1, py + r + 1);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(penOuter);
    DeleteObject(penInner);
    DeleteObject(brush);
    const wchar_t* s = color == RED ? PIECE_W_RED[type] : PIECE_W_BLACK[type];
    drawTextC(hdc, px, py + 1, s, text, g_fontPiece);
}

// 交叉点小角标记（炮位/兵位），边缘侧自动省略
static void drawCross(HDC hdc, int x, int y) {
    static const int d[4][2] = { {1,1},{1,-1},{-1,1},{-1,-1} };
    for (int i = 0; i < 4; ++i) {
        int nx = x + d[i][0], ny = y + d[i][1];
        if (nx < 0 || nx >= BOARDWIDTH || ny < 0 || ny >= BOARDHEIGHT) continue;
        int px = pxX(x) + d[i][0] * 6, py = pxY(y) + d[i][1] * 6;
        MoveToEx(hdc, px, py, nullptr); LineTo(hdc, px + d[i][0] * 7, py);
        MoveToEx(hdc, px, py, nullptr); LineTo(hdc, px, py + d[i][1] * 7);
    }
}

static void paintBoard(HDC hdc) {
    // 木色底
    RECT rb = { 0, 0, BOARD_W + 12, BOARD_H + 12 };
    HBRUSH wood = CreateSolidBrush(RGB(232, 202, 148));
    FillRect(hdc, &rb, wood);
    FrameRect(hdc, &rb, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    DeleteObject(wood);

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(90, 58, 26));
    HPEN open2 = static_cast<HPEN>(SelectObject(hdc, pen));
    // 横线 10 条
    for (int y = 0; y < BOARDHEIGHT; ++y) {
        MoveToEx(hdc, pxX(0), pxY(y), nullptr);
        LineTo(hdc, pxX(BOARDWIDTH - 1), pxY(y));
    }
    // 竖线：两侧贯通，中间楚河汉界断开
    for (int x = 0; x < BOARDWIDTH; ++x) {
        MoveToEx(hdc, pxX(x), pxY(0), nullptr);
        LineTo(hdc, pxX(x), pxY(4));
        MoveToEx(hdc, pxX(x), pxY(5), nullptr);
        LineTo(hdc, pxX(x), pxY(9));
        if (x == 0 || x == BOARDWIDTH - 1) {
            MoveToEx(hdc, pxX(x), pxY(4), nullptr);
            LineTo(hdc, pxX(x), pxY(5));
        }
    }
    // 九宫斜线
    struct { int x1, y1, x2, y2; } diag[4] = { {3,0,5,2},{5,0,3,2},{3,7,5,9},{5,7,3,9} };
    for (const auto& d : diag) {
        MoveToEx(hdc, pxX(d.x1), pxY(d.y1), nullptr);
        LineTo(hdc, pxX(d.x2), pxY(d.y2));
    }
    // 炮位与兵位标记
    drawCross(hdc, 1, 2); drawCross(hdc, 7, 2);
    drawCross(hdc, 1, 7); drawCross(hdc, 7, 7);
    for (int x = 0; x < BOARDWIDTH; x += 2) { drawCross(hdc, x, 3); drawCross(hdc, x, 6); }
    SelectObject(hdc, open2);
    DeleteObject(pen);

    // 楚河汉界
    drawTextC(hdc, (pxX(0) + pxX(3)) / 2, (pxY(4) + pxY(5)) / 2, L"楚    河", RGB(90, 58, 26), g_fontMark);
    drawTextC(hdc, (pxX(5) + pxX(8)) / 2, (pxY(4) + pxY(5)) / 2, L"漢    界", RGB(90, 58, 26), g_fontMark);

    // 最后一步落点标记（暗红小方块）
    if (!g_history.empty()) {
        HBRUSH b = CreateSolidBrush(RGB(178, 58, 40));
        const Move& lm = g_history.back().move;
        for (int i = 0; i < 2; ++i) {
            int px = i == 0 ? pxX(lm.source_x) : pxX(lm.target_x);
            int py = i == 0 ? pxY(lm.source_y) : pxY(lm.target_y);
            RECT r2 = { px - 4, py - 4, px + 4, py + 4 };
            FillRect(hdc, &r2, b);
        }
        DeleteObject(b);
    }

    // 选中高亮（金圈）
    if (g_selX >= 0) {
        HPEN gold = CreatePen(PS_SOLID, 3, RGB(226, 158, 16));
        HPEN o = static_cast<HPEN>(SelectObject(hdc, gold));
        HBRUSH ob = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        int px = pxX(g_selX), py = pxY(g_selY);
        Ellipse(hdc, px - PIECE_R - 3, py - PIECE_R - 3, px + PIECE_R + 4, py + PIECE_R + 4);
        SelectObject(hdc, o);
        SelectObject(hdc, ob);
        DeleteObject(gold);
    }

    // 棋子
    for (int x = 0; x < BOARDWIDTH; ++x)
        for (int y = 0; y < BOARDHEIGHT; ++y) {
            Grid g = g_chess.getGrid(x, y);
            if (g.color == EMPTY || g.type == None) continue;
            drawPiece(hdc, pxX(x), pxY(y), g.color, g.type);
        }

    // 合法落点：空格绿点 / 吃子红环
    for (const Move& t : g_targets) {
        Grid g = g_chess.getGrid(t.target_x, t.target_y);
        int px = pxX(t.target_x), py = pxY(t.target_y);
        if (g.color == EMPTY || g.type == None) {
            HBRUSH b = CreateSolidBrush(RGB(70, 158, 66));
            HBRUSH o = static_cast<HBRUSH>(SelectObject(hdc, b));
            HPEN op = static_cast<HPEN>(SelectObject(hdc, GetStockObject(NULL_PEN)));
            Ellipse(hdc, px - 9, py - 9, px + 9, py + 9);
            SelectObject(hdc, o);
            SelectObject(hdc, op);
            DeleteObject(b);
        } else {
            HPEN p = CreatePen(PS_SOLID, 3, RGB(196, 52, 40));
            HPEN o = static_cast<HPEN>(SelectObject(hdc, p));
            HBRUSH ob = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            Ellipse(hdc, px - PIECE_R - 4, py - PIECE_R - 4, px + PIECE_R + 5, py + PIECE_R + 5);
            SelectObject(hdc, o);
            SelectObject(hdc, ob);
            DeleteObject(p);
        }
    }
}

static void paintSide(HDC hdc, int x0) {
    int y = 156;                               // 避开按钮区（3 行按钮到 y=138）
    drawTextL(hdc, x0, y, L"中国象棋", RGB(70, 46, 20), g_fontTitle); y += 38;
    drawTextL(hdc, x0, y, g_status.c_str(),
              g_gameOver ? RGB(180, 30, 20) : RGB(30, 30, 30), g_fontUi); y += 30;
    if (g_lastCheck && !g_gameOver) {
        drawTextL(hdc, x0, y, L"将军！", RGB(200, 40, 30), g_fontUi); y += 28;
    }
    if (!g_engineText.empty()) {
        drawTextL(hdc, x0, y, g_engineText.c_str(), RGB(110, 110, 110), g_fontSmall); y += 24;
    }
    y += 8;
    drawTextL(hdc, x0, y, L"走子记录", RGB(70, 46, 20), g_fontUi); y += 26;
    int maxLines = (CLIENT_H - y - 10) / 20;
    int total = static_cast<int>(g_history.size());
    int start = total > maxLines ? total - maxLines : 0;
    for (int i = start; i < static_cast<int>(g_history.size()); ++i) {
        drawTextL(hdc, x0, y, g_history[i].text.c_str(), RGB(60, 60, 60), g_fontSmall);
        y += 20;
    }
}

static void paint(HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);
    RECT rc = { 0, 0, CLIENT_W, CLIENT_H };
    HBRUSH bg = CreateSolidBrush(RGB(238, 232, 218));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
    paintBoard(hdc);
    paintSide(hdc, BOARD_W + 24);
}

static void onPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(g_hwnd, &ps);
    HDC mem = CreateCompatibleDC(hdc);         // 双缓冲防闪烁
    HBITMAP bmp = CreateCompatibleBitmap(hdc, CLIENT_W, CLIENT_H);
    HBITMAP ob = static_cast<HBITMAP>(SelectObject(mem, bmp));
    paint(mem);
    BitBlt(hdc, 0, 0, CLIENT_W, CLIENT_H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(g_hwnd, &ps);
}

// ---------------- 消息处理 ----------------
static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        onPaint();
        return 0;
    case WM_LBUTTONDOWN:
        onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_AI_DONE: {
        AiResult* r = reinterpret_cast<AiResult*>(lParam);
        g_aiThinking = false;
        for (int i = 0; i < 4; ++i) EnableWindow(g_btn[i], TRUE);
        EnableWindow(g_btn[5], TRUE);          // “重置”恢复可用
        wchar_t buf[96];
        swprintf(buf, 96, L"AI: score=%d depth=%d nodes=%d %dms",
                 r->info.score, r->info.depth, static_cast<int>(r->info.nodes), static_cast<int>(r->info.elapsedMs));
        g_engineText = buf;
        if (!g_gameOver) applyMove(r->move);
        delete r;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 1001) newGame(1);
        else if (id == 1002) newGame(2);
        else if (id == 1003) newGame(3);
        else if (id == 1004) undoMove();
        else if (id == 1005) newGame(g_mode);   // 再来一局：按当前模式重开
        else if (id == 1006) newGame(g_mode);   // 重置：随时按当前模式重开
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void createButtons(HINSTANCE hInst) {
    struct { const wchar_t* text; int id; } defs[6] = {
        { L"人机·执红", 1001 }, { L"人机·执黑", 1002 }, { L"双人对战", 1003 },
        { L"悔  棋", 1004 }, { L"再来一局", 1005 }, { L"重  置", 1006 },
    };
    int x0 = BOARD_W + 24, w = 96, h = 30, gap = 12;
    for (int i = 0; i < 6; ++i) {
        int x = x0 + (i % 2) * (w + gap);
        int y = 24 + (i / 2) * (h + gap);      // 3 行 2 列
        g_btn[i] = CreateWindowExW(0, L"BUTTON", defs[i].text,
            WS_CHILD | WS_VISIBLE | (i == 4 ? WS_DISABLED : 0) | BS_PUSHBUTTON,
            x, y, w, h,
            g_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(defs[i].id)), hInst, nullptr);
        SendMessageW(g_btn[i], WM_SETFONT, reinterpret_cast<WPARAM>(g_fontUi), TRUE);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"ChessGuiWnd";
    wc.hbrBackground = CreateSolidBrush(RGB(238, 232, 218));
    RegisterClassW(&wc);

    RECT wr = { 0, 0, CLIENT_W, CLIENT_H };
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    g_fontPiece = CreateFontW(-36, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    g_fontTitle = CreateFontW(-22, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    g_fontUi = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    g_fontSmall = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
    g_fontMark = CreateFontW(-24, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");

    g_hwnd = CreateWindowExW(0, L"ChessGuiWnd", L"中国象棋",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             wr.right - wr.left, wr.bottom - wr.top,
                             nullptr, nullptr, hInst, nullptr);
    createButtons(hInst);
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

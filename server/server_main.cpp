// ============================================================
// 中国象棋本地对战服务
// Winsock 实现的最小 HTTP 服务器，复用 engine/ 引擎层：
//   GET  /                → web/index.html
//   GET  /style.css /app.js → 静态文件
//   GET  /api/state       → 当前局面 JSON
//   POST /api/new         → 新对局 {mode}
//   POST /api/move        → 人类走子 {from,to,mode}
//   POST /api/undo        → 悔棋 {mode}
//   POST /api/bestmove    → 引擎替当前方走棋 {mode}
// 编译: g++ -O2 -std=c++14 -Iengine server/server_main.cpp engine/chess.cpp
//       engine/search.cpp -o bin/chess_server.exe -lws2_32
// ============================================================

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>   // ShellExecuteA：启动后自动打开浏览器
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")   // MSVC 专用；MinGW 用命令行 -lws2_32
#endif
#else
// 为便于跨平台调试，也给出 POSIX 等价实现
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define closesocket close
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "chess.h"
#include "search.h"
#include "web_resources.h"   // build 时由 tools/gen_web_res.ps1 生成的内嵌网页资源

// ============================================================
// 一、极简 JSON 工具（只覆盖本服务需要的能力，不引入第三方库）
// ============================================================

// 字符串拼接式 JSON 输出器
class JsonOut {
public:
    std::string s;
    void beginObj() { s += "{"; }
    void endObj() { s += "}"; }
    void key(const char* k) {
        if (s.size() && s.back() != '{') s += ',';
        s += '"'; s += k; s += "\":";
    }
    void value(const std::string& v) {
        s += '"';
        for (char c : v) {
            if (c == '"' || c == '\\') s += '\\';
            s += c;
        }
        s += '"';
    }
    void value(const char* v) { value(std::string(v)); }
    void value(int v) { s += std::to_string(v); }
    void value(long long v) { s += std::to_string(v); }
    void value(bool v) { s += (v ? "true" : "false"); }
    void raw(const std::string& v) { s += v; }
};

// 从请求体中提取字符串字段（如 "from":"h2"），找不到返回 false
bool jsonFindString(const std::string& body, const char* key, std::string& out) {
    std::string pat = "\"" + std::string(key) + "\"";
    size_t p = body.find(pat);
    if (p == std::string::npos) return false;
    p = body.find(':', p + pat.size());
    if (p == std::string::npos) return false;
    ++p;
    while (p < body.size() && (body[p] == ' ' || body[p] == '\t')) ++p;
    if (p >= body.size() || body[p] != '"') return false;
    ++p;
    out.clear();
    while (p < body.size() && body[p] != '"') {
        if (body[p] == '\\' && p + 1 < body.size()) ++p;
        out += body[p++];
    }
    return true;
}

// ============================================================
// 二、对局会话：引擎 + 历史 + 模式（单线程，无需加锁）
// ============================================================

struct HistoryEntry {
    std::string piece;    // 走子方棋子编码，如 "rC"
    std::string from, to; // "h2" 坐标
    std::string capture;  // 被吃棋子编码，空串表示未吃子
    bool check;           // 是否形成将军
};

class GameSession {
public:
    Chess chess;
    SearchEngine engine;
    std::vector<HistoryEntry> history;
    std::string mode = "pve_red";
    SearchInfo lastEngineInfo;
    std::string lastEngineMove;

    GameSession() { chess.resetBoard(); }

    // mode → 人类执子颜色；pvp 返回 EMPTY
    static colorType humanColor(const std::string& m) {
        if (m == "pve_red") return RED;
        if (m == "pve_black") return BLACK;
        return EMPTY;
    }

    static std::string colorName(colorType c) { return c == RED ? "red" : "black"; }

    // 棋子 → 双字符编码（颜色 + 类型），与前端 PIECE_CHARS 对应
    static std::string pieceCode(const Grid& g) {
        if (g.type == None || g.color == EMPTY) return "";
        std::string code;
        code += (g.color == RED ? 'r' : 'b');
        switch (g.type) {
            case King:      code += 'K'; break;
            case Assistant: code += 'A'; break;
            case Bishop:    code += 'B'; break;
            case Knight:    code += 'N'; break;
            case Rook:      code += 'R'; break;
            case Pawn:      code += 'P'; break;
            case Cannon:    code += 'C'; break;
            default:        code += '?'; break;
        }
        return code;
    }

    static std::string sq(int x, int y) {
        std::string s;
        s += static_cast<char>('a' + x);
        s += static_cast<char>('0' + y);
        return s;
    }

    // 走子并记录历史（前提：move 已验证合法）
    void applyMove(const Move& m) {
        HistoryEntry e;
        e.piece = pieceCode(chess.getGrid(m.source_x, m.source_y));
        e.from = sq(m.source_x, m.source_y);
        e.to = sq(m.target_x, m.target_y);
        e.capture = pieceCode(chess.getGrid(m.target_x, m.target_y));
        e.check = chess.moveGivesCheck(m);
        chess.makeMoveAssumeLegal(m, true);
        history.push_back(e);
    }

    // 终局判定：轮到走棋的一方无合法着法 → 负；60 回合无吃子/重复局面 → 和
    std::string computeResult() {
        std::vector<Move> moves;
        chess.generateMovesWithForbidden(moves, true);
        if (moves.empty()) {
            return chess.currentColor() == RED ? "black_win" : "red_win";
        }
        if (chess.exceedMaxPeaceState()) return "draw_peace";
        if (chess.positionRepeatCount() >= 3) return "draw_repeat";
        return "";
    }

    // 输出 /api/state 的 state 对象
    std::string stateJson(const std::string& resultOverride = "") {
        std::vector<Move> legal;
        chess.generateMovesWithForbidden(legal, true);

        std::string result = resultOverride.empty() ? computeResult() : resultOverride;

        JsonOut o;
        o.beginObj();

        // 棋盘 90 格，顺序 y*9+x，与前端一致
        o.key("board");
        o.s += '[';
        for (int y = 0; y < BOARDHEIGHT; ++y) {
            for (int x = 0; x < BOARDWIDTH; ++x) {
                if (x || y) o.s += ',';
                o.value(pieceCode(chess.getGrid(x, y)));
            }
        }
        o.s += ']';

        o.key("current");  o.value(colorName(chess.currentColor()));
        o.key("inCheck");  o.value(chess.isKingAttacked(chess.currentColor()));
        o.key("result");   o.value(result);

        // 上一手
        o.key("lastMove");
        if (!history.empty()) {
            const HistoryEntry& h = history.back();
            o.beginObj();
            o.key("from"); o.value(h.from);
            o.key("to");   o.value(h.to);
            o.endObj();
        } else {
            o.raw("null");
        }

        // 当前方可行走法
        o.key("legalMoves");
        o.s += '[';
        for (size_t i = 0; i < legal.size(); ++i) {
            if (i) o.s += ',';
            o.beginObj();
            o.key("from"); o.value(sq(legal[i].source_x, legal[i].source_y));
            o.key("to");   o.value(sq(legal[i].target_x, legal[i].target_y));
            o.endObj();
        }
        o.s += ']';

        // 走子记录
        o.key("history");
        o.s += '[';
        for (size_t i = 0; i < history.size(); ++i) {
            if (i) o.s += ',';
            const HistoryEntry& h = history[i];
            o.beginObj();
            o.key("piece");   o.value(h.piece);
            o.key("from");    o.value(h.from);
            o.key("to");      o.value(h.to);
            o.key("capture"); o.value(h.capture);
            o.key("check");   o.value(h.check);
            o.endObj();
        }
        o.s += ']';

        // 最近一次引擎思考信息（供界面展示）
        if (!lastEngineMove.empty()) {
            o.key("engine");
            o.beginObj();
            o.key("move");   o.value(lastEngineMove);
            o.key("score");  o.value(lastEngineInfo.score);
            o.key("depth");  o.value(lastEngineInfo.depth);
            o.key("nodes");  o.value(lastEngineInfo.nodes);
            o.key("ms");     o.value(lastEngineInfo.elapsedMs);
            o.endObj();
        }

        o.endObj();
        return o.s;
    }

    // 包一层 {ok:..., state:...}
    std::string responseJson(bool ok, const std::string& message = "") {
        JsonOut o;
        o.beginObj();
        o.key("ok");    o.value(ok);
        if (!message.empty()) { o.key("message"); o.value(message); }
        o.key("state"); o.raw(stateJson());
        o.endObj();
        return o.s;
    }
};

// ============================================================
// 三、HTTP 基础设施
// ============================================================

std::string g_webDir = "web";

std::string mimeOf(const std::string& path) {
    if (path.size() >= 5 && path.rfind(".html") == path.size() - 5) return "text/html; charset=utf-8";
    if (path.size() >= 4 && path.rfind(".css") == path.size() - 4) return "text/css; charset=utf-8";
    if (path.size() >= 3 && path.rfind(".js") == path.size() - 3) return "application/javascript; charset=utf-8";
    return "application/octet-stream";
}

bool readFileText(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// 去掉查询串与开头的斜杠，并阻止 ../ 穿越
bool safeStaticPath(const std::string& url, std::string& fsPath) {
    std::string p = url;
    size_t q = p.find('?');
    if (q != std::string::npos) p = p.substr(0, q);
    if (p.empty() || p == "/") p = "/index.html";
    if (p.find("..") != std::string::npos) return false;
    fsPath = g_webDir + p;
    return true;
}

// 内嵌静态资源：磁盘 web 目录不存在时（发布单文件 exe）从这里取内容
const char* embeddedStatic(const std::string& path) {
    if (path == "/" || path == "/index.html") return INDEX_HTML;
    if (path == "/app.js") return APP_JS;
    if (path == "/style.css") return STYLE_CSS;
    return nullptr;
}

void sendAll(SOCKET client, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = static_cast<int>(send(client, data.data() + static_cast<int>(sent),
            static_cast<int>(data.size() - sent), 0));
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

void sendResponse(SOCKET client, int status, const std::string& contentType, const std::string& body) {
    std::ostringstream head;
    head << "HTTP/1.1 " << status << (status == 200 ? " OK" : " Error") << "\r\n"
         << "Content-Type: " << contentType << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Cache-Control: no-store\r\n"
         << "Connection: close\r\n\r\n";
    sendAll(client, head.str());
    sendAll(client, body);
}

// 读取一个完整请求（头 + 定长 body）
bool readRequest(SOCKET client, std::string& method, std::string& url, std::string& body) {
    std::string raw;
    char buf[4096];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        raw.append(buf, static_cast<size_t>(n));
        headerEnd = raw.find("\r\n\r\n");
        if (raw.size() > 65536) return false; // 防御异常请求
    }

    std::istringstream lines(raw.substr(0, headerEnd));
    std::string requestLine;
    std::getline(lines, requestLine);
    {
        size_t r = requestLine.find('\r');
        if (r != std::string::npos) requestLine.erase(r);
    }
    std::istringstream rl(requestLine);
    rl >> method >> url;

    // Content-Length
    size_t contentLen = 0;
    bool expectContinue = false;
    std::string line;
    while (std::getline(lines, line)) {
        size_t r = line.find('\r');
        if (r != std::string::npos) line.erase(r);
        std::istringstream ls(line);
        std::string k, v;
        ls >> k >> v;
        for (auto& c : k) c = static_cast<char>(tolower(c));
        for (auto& c : v) c = static_cast<char>(tolower(c));
        if (k == "content-length:") contentLen = static_cast<size_t>(std::atoi(v.c_str()));
        if (k == "expect:" && v == "100-continue") expectContinue = true;
    }
    if (contentLen > 65536) return false;

    // 客户端（如 .NET HttpWebRequest）发 Expect: 100-continue 时会等服务器应答后才发 body；
    // 若不应答，双方互等，单线程服务器将被永久堵死
    if (expectContinue && contentLen > 0) {
        sendAll(client, "HTTP/1.1 100 Continue\r\n\r\n");
    }

    body = raw.substr(headerEnd + 4);
    while (body.size() < contentLen) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) break;
        body.append(buf, static_cast<size_t>(n));
    }
    return true;
}

// ============================================================
// 四、API 路由处理
// ============================================================

GameSession g_game;

void handleApi(SOCKET client, const std::string& api, const std::string& body) {
    // --- 查询当前局面 ---
    if (api == "/api/state") {
        sendResponse(client, 200, "application/json; charset=utf-8", g_game.stateJson());
        return;
    }

    // --- 新对局 ---
    if (api == "/api/new") {
        std::string mode;
        jsonFindString(body, "mode", mode);
        if (mode.empty()) mode = "pve_red";
        g_game.mode = mode;
        g_game.chess.resetBoard();
        g_game.history.clear();
        g_game.lastEngineMove.clear();
        g_game.engine.setTimeLimitMs(DEFAULT_TIME_MS);
        std::cout << "[new] mode=" << mode << std::endl;
        sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(true));
        return;
    }

    // --- 人类走子 ---
    if (api == "/api/move") {
        std::string from, to;
        jsonFindString(body, "from", from);
        jsonFindString(body, "to", to);
        Move m(from, to);
        if (!validMoveObject(m)) {
            sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(false, "坐标无效"));
            return;
        }
        if (!g_game.chess.isLegalMoveWithForbidden(m, true)) {
            sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(false, "该走法不合法"));
            return;
        }
        // 人机模式下只允许人类执子方走棋，电脑的一手由 /api/bestmove 负责
        colorType human = GameSession::humanColor(g_game.mode);
        if (human != EMPTY && g_game.chess.currentColor() != human) {
            sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(false, "人机模式下轮到电脑走棋"));
            return;
        }
        g_game.applyMove(m);
        std::cout << "[move] " << from << "-" << to << std::endl;
        g_game.chess.printBoard();
        sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(true));
        return;
    }

    // --- 悔棋：人机模式撤销到"轮到人类"为止 ---
    if (api == "/api/undo") {
        colorType human = GameSession::humanColor(g_game.mode);
        int steps = 0;
        do {
            if (g_game.history.empty()) break;
            g_game.chess.undoMove();
            g_game.history.pop_back();
            ++steps;
        } while (human != EMPTY && g_game.chess.currentColor() != human);
        std::cout << "[undo] " << steps << " plies" << std::endl;
        sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(true));
        return;
    }

    // --- 引擎为当前行棋方走一步 ---
    if (api == "/api/bestmove") {
        // 人机模式下只有电脑执子方才能请求走棋，防止引擎替人类行棋
        colorType aiColor = EMPTY;
        if (g_game.mode == "pve_red") aiColor = BLACK;
        else if (g_game.mode == "pve_black") aiColor = RED;
        if (aiColor != EMPTY && g_game.chess.currentColor() != aiColor) {
            sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(false, "还没轮到电脑走棋"));
            return;
        }
        std::vector<Move> legal;
        g_game.chess.generateMovesWithForbidden(legal, true);
        if (legal.empty()) {
            sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(false, "无棋可走"));
            return;
        }
        SearchInfo info;
        Move m = g_game.engine.findBestMove(g_game.chess, info);
        // 双保险：source 格必须真的有当前行棋方的棋子，否则历史记录会写入空棋子编码
        if (!validMoveObject(m) || g_game.chess.getGrid(m.source_x, m.source_y).color != g_game.chess.currentColor()) {
            sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(false, "引擎返回了无效着法"));
            return;
        }
        std::string mvName = GameSession::sq(m.source_x, m.source_y) + "-" + GameSession::sq(m.target_x, m.target_y);
        g_game.lastEngineInfo = info;
        g_game.lastEngineMove = mvName;
        g_game.applyMove(m);
        std::cout << "[ai] " << mvName << " score=" << info.score << " depth=" << info.depth
                  << " nodes=" << info.nodes << " ms=" << info.elapsedMs << std::endl;
        g_game.chess.printBoard();
        sendResponse(client, 200, "application/json; charset=utf-8", g_game.responseJson(true));
        return;
    }

    sendResponse(client, 404, "application/json; charset=utf-8", "{\"ok\":false,\"message\":\"unknown api\"}");
}

void handleRequest(SOCKET client) {
    std::string method, url, body;
    if (!readRequest(client, method, url, body)) return;

    if (url.rfind("/api/", 0) == 0) {
        handleApi(client, url, body);
        return;
    }

    if (method == "GET") {
        // 磁盘 web 目录优先（开发调试方便改动），不存在则回退到内嵌资源（单文件发布）
        std::string fsPath;
        if (safeStaticPath(url, fsPath)) {
            std::string content;
            if (readFileText(fsPath, content)) {
                sendResponse(client, 200, mimeOf(fsPath), content);
                return;
            }
            const char* emb = embeddedStatic(url == "/" ? "/index.html" : fsPath.substr(g_webDir.size()));
            if (emb) {
                sendResponse(client, 200, mimeOf(fsPath), emb);
                return;
            }
        }
        sendResponse(client, 404, "text/plain; charset=utf-8", "404 Not Found");
        return;
    }

    sendResponse(client, 405, "text/plain; charset=utf-8", "Method Not Allowed");
}

// ============================================================
// 五、main：监听循环
// ============================================================

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001); // 控制台正确显示 UTF-8 中文
#endif
    int port = argc > 1 ? std::atoi(argv[1]) : 8080;
    if (port <= 0 || port > 65535) port = 8080;
    if (argc > 2) g_webDir = argv[2];

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }
#endif

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        std::cerr << "socket() failed" << std::endl;
        return 1;
    }

    // 允许快速重启复用端口
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 仅本机可访问
    addr.sin_port = htons(static_cast<u_short>(port));

    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed on port " << port << std::endl;
        return 1;
    }
    if (listen(listener, 8) == SOCKET_ERROR) {
        std::cerr << "listen() failed" << std::endl;
        return 1;
    }

    std::cout << "中国象棋服务已启动: http://localhost:" << port
              << "  (网页目录: " << g_webDir << ")" << std::endl;

#ifdef _WIN32
    // 双击 exe 即用：自动打开默认浏览器
    {
        std::string url = "http://localhost:" + std::to_string(port);
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
#endif

    while (true) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        // 半吊子请求（如 Expect: 100-continue 互等）最多等 5 秒即断开，避免堵死单线程服务
        DWORD rcvTimeoutMs = 5000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&rcvTimeoutMs), sizeof(rcvTimeoutMs));
        // 单线程顺序处理：引擎思考期间不接收新请求（本地单用户场景足够）
        handleRequest(client);
        closesocket(client);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

// Botzone 平台入口：交互协议部分（从 ChineseChess.cpp 原样移植，逻辑未改）
// 本地编译验证时用 third_party/jsoncpp_stub 桩替代平台的 jsoncpp。

#include "chess.h"
#include "search.h"
#include "jsoncpp/json.h"

using std::cin;
using std::cout;
using std::string;
using std::vector;

void getInputBotzone(Chess& chess);
void giveOutputBotzone(Chess& chess);

int main() {
    Chess chess;
    getInputBotzone(chess);
    giveOutputBotzone(chess);
    return 0;
}

namespace {

    int parseTimeLimitMs(const Json::Value& input) {
        if (!input.isMember("time_limit")) return DEFAULT_TIME_MS;
        const Json::Value& v = input["time_limit"];
        int raw = 0;
        if (v.isInt()) raw = v.asInt();
        else if (v.isUInt()) raw = static_cast<int>(v.asUInt());
        else if (v.isString()) raw = std::atoi(v.asString().c_str());
        if (raw <= 0) return DEFAULT_TIME_MS;
        int budget = raw;
        if (budget >= 100) budget = std::min(budget - 70, budget * 80 / 100);
        else budget = std::max(30, budget - 10);
        return clampInt(budget, 30, 5000);
    }

    bool isPassMoveString(const string& source) {
        return source == "-1";
    }

    void applyJsonMove(Chess& chess, const Json::Value& obj) {
        if (!obj.isObject()) return;
        string source = obj["source"].asString();
        string target = obj["target"].asString();
        if (isPassMoveString(source)) return;
        Move move(source, target);
        if (validMoveObject(move)) chess.makeMoveAssumeLegal(move, true);
    }

} // namespace

void getInputBotzone(Chess& chess) {
    string str;
    if (!std::getline(cin, str)) return;
    Json::Reader reader;
    Json::Value input;
    if (!reader.parse(str, input)) return;
    GLOBAL_TIME_LIMIT_MS = parseTimeLimitMs(input);

    int turnID = 0;
    if (input.isMember("responses") && input["responses"].isArray()) {
        turnID = static_cast<int>(input["responses"].size());
    }

    for (int i = 0; i < turnID; ++i) {
        if (input.isMember("requests") && input["requests"].isArray() && i < static_cast<int>(input["requests"].size())) {
            applyJsonMove(chess, input["requests"][i]);
        }
        applyJsonMove(chess, input["responses"][i]);
    }

    if (input.isMember("requests") && input["requests"].isArray() && turnID < static_cast<int>(input["requests"].size())) {
        applyJsonMove(chess, input["requests"][turnID]);
    }
}

void giveOutputBotzone(Chess& chess) {
    vector<Move> retMoves;
    chess.generateMovesWithForbidden(retMoves, true);
    Json::Value ret;

    if (retMoves.empty()) {
        ret["response"]["source"] = string("-1");
        ret["response"]["target"] = string("-1");
    }
    else {
        SearchEngine engine;
        Move selMove = engine.findBestMove(chess);
        if (!validMoveObject(selMove) || std::find(retMoves.begin(), retMoves.end(), selMove) == retMoves.end()) {
            selMove = retMoves[0];
        }
        ret["response"]["source"] = string(1, pgnint2char(selMove.source_x)) + string(1, int2char(selMove.source_y));
        ret["response"]["target"] = string(1, pgnint2char(selMove.target_x)) + string(1, int2char(selMove.target_y));
    }

    Json::FastWriter writer;
    cout << writer.write(ret) << std::endl;
}

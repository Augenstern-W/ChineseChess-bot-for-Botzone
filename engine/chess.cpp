#include "chess.h"

#include <algorithm>
#include <cmath>
#include <iostream>

// 方向偏移表定义
const int dx_ob[4] = { -1, 1, -1, 1 };
const int dy_ob[4] = { -1, -1, 1, 1 };
const int dx_strai[4] = { -1, 0, 0, 1 };
const int dy_strai[4] = { 0, -1, 1, 0 };
const int dx_lr[2] = { -1, 1 };
const int dx_knight[8] = { -2, -2, -1, -1, 1, 1, 2, 2 };
const int dy_knight[8] = { -1, 1, -2, 2, -2, 2, -1, 1 };
const int dx_knight_foot[8] = { -1, -1, 0, 0, 0, 0, 1, 1 };
const int dy_knight_foot[8] = { 0, 0, -1, 1, -1, 1, 0, 0 };
const int dx_bishop[4] = { -2, -2, 2, 2 };
const int dy_bishop[4] = { -2, 2, -2, 2 };
const int dx_bishop_eye[4] = { -1, -1, 1, 1 };
const int dy_bishop_eye[4] = { -1, 1, -1, 1 };

int pgnchar2int(char c) {
    return static_cast<int>(c) - static_cast<int>('a');
}

char pgnint2char(int i) {
    return static_cast<char>(static_cast<int>('a') + i);
}

int char2int(char c) {
    return static_cast<int>(c) - static_cast<int>('0');
}

char int2char(int i) {
    return static_cast<char>(static_cast<int>('0') + i);
}

namespace {

    const int PEACE_LIMIT = 59;
    const int MAX_CONSECUTIVE_QUIET_CHECKS = 2;

    uint64_t ZOBRIST[2][8][BOARDWIDTH * BOARDHEIGHT];
    uint64_t ZOBRIST_SIDE = 0;
    uint64_t ZOBRIST_PEACE[64];
    bool ZOBRIST_READY = false;

    uint64_t splitMix64(uint64_t& x) {
        x += 0x9e3779b97f4a7c15ULL;
        uint64_t z = x;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    void initZobrist() {
        if (ZOBRIST_READY) return;
        uint64_t seed = 202605130971ULL;
        for (int c = 0; c < 2; ++c) {
            for (int t = 0; t < 8; ++t) {
                for (int p = 0; p < BOARDWIDTH * BOARDHEIGHT; ++p) {
                    ZOBRIST[c][t][p] = splitMix64(seed);
                }
            }
        }
        ZOBRIST_SIDE = splitMix64(seed);
        for (int i = 0; i < 64; ++i) {
            ZOBRIST_PEACE[i] = splitMix64(seed);
        }
        ZOBRIST_READY = true;
    }

} // namespace

bool validMoveObject(const Move& move) {
    return move.source_x >= 0 && move.source_x < BOARDWIDTH && move.source_y >= 0 && move.source_y < BOARDHEIGHT &&
        move.target_x >= 0 && move.target_x < BOARDWIDTH && move.target_y >= 0 && move.target_y < BOARDHEIGHT;
}

int pieceValue(stoneType type) {
    switch (type) {
    case King: return 20000;
    case Rook: return 1000;
    case Cannon: return 500;
    case Knight: return 450;
    case Pawn: return 105;
    case Bishop: return 125;
    case Assistant: return 125;
    default: return 0;
    }
}

int relativeY(colorType color, int y) {
    return color == RED ? y : (BOARDHEIGHT - 1 - y);
}

bool crossedRiver(colorType color, int y) {
    return color == RED ? (y > 4) : (y < 5);
}

int centerScore(int x) {
    return 4 - std::abs(x - 4);
}

int clampInt(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

colorType oppositeColor(colorType color) {
    return color == RED ? BLACK : RED;
}

int colorIndex(colorType color) {
    return color == RED ? 1 : 0;
}

Chess::Chess() {
    resetBoard();
}

void Chess::resetBoard() {
    initZobrist();
    for (size_t i = 0; i < board.size(); ++i) board[i] = Grid();
    currColor = RED;
    currTurnId = 0;
    peaceCount = 0;
    kingPos[0] = -1;
    kingPos[1] = -1;
    boardHash = 0;
    undoStack.clear();
    undoStack.reserve(256);
    stateKeys.clear();
    lastMoveEaten.clear();
    lastMoveChecked.clear();

    auto put = [&](int x, int y, stoneType type, colorType color) {
        int pos = xy2pos(x, y);
        board[pos] = Grid(type, color);
        boardHash ^= ZOBRIST[colorIndex(color)][type][pos];
        if (type == King) kingPos[colorIndex(color)] = pos;
        };

    put(0, 0, Rook, RED);      put(1, 0, Knight, RED);    put(2, 0, Bishop, RED);
    put(3, 0, Assistant, RED); put(4, 0, King, RED);      put(5, 0, Assistant, RED);
    put(6, 0, Bishop, RED);    put(7, 0, Knight, RED);    put(8, 0, Rook, RED);
    put(1, 2, Cannon, RED);    put(7, 2, Cannon, RED);
    put(0, 3, Pawn, RED);      put(2, 3, Pawn, RED);      put(4, 3, Pawn, RED);
    put(6, 3, Pawn, RED);      put(8, 3, Pawn, RED);

    put(0, 9, Rook, BLACK);      put(1, 9, Knight, BLACK);    put(2, 9, Bishop, BLACK);
    put(3, 9, Assistant, BLACK); put(4, 9, King, BLACK);      put(5, 9, Assistant, BLACK);
    put(6, 9, Bishop, BLACK);    put(7, 9, Knight, BLACK);    put(8, 9, Rook, BLACK);
    put(1, 7, Cannon, BLACK);    put(7, 7, Cannon, BLACK);
    put(0, 6, Pawn, BLACK);      put(2, 6, Pawn, BLACK);      put(4, 6, Pawn, BLACK);
    put(6, 6, Pawn, BLACK);      put(8, 6, Pawn, BLACK);

    stateKeys.push_back(boardHash);
    lastMoveEaten.push_back(0);
    lastMoveChecked.push_back(0);
}

bool Chess::inBoard(int mx, int my) {
    return mx >= 0 && mx < BOARDWIDTH && my >= 0 && my < BOARDHEIGHT;
}

bool Chess::inKingArea(int mx, int my, colorType mcolor) {
    if (mcolor == RED) return mx >= 3 && mx <= 5 && my >= 0 && my <= 2;
    if (mcolor == BLACK) return mx >= 3 && mx <= 5 && my >= 7 && my <= 9;
    return false;
}

bool Chess::inColorArea(int mx, int my, colorType mcolor) {
    (void)mx;
    if (mcolor == RED) return my <= 4;
    if (mcolor == BLACK) return my >= 5;
    return false;
}

colorType Chess::oppColor() const {
    return currColor == RED ? BLACK : RED;
}

colorType Chess::oppColor(colorType mcolor) {
    return mcolor == RED ? BLACK : RED;
}

int Chess::xy2pos(int mx, int my) {
    return my * BOARDWIDTH + mx;
}

int Chess::pos2x(int mpos) {
    return mpos % BOARDWIDTH;
}

int Chess::pos2y(int mpos) {
    return mpos / BOARDWIDTH;
}

Grid Chess::getGrid(int mx, int my) const {
    if (!inBoard(mx, my)) return Grid();
    return board[xy2pos(mx, my)];
}

Grid Chess::getGridAt(int pos) const {
    if (pos < 0 || pos >= BOARDWIDTH * BOARDHEIGHT) return Grid();
    return board[pos];
}

colorType Chess::currentColor() const {
    return currColor;
}

int Chess::currentTurn() const {
    return currTurnId;
}

int Chess::peaceTurn() const {
    return peaceCount;
}

uint64_t Chess::positionKey() const {
    return boardHash;
}

uint64_t Chess::searchKey() const {
    uint64_t key = boardHash;
    if (currColor == BLACK) key ^= ZOBRIST_SIDE;
    key ^= ZOBRIST_PEACE[clampInt(peaceCount, 0, 63)];
    return key;
}

stoneType Chess::sourceType(const Move& move) const {
    if (!inBoard(move.source_x, move.source_y)) return None;
    return board[xy2pos(move.source_x, move.source_y)].type;
}

stoneType Chess::targetType(const Move& move) const {
    if (!inBoard(move.target_x, move.target_y)) return None;
    return board[xy2pos(move.target_x, move.target_y)].type;
}

bool Chess::moveIsCapture(const Move& move) const {
    if (!inBoard(move.target_x, move.target_y)) return false;
    const Grid& target = board[xy2pos(move.target_x, move.target_y)];
    return target.color != EMPTY && target.color != currColor;
}

bool Chess::hasKing(colorType mcolor) const {
    int pos = kingPos[colorIndex(mcolor)];
    return pos >= 0 && pos < BOARDWIDTH * BOARDHEIGHT && board[pos].type == King && board[pos].color == mcolor;
}

bool Chess::makeMoveAssumeLegal(const Move& move, bool recordCheck) {
    if (!validMoveObject(move)) return false;
    int source = xy2pos(move.source_x, move.source_y);
    int target = xy2pos(move.target_x, move.target_y);
    if (source == target) return false;
    Grid moved = board[source];
    Grid captured = board[target];
    if (moved.color != currColor || moved.type == None) return false;
    if (captured.color == currColor) return false;

    UndoRecord record;
    record.move = move;
    record.moved = moved;
    record.captured = captured;
    record.prevColor = currColor;
    record.prevTurnId = currTurnId;
    record.prevPeace = peaceCount;
    record.prevKingPos[0] = kingPos[0];
    record.prevKingPos[1] = kingPos[1];
    record.prevHash = boardHash;
    undoStack.push_back(record);

    boardHash ^= ZOBRIST[colorIndex(moved.color)][moved.type][source];
    if (captured.color != EMPTY && captured.type != None) {
        boardHash ^= ZOBRIST[colorIndex(captured.color)][captured.type][target];
    }

    board[target] = moved;
    board[source] = Grid();
    boardHash ^= ZOBRIST[colorIndex(moved.color)][moved.type][target];

    if (moved.type == King) kingPos[colorIndex(moved.color)] = target;
    if (captured.type == King && captured.color != EMPTY) kingPos[colorIndex(captured.color)] = -1;

    peaceCount = (captured.color != EMPTY) ? 0 : (peaceCount + 1);
    bool givesCheck = recordCheck && isKingAttacked(oppColor(currColor));
    ++currTurnId;
    lastMoveEaten.push_back(captured.color != EMPTY ? 1 : 0);
    lastMoveChecked.push_back(givesCheck ? 1 : 0);
    stateKeys.push_back(boardHash);
    currColor = oppColor(currColor);
    return true;
}

void Chess::undoMove() {
    if (undoStack.empty()) return;
    UndoRecord record = undoStack.back();
    undoStack.pop_back();

    int source = xy2pos(record.move.source_x, record.move.source_y);
    int target = xy2pos(record.move.target_x, record.move.target_y);
    board[source] = record.moved;
    board[target] = record.captured;
    currColor = record.prevColor;
    currTurnId = record.prevTurnId;
    peaceCount = record.prevPeace;
    kingPos[0] = record.prevKingPos[0];
    kingPos[1] = record.prevKingPos[1];
    boardHash = record.prevHash;
    if (!stateKeys.empty()) stateKeys.pop_back();
    if (!lastMoveEaten.empty()) lastMoveEaten.pop_back();
    if (!lastMoveChecked.empty()) lastMoveChecked.pop_back();
}

bool Chess::attacked(colorType color, int mx, int my) const {
    if (!inBoard(mx, my) || color == EMPTY) return false;

    for (int dir = 0; dir < 4; ++dir) {
        int blockers = 0;
        int tx = mx + dx_strai[dir];
        int ty = my + dy_strai[dir];
        while (inBoard(tx, ty)) {
            Grid g = getGrid(tx, ty);
            if (g.color != EMPTY) {
                if (blockers == 0) {
                    if (g.color == color && g.type == Rook) return true;
                    if (g.color == color && g.type == King && tx == mx) return true;
                }
                else if (blockers == 1) {
                    if (g.color == color && g.type == Cannon) return true;
                    break;
                }
                ++blockers;
                if (blockers > 1) break;
            }
            tx += dx_strai[dir];
            ty += dy_strai[dir];
        }
    }

    for (int dir = 0; dir < 8; ++dir) {
        int hx = mx - dx_knight[dir];
        int hy = my - dy_knight[dir];
        if (!inBoard(hx, hy)) continue;
        Grid h = getGrid(hx, hy);
        if (h.color != color || h.type != Knight) continue;
        int fx = hx + dx_knight_foot[dir];
        int fy = hy + dy_knight_foot[dir];
        if (inBoard(fx, fy) && getGrid(fx, fy).color == EMPTY) return true;
    }

    int py = (color == RED) ? (my - 1) : (my + 1);
    if (inBoard(mx, py)) {
        Grid p = getGrid(mx, py);
        if (p.color == color && p.type == Pawn) return true;
    }
    for (int side = 0; side < 2; ++side) {
        int px = mx + dx_lr[side];
        if (!inBoard(px, my)) continue;
        Grid p = getGrid(px, my);
        if (p.color == color && p.type == Pawn && crossedRiver(color, my)) return true;
    }

    if (inKingArea(mx, my, color)) {
        for (int dir = 0; dir < 4; ++dir) {
            int ax = mx - dx_ob[dir];
            int ay = my - dy_ob[dir];
            if (!inBoard(ax, ay)) continue;
            Grid a = getGrid(ax, ay);
            if (a.color == color && a.type == Assistant) return true;
        }
    }

    if (inColorArea(mx, my, color)) {
        for (int dir = 0; dir < 4; ++dir) {
            int bx = mx - dx_bishop[dir];
            int by = my - dy_bishop[dir];
            int ex = bx + dx_bishop_eye[dir];
            int ey = by + dy_bishop_eye[dir];
            if (!inBoard(bx, by) || !inBoard(ex, ey)) continue;
            Grid b = getGrid(bx, by);
            if (b.color == color && b.type == Bishop && getGrid(ex, ey).color == EMPTY) return true;
        }
    }

    return false;
}

bool Chess::isKingAttacked(colorType side) const {
    int pos = kingPos[colorIndex(side)];
    if (pos < 0) return true;
    return attacked(oppColor(side), pos2x(pos), pos2y(pos));
}

bool Chess::currentKingAttacked() const {
    return isKingAttacked(currColor);
}

void Chess::generateMoves(std::vector<Move>& legalMoves, bool mustDefend, bool capturesOnly) {
    legalMoves.clear();
    colorType side = currColor;
    colorType enemy = oppColor(side);

    auto tryAddMove = [&](int sx, int sy, int tx, int ty) {
        if (!inBoard(tx, ty)) return;
        Grid target = getGrid(tx, ty);
        if (target.color == side) return;
        if (capturesOnly && target.color != enemy) return;
        Move move(sx, sy, tx, ty);
        if (mustDefend) {
            if (!makeMoveAssumeLegal(move)) return;
            bool bad = isKingAttacked(side);
            undoMove();
            if (bad) return;
        }
        legalMoves.push_back(move);
        };

    for (int y = 0; y < BOARDHEIGHT; ++y) {
        for (int x = 0; x < BOARDWIDTH; ++x) {
            Grid curGrid = getGrid(x, y);
            if (curGrid.color != side) continue;
            switch (curGrid.type) {
            case King: {
                for (int dir = 0; dir < 4; ++dir) {
                    int tx = x + dx_strai[dir];
                    int ty = y + dy_strai[dir];
                    if (inKingArea(tx, ty, curGrid.color)) tryAddMove(x, y, tx, ty);
                }
                break;
            }
            case Assistant: {
                for (int dir = 0; dir < 4; ++dir) {
                    int tx = x + dx_ob[dir];
                    int ty = y + dy_ob[dir];
                    if (inKingArea(tx, ty, curGrid.color)) tryAddMove(x, y, tx, ty);
                }
                break;
            }
            case Bishop: {
                for (int dir = 0; dir < 4; ++dir) {
                    int tx = x + dx_bishop[dir];
                    int ty = y + dy_bishop[dir];
                    int ex = x + dx_bishop_eye[dir];
                    int ey = y + dy_bishop_eye[dir];
                    if (inBoard(tx, ty) && inBoard(ex, ey) && inColorArea(tx, ty, curGrid.color) &&
                        getGrid(ex, ey).color == EMPTY) {
                        tryAddMove(x, y, tx, ty);
                    }
                }
                break;
            }
            case Knight: {
                for (int dir = 0; dir < 8; ++dir) {
                    int tx = x + dx_knight[dir];
                    int ty = y + dy_knight[dir];
                    int fx = x + dx_knight_foot[dir];
                    int fy = y + dy_knight_foot[dir];
                    if (inBoard(tx, ty) && inBoard(fx, fy) && getGrid(fx, fy).color == EMPTY) {
                        tryAddMove(x, y, tx, ty);
                    }
                }
                break;
            }
            case Rook: {
                for (int dir = 0; dir < 4; ++dir) {
                    int tx = x + dx_strai[dir];
                    int ty = y + dy_strai[dir];
                    while (inBoard(tx, ty)) {
                        Grid target = getGrid(tx, ty);
                        if (target.color == side) break;
                        tryAddMove(x, y, tx, ty);
                        if (target.color == enemy) break;
                        tx += dx_strai[dir];
                        ty += dy_strai[dir];
                    }
                }
                break;
            }
            case Cannon: {
                for (int dir = 0; dir < 4; ++dir) {
                    bool hasScreen = false;
                    int tx = x + dx_strai[dir];
                    int ty = y + dy_strai[dir];
                    while (inBoard(tx, ty)) {
                        Grid target = getGrid(tx, ty);
                        if (!hasScreen) {
                            if (target.color == EMPTY) {
                                if (!capturesOnly) tryAddMove(x, y, tx, ty);
                            }
                            else {
                                hasScreen = true;
                            }
                        }
                        else {
                            if (target.color != EMPTY) {
                                if (target.color == enemy) tryAddMove(x, y, tx, ty);
                                break;
                            }
                        }
                        tx += dx_strai[dir];
                        ty += dy_strai[dir];
                    }
                }
                break;
            }
            case Pawn: {
                int forward = (curGrid.color == RED) ? 1 : -1;
                tryAddMove(x, y, x, y + forward);
                if (crossedRiver(curGrid.color, y)) {
                    tryAddMove(x, y, x - 1, y);
                    tryAddMove(x, y, x + 1, y);
                }
                break;
            }
            default:
                break;
            }
        }
    }
}

void Chess::generateCaptureMoves(std::vector<Move>& legalMoves, bool mustDefend) {
    generateMoves(legalMoves, mustDefend, true);
}

bool Chess::repeatAfterMove(const Move& move) {
    if (!makeMoveAssumeLegal(move)) return true;
    int idNow = currTurnId;
    bool isCapture = !lastMoveEaten.empty() && lastMoveEaten[idNow] != 0;
    if (isCapture) {
        undoMove();
        return false;
    }

    int start = idNow;
    while (start > 1) {
        if (lastMoveEaten[start - 2]) break;
        start -= 2;
    }

    int repeatTimes = 0;
    uint64_t key = stateKeys[idNow];
    for (int id = idNow; id >= start; id -= 2) {
        if (stateKeys[id] == key) ++repeatTimes;
    }
    undoMove();
    return repeatTimes >= 3;
}

void Chess::generateMovesWithForbidden(std::vector<Move>& legalMoves, bool mustDefend) {
    std::vector<Move> firstMoves;
    generateMoves(firstMoves, mustDefend, false);
    legalMoves.clear();
    legalMoves.reserve(firstMoves.size());
    for (size_t i = 0; i < firstMoves.size(); ++i) {
        if (!repeatAfterMove(firstMoves[i]) && !longCheckAfterMove(firstMoves[i])) legalMoves.push_back(firstMoves[i]);
    }
}

bool Chess::isLegalMove(const Move& move, bool mustDefend) {
    if (!validMoveObject(move)) return false;
    std::vector<Move> moves;
    generateMoves(moves, mustDefend, false);
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

bool Chess::isMoveValid(const Move& move, bool mustDefend) {
    return isLegalMove(move, mustDefend);
}

bool Chess::isLegalMoveWithForbidden(const Move& move, bool mustDefend) {
    if (!validMoveObject(move)) return false;
    std::vector<Move> moves;
    generateMovesWithForbidden(moves, mustDefend);
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}

bool Chess::isMoveValidWithForbidden(const Move& move, bool mustDefend) {
    return isLegalMoveWithForbidden(move, mustDefend);
}

bool Chess::isMyKingAttackedAfterMove(const Move& move) {
    colorType side = currColor;
    if (!makeMoveAssumeLegal(move)) return true;
    bool attackedAfter = isKingAttacked(side);
    undoMove();
    return attackedAfter;
}

bool Chess::isOppKingAttackedAfterMove(const Move& move) {
    colorType enemy = oppColor(currColor);
    if (!makeMoveAssumeLegal(move)) return false;
    bool attackedAfter = isKingAttacked(enemy);
    undoMove();
    return attackedAfter;
}

bool Chess::moveGivesCheck(const Move& move) {
    if (!makeMoveAssumeLegal(move)) return false;
    bool givesCheck = currentKingAttacked();
    undoMove();
    return givesCheck;
}

int Chess::quietCheckStreakForCurrentSide() const {
    int streak = 0;
    for (int id = currTurnId - 1; id >= 1; id -= 2) {
        if (id >= static_cast<int>(lastMoveEaten.size()) || id >= static_cast<int>(lastMoveChecked.size())) break;
        if (lastMoveEaten[id] || !lastMoveChecked[id]) break;
        ++streak;
    }
    return streak;
}

bool Chess::longCheckAfterMove(const Move& move) {
    if (moveIsCapture(move)) return false;
    if (!moveGivesCheck(move)) return false;
    if (winAfterMove(move)) return false;
    return quietCheckStreakForCurrentSide() >= MAX_CONSECUTIVE_QUIET_CHECKS;
}

bool Chess::winAfterMove(const Move& move) {
    if (!makeMoveAssumeLegal(move)) return false;
    bool win = !hasKing(currColor);
    if (!win) {
        std::vector<Move> moves;
        generateMoves(moves, true, false);
        win = moves.empty();
    }
    undoMove();
    return win;
}

bool Chess::exceedMaxPeaceState() const {
    return peaceCount >= PEACE_LIMIT;
}

int Chess::positionRepeatCount() const {
    if (stateKeys.empty()) return 0;
    uint64_t key = stateKeys.back();
    int count = 0;
    for (int id = static_cast<int>(stateKeys.size()) - 1; id >= 0; id -= 2) {
        if (stateKeys[id] == key) ++count;
    }
    return count;
}

void Chess::printBoard() const {
    static const char sym[] = "*kbnrpca";
    for (int y = BOARDHEIGHT - 1; y >= 0; --y) {
        for (int x = 0; x < BOARDWIDTH; ++x) {
            Grid g = getGrid(x, y);
            char c = sym[g.type];
            if (g.color == RED && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            std::cerr << c << ' ';
        }
        std::cerr << '\n';
    }
}

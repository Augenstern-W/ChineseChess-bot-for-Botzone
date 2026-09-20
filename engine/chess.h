#ifndef CHINESE_CHESS_CHESS_H
#define CHINESE_CHESS_CHESS_H

// 引擎层：棋盘表示与规则（从 ChineseChess.cpp 拆分而来）
// 与平台无关，不依赖 jsoncpp；Botzone 提交版与本地服务共用。

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#define BOARDWIDTH 9
#define BOARDHEIGHT 10

enum stoneType { None = 0, King = 1, Bishop = 2, Knight = 3, Rook = 4, Pawn = 5, Cannon = 6, Assistant = 7 };
enum colorType { BLACK = 0, RED = 1, EMPTY = 2 };

// 方向偏移表（走法生成 / 攻击检测 / 评估共用），定义在 chess.cpp
extern const int dx_ob[4];            // 斜向（士）
extern const int dy_ob[4];
extern const int dx_strai[4];         // 直向（帅/车/炮）
extern const int dy_strai[4];
extern const int dx_lr[2];            // 左右（过河兵横走）
extern const int dx_knight[8];        // 马
extern const int dy_knight[8];
extern const int dx_knight_foot[8];   // 马腿
extern const int dy_knight_foot[8];
extern const int dx_bishop[4];        // 象
extern const int dy_bishop[4];
extern const int dx_bishop_eye[4];    // 象眼
extern const int dy_bishop_eye[4];

// 坐标与记谱转换："a0"~"i9"
int pgnchar2int(char c);
char pgnint2char(int i);
int char2int(char c);
char int2char(int i);

struct Move {
    int source_x, source_y, target_x, target_y;
    Move() : source_x(-1), source_y(-1), target_x(-1), target_y(-1) {}
    Move(int msx, int msy, int mtx, int mty)
        : source_x(msx), source_y(msy), target_x(mtx), target_y(mty) {
    }
    Move(const std::string& msource, const std::string& mtarget)
        : source_x(-1), source_y(-1), target_x(-1), target_y(-1) {
        if (msource.size() >= 2 && mtarget.size() >= 2 && msource[0] >= 'a' && msource[0] <= 'i' &&
            msource[1] >= '0' && msource[1] <= '9' && mtarget[0] >= 'a' && mtarget[0] <= 'i' &&
            mtarget[1] >= '0' && mtarget[1] <= '9') {
            source_x = pgnchar2int(msource[0]);
            source_y = char2int(msource[1]);
            target_x = pgnchar2int(mtarget[0]);
            target_y = char2int(mtarget[1]);
        }
    }
    bool operator==(const Move& other) const {
        return source_x == other.source_x && source_y == other.source_y &&
            target_x == other.target_x && target_y == other.target_y;
    }
    bool operator!=(const Move& other) const {
        return !(*this == other);
    }
};

struct Grid {
    stoneType type;
    colorType color;
    Grid() : type(None), color(EMPTY) {}
    Grid(stoneType mtype, colorType mcolor) : type(mtype), color(mcolor) {}
    bool operator==(const Grid& other) const {
        return type == other.type && color == other.color;
    }
};

// 通用小工具（定义在 chess.cpp）
bool validMoveObject(const Move& move);
int pieceValue(stoneType type);
int relativeY(colorType color, int y);
bool crossedRiver(colorType color, int y);
int centerScore(int x);
int clampInt(int v, int lo, int hi);
colorType oppositeColor(colorType color);
int colorIndex(colorType color);

class Chess {
private:
    std::array<Grid, BOARDWIDTH* BOARDHEIGHT> board;
    colorType currColor;
    int currTurnId;
    int peaceCount;
    int kingPos[2];
    uint64_t boardHash;

    struct UndoRecord {
        Move move;
        Grid moved;
        Grid captured;
        colorType prevColor;
        int prevTurnId;
        int prevPeace;
        int prevKingPos[2];
        uint64_t prevHash;
    };

    std::vector<UndoRecord> undoStack;
    std::vector<uint64_t> stateKeys;
    std::vector<int> lastMoveEaten;
    std::vector<int> lastMoveChecked;

    int quietCheckStreakForCurrentSide() const;

public:
    Chess();
    void resetBoard();
    void generateMoves(std::vector<Move>& legalMoves, bool mustDefend = true, bool capturesOnly = false);
    void generateCaptureMoves(std::vector<Move>& legalMoves, bool mustDefend = true);
    void generateMovesWithForbidden(std::vector<Move>& legalMoves, bool mustDefend = true);
    bool repeatAfterMove(const Move& move);
    static bool inBoard(int mx, int my);
    static bool inKingArea(int mx, int my, colorType mcolor);
    static bool inColorArea(int mx, int my, colorType mcolor);
    colorType oppColor() const;
    static colorType oppColor(colorType mcolor);
    bool isMoveValid(const Move& move, bool mustDefend = true);
    bool isLegalMove(const Move& move, bool mustDefend = true);
    bool isLegalMoveWithForbidden(const Move& move, bool mustDefend = true);
    bool isMoveValidWithForbidden(const Move& move, bool mustDefend = true);
    bool makeMoveAssumeLegal(const Move& move, bool recordCheck = false);
    bool attacked(colorType color, int mx, int my) const;
    bool isKingAttacked(colorType side) const;
    bool currentKingAttacked() const;
    bool isMyKingAttackedAfterMove(const Move& move);
    bool isOppKingAttackedAfterMove(const Move& move);
    bool moveGivesCheck(const Move& move);
    bool longCheckAfterMove(const Move& move);
    bool winAfterMove(const Move& move);
    void undoMove();
    bool exceedMaxPeaceState() const;
    int positionRepeatCount() const;
    static int xy2pos(int mx, int my);
    static int pos2x(int mpos);
    static int pos2y(int mpos);
    Grid getGrid(int mx, int my) const;
    Grid getGridAt(int pos) const;
    colorType currentColor() const;
    int currentTurn() const;
    int peaceTurn() const;
    stoneType sourceType(const Move& move) const;
    stoneType targetType(const Move& move) const;
    bool moveIsCapture(const Move& move) const;
    bool hasKing(colorType mcolor) const;
    uint64_t positionKey() const;
    uint64_t searchKey() const;
    void printBoard() const;
};

#endif // CHINESE_CHESS_CHESS_H

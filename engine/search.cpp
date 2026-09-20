#include "search.h"

#include <algorithm>

int GLOBAL_TIME_LIMIT_MS = DEFAULT_TIME_MS;

Move SearchEngine::findBestMove(Chess& board) {
    SearchInfo info;
    return findBestMove(board, info);
}

Move SearchEngine::findBestMove(Chess& board, SearchInfo& info) {
    // 搜索在副本上进行：SearchTimeout 异常从搜索树深处抛出时，分支上已
    // makeMoveAssumeLegal 的走法来不及 undoMove，若直接在真实棋盘上搜索，
    // 棋盘会被中断的搜索污染（source 格棋子被搬走），后续 applyMove 取到
    // 空棋子编码，前端渲染走子记录时崩溃
    Chess searchBoard = board;
    chess = &searchBoard;
    nodes = 0;
    completedDepth = 0;
    startTime = std::chrono::steady_clock::now();

    std::vector<Move> rootMoves;
    chess->generateMovesWithForbidden(rootMoves, true);
    if (rootMoves.empty()) return Move();
    orderMoves(rootMoves, Move(), 0);
    Move bestMove = rootMoves[0];

    Move bookMove = findOpeningBook(rootMoves);
    if (validMoveObject(bookMove)) {
        info.score = 0;
        info.depth = 0;
        info.nodes = 0;
        info.elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count());
        return bookMove;
    }

    int lastScore = 0;
    for (int depth = 1; depth <= 64; ++depth) {
        try {
            int alpha = -INF_SCORE;
            int beta = INF_SCORE;
            std::pair<int, Move> result;
            if (depth >= 4) {
                int delta = 120;
                alpha = lastScore - delta;
                beta = lastScore + delta;
                while (true) {
                    result = searchRoot(rootMoves, depth, bestMove, alpha, beta);
                    if (result.first <= alpha && alpha > -INF_SCORE / 2) {
                        alpha -= delta;
                        delta *= 2;
                        if (delta > 2000) alpha = -INF_SCORE;
                        continue;
                    }
                    if (result.first >= beta && beta < INF_SCORE / 2) {
                        beta += delta;
                        delta *= 2;
                        if (delta > 2000) beta = INF_SCORE;
                        continue;
                    }
                    break;
                }
            }
            else {
                result = searchRoot(rootMoves, depth, bestMove, alpha, beta);
            }
            lastScore = result.first;
            bestMove = result.second;
            completedDepth = depth;
            if (lastScore > MATE_SCORE - 1024 || lastScore < -MATE_SCORE + 1024) break;
        }
        catch (const SearchTimeout&) {
            break;
        }
    }

    info.score = lastScore;
    info.depth = completedDepth;
    info.nodes = nodes;
    info.elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count());
    return bestMove;
}

Move SearchEngine::makeCoordMove(const char* source, const char* target) const {
    return Move(std::string(source), std::string(target));
}

bool SearchEngine::containsMove(const std::vector<Move>& moves, const Move& candidate) const {
    return std::find(moves.begin(), moves.end(), candidate) != moves.end();
}

bool SearchEngine::hasMajorCapture(const std::vector<Move>& moves) const {
    for (size_t i = 0; i < moves.size(); ++i) {
        stoneType dst = chess->targetType(moves[i]);
        if (dst == King || pieceValue(dst) >= pieceValue(Knight)) return true;
    }
    return false;
}

Move SearchEngine::findOpeningBook(const std::vector<Move>& moves) const {
    if (chess->currentKingAttacked()) return Move();
    int turn = chess->currentTurn();
    colorType side = chess->currentColor();
    std::vector<Move> candidates;
    if (turn == 0 && side == RED) {
        candidates.push_back(makeCoordMove("h2", "e2"));
        candidates.push_back(makeCoordMove("b2", "e2"));
        candidates.push_back(makeCoordMove("b0", "c2"));
        candidates.push_back(makeCoordMove("h0", "g2"));
    }
    else if (turn == 1 && side == BLACK) {
        if (hasMajorCapture(moves)) return Move();
        candidates.push_back(makeCoordMove("h7", "e7"));
        candidates.push_back(makeCoordMove("b7", "e7"));
        candidates.push_back(makeCoordMove("b9", "c7"));
        candidates.push_back(makeCoordMove("h9", "g7"));
    }
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (containsMove(moves, candidates[i])) return candidates[i];
    }
    return Move();
}

void SearchEngine::checkTime() {
    if ((nodes & 2047LL) != 0) return;
    int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count());
    if (elapsed >= timeLimitMs) throw SearchTimeout();
}

int SearchEngine::scoreToTT(int score, int ply) const {
    if (score > MATE_SCORE / 2) return score + ply;
    if (score < -MATE_SCORE / 2) return score - ply;
    return score;
}

int SearchEngine::scoreFromTT(int score, int ply) const {
    if (score > MATE_SCORE / 2) return score - ply;
    if (score < -MATE_SCORE / 2) return score + ply;
    return score;
}

bool SearchEngine::probeTT(uint64_t key, int depth, int alpha, int beta, int ply, int& value, Move& bestMove) {
    const TTEntry& entry = transTable[static_cast<size_t>(key) & TT_MASK];
    if (entry.depth < 0 || entry.key != key) return false;
    bestMove = entry.bestMove;
    if (entry.depth < depth) return false;
    int ttValue = scoreFromTT(entry.value, ply);
    if (entry.flag == TT_EXACT) {
        value = ttValue;
        return true;
    }
    if (entry.flag == TT_LOWER && ttValue >= beta) {
        value = ttValue;
        return true;
    }
    if (entry.flag == TT_UPPER && ttValue <= alpha) {
        value = ttValue;
        return true;
    }
    return false;
}

void SearchEngine::storeTT(uint64_t key, int depth, int value, int flag, const Move& bestMove, int ply) {
    TTEntry& entry = transTable[static_cast<size_t>(key) & TT_MASK];
    if (entry.depth <= depth || entry.key != key || flag == TT_EXACT) {
        entry.key = key;
        entry.depth = depth;
        entry.value = scoreToTT(value, ply);
        entry.flag = flag;
        entry.bestMove = bestMove;
    }
}

std::pair<int, Move> SearchEngine::searchRoot(std::vector<Move>& rootMoves, int depth, const Move& previousBest, int alpha, int beta) {
    orderMoves(rootMoves, previousBest, 0);
    int bestValue = -INF_SCORE;
    Move bestMove = rootMoves[0];
    bool firstMove = true;

    for (size_t i = 0; i < rootMoves.size(); ++i) {
        checkTime();
        const Move& move = rootMoves[i];
        int score;
        chess->makeMoveAssumeLegal(move);
        if (firstMove) {
            score = -negamax(depth - 1, -beta, -alpha, 1);
        }
        else {
            score = -negamax(depth - 1, -alpha - 1, -alpha, 1);
            if (score > alpha && score < beta) {
                score = -negamax(depth - 1, -beta, -alpha, 1);
            }
        }
        chess->undoMove();
        firstMove = false;

        if (score > bestValue) {
            bestValue = score;
            bestMove = move;
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) break;
    }
    return std::make_pair(bestValue, bestMove);
}

int SearchEngine::negamax(int depth, int alpha, int beta, int ply) {
    ++nodes;
    checkTime();

    if (!chess->hasKing(chess->currentColor())) return -MATE_SCORE + ply;
    if (!chess->hasKing(Chess::oppColor(chess->currentColor()))) return MATE_SCORE - ply;
    if (chess->exceedMaxPeaceState()) return 0;
    if (ply >= MAX_SEARCH_PLY - 2) return evaluate();

    bool inCheck = chess->currentKingAttacked();
    if (depth <= 0 && !inCheck) return quiescence(alpha, beta, ply, 0);
    if (depth <= 0 && inCheck) depth = 1;

    int alphaOriginal = alpha;
    uint64_t key = chess->searchKey();
    Move hashMove;
    int ttValue = 0;
    if (probeTT(key, depth, alpha, beta, ply, ttValue, hashMove)) return ttValue;

    std::vector<Move> moves;
    chess->generateMoves(moves, true, false);
    if (moves.empty()) return -MATE_SCORE + ply;
    orderMoves(moves, hashMove, ply);

    int bestValue = -INF_SCORE;
    Move bestMove = moves[0];
    int moveIndex = 0;

    for (size_t i = 0; i < moves.size(); ++i) {
        const Move& move = moves[i];
        bool capture = chess->moveIsCapture(move);
        chess->makeMoveAssumeLegal(move);
        bool givesCheck = chess->currentKingAttacked();
        int nextDepth = depth - 1;
        if (givesCheck && nextDepth <= 2) ++nextDepth;

        int score;
        if (moveIndex == 0) {
            score = -negamax(nextDepth, -beta, -alpha, ply + 1);
        }
        else {
            int reduction = 0;
            if (depth >= 3 && moveIndex >= 4 && !capture && !inCheck && !givesCheck) {
                reduction = 1;
                if (depth >= 5 && moveIndex >= 10) reduction = 2;
            }
            int reducedDepth = nextDepth - reduction;
            if (reducedDepth < 0) reducedDepth = 0;
            score = -negamax(reducedDepth, -alpha - 1, -alpha, ply + 1);
            if (reduction > 0 && score > alpha) {
                score = -negamax(nextDepth, -alpha - 1, -alpha, ply + 1);
            }
            if (score > alpha && score < beta) {
                score = -negamax(nextDepth, -beta, -alpha, ply + 1);
            }
        }
        chess->undoMove();
        ++moveIndex;

        if (score > bestValue) {
            bestValue = score;
            bestMove = move;
        }
        if (score > alpha) alpha = score;
        if (alpha >= beta) {
            if (!capture) {
                saveKiller(ply, move);
                updateHistory(move, depth, chess->currentColor());
            }
            storeTT(key, depth, alpha, TT_LOWER, bestMove, ply);
            return alpha;
        }
    }

    int flag = TT_EXACT;
    if (bestValue <= alphaOriginal) flag = TT_UPPER;
    else if (bestValue >= beta) flag = TT_LOWER;
    storeTT(key, depth, bestValue, flag, bestMove, ply);
    return bestValue;
}

int SearchEngine::quiescence(int alpha, int beta, int ply, int qply) {
    ++nodes;
    checkTime();
    if (!chess->hasKing(chess->currentColor())) return -MATE_SCORE + ply;
    if (!chess->hasKing(Chess::oppColor(chess->currentColor()))) return MATE_SCORE - ply;
    if (chess->exceedMaxPeaceState()) return 0;
    if (ply >= MAX_SEARCH_PLY - 2) return evaluate();

    bool inCheck = chess->currentKingAttacked();
    int standPat = -INF_SCORE;
    if (!inCheck) {
        standPat = evaluate();
        if (standPat >= beta) return beta;
        if (standPat > alpha) alpha = standPat;
        if (qply >= 8) return alpha;
    }

    std::vector<Move> moves;
    if (inCheck) chess->generateMoves(moves, true, false);
    else chess->generateCaptureMoves(moves, true);
    if (moves.empty()) return inCheck ? (-MATE_SCORE + ply) : alpha;
    orderMoves(moves, Move(), ply);

    for (size_t i = 0; i < moves.size(); ++i) {
        const Move& move = moves[i];
        if (!inCheck && standPat != -INF_SCORE) {
            stoneType capturedType = chess->targetType(move);
            int capturedValue = pieceValue(capturedType);
            if (capturedType != King && standPat + capturedValue + 180 < alpha) continue;
        }
        chess->makeMoveAssumeLegal(move);
        int score = -quiescence(-beta, -alpha, ply + 1, qply + 1);
        chess->undoMove();
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

void SearchEngine::saveKiller(int ply, const Move& move) {
    if (ply < 0 || ply >= MAX_SEARCH_PLY) return;
    if (killer[ply][0] != move) {
        killer[ply][1] = killer[ply][0];
        killer[ply][0] = move;
    }
}

void SearchEngine::updateHistory(const Move& move, int depth, colorType side) {
    stoneType type = chess->sourceType(move);
    if (type == None) return;
    int& cell = historyScore[colorIndex(side)][type][Chess::xy2pos(move.target_x, move.target_y)];
    cell += depth * depth * 32;
    if (cell > 10000000) {
        for (int c = 0; c < 2; ++c)
            for (int t = 0; t < 8; ++t)
                for (int p = 0; p < BOARDWIDTH * BOARDHEIGHT; ++p)
                    historyScore[c][t][p] /= 2;
    }
}

void SearchEngine::orderMoves(std::vector<Move>& moves, const Move& hashMove, int ply) {
    struct ScoredMove {
        Move move;
        int score;
    };
    std::vector<ScoredMove> scored;
    scored.reserve(moves.size());
    colorType mover = chess->currentColor();
    for (size_t i = 0; i < moves.size(); ++i) {
        const Move& move = moves[i];
        stoneType src = chess->sourceType(move);
        stoneType dst = chess->targetType(move);
        int score = 0;
        if (validMoveObject(hashMove) && move == hashMove) score += 100000000;
        if (dst != None) {
            score += 10000000 + pieceValue(dst) * 24 - pieceValue(src);
            if (dst == King) score += 50000000;
        }
        else {
            if (ply >= 0 && ply < MAX_SEARCH_PLY) {
                if (move == killer[ply][0]) score += 9000000;
                else if (move == killer[ply][1]) score += 8500000;
            }
            if (src != None) score += historyScore[colorIndex(mover)][src][Chess::xy2pos(move.target_x, move.target_y)];
        }
        int targetCenter = centerScore(move.target_x);
        if (src == Pawn) score += relativeY(mover, move.target_y) * 12 + targetCenter * 5;
        else if (src == Knight || src == Cannon) score += targetCenter * 8;
        else if (src == Rook) score += targetCenter * 3;
        scored.push_back(ScoredMove{ move, score });
    }
    std::sort(scored.begin(), scored.end(), [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
        });
    for (size_t i = 0; i < scored.size(); ++i) moves[i] = scored[i].move;
}

int SearchEngine::mobilityAt(int x, int y, const Grid& grid) const {
    int mobility = 0;
    switch (grid.type) {
    case King: {
        for (int dir = 0; dir < 4; ++dir) {
            int tx = x + dx_strai[dir], ty = y + dy_strai[dir];
            if (Chess::inKingArea(tx, ty, grid.color)) {
                Grid t = chess->getGrid(tx, ty);
                if (t.color != grid.color) ++mobility;
            }
        }
        break;
    }
    case Assistant: {
        for (int dir = 0; dir < 4; ++dir) {
            int tx = x + dx_ob[dir], ty = y + dy_ob[dir];
            if (Chess::inKingArea(tx, ty, grid.color)) {
                Grid t = chess->getGrid(tx, ty);
                if (t.color != grid.color) ++mobility;
            }
        }
        break;
    }
    case Bishop: {
        for (int dir = 0; dir < 4; ++dir) {
            int tx = x + dx_bishop[dir], ty = y + dy_bishop[dir];
            int ex = x + dx_bishop_eye[dir], ey = y + dy_bishop_eye[dir];
            if (Chess::inBoard(tx, ty) && Chess::inBoard(ex, ey) && Chess::inColorArea(tx, ty, grid.color) &&
                chess->getGrid(ex, ey).color == EMPTY) {
                Grid t = chess->getGrid(tx, ty);
                if (t.color != grid.color) ++mobility;
            }
        }
        break;
    }
    case Knight: {
        for (int dir = 0; dir < 8; ++dir) {
            int tx = x + dx_knight[dir], ty = y + dy_knight[dir];
            int fx = x + dx_knight_foot[dir], fy = y + dy_knight_foot[dir];
            if (Chess::inBoard(tx, ty) && Chess::inBoard(fx, fy) && chess->getGrid(fx, fy).color == EMPTY) {
                Grid t = chess->getGrid(tx, ty);
                if (t.color != grid.color) ++mobility;
            }
        }
        break;
    }
    case Rook: {
        for (int dir = 0; dir < 4; ++dir) {
            int tx = x + dx_strai[dir], ty = y + dy_strai[dir];
            while (Chess::inBoard(tx, ty)) {
                Grid t = chess->getGrid(tx, ty);
                if (t.color == grid.color) break;
                mobility += (t.color == EMPTY ? 1 : 2);
                if (t.color != EMPTY) break;
                tx += dx_strai[dir];
                ty += dy_strai[dir];
            }
        }
        break;
    }
    case Cannon: {
        for (int dir = 0; dir < 4; ++dir) {
            bool screen = false;
            int tx = x + dx_strai[dir], ty = y + dy_strai[dir];
            while (Chess::inBoard(tx, ty)) {
                Grid t = chess->getGrid(tx, ty);
                if (!screen) {
                    if (t.color == EMPTY) ++mobility;
                    else screen = true;
                }
                else if (t.color != EMPTY) {
                    if (t.color != grid.color) mobility += 2;
                    break;
                }
                tx += dx_strai[dir];
                ty += dy_strai[dir];
            }
        }
        break;
    }
    case Pawn: {
        int forward = grid.color == RED ? 1 : -1;
        int tx = x, ty = y + forward;
        if (Chess::inBoard(tx, ty) && chess->getGrid(tx, ty).color != grid.color) ++mobility;
        if (crossedRiver(grid.color, y)) {
            for (int dir = 0; dir < 2; ++dir) {
                tx = x + dx_lr[dir];
                ty = y;
                if (Chess::inBoard(tx, ty) && chess->getGrid(tx, ty).color != grid.color) ++mobility;
            }
        }
        break;
    }
    default:
        break;
    }
    return mobility;
}

int SearchEngine::blockedKnightFeet(int x, int y) const {
    static const int fx[4] = { -1, 1, 0, 0 };
    static const int fy[4] = { 0, 0, -1, 1 };
    int blocked = 0;
    for (int i = 0; i < 4; ++i) {
        int tx = x + fx[i], ty = y + fy[i];
        if (Chess::inBoard(tx, ty) && chess->getGrid(tx, ty).color != EMPTY) ++blocked;
    }
    return blocked;
}

bool SearchEngine::findKing(colorType color, int& kx, int& ky) const {
    for (int y = 0; y < BOARDHEIGHT; ++y) {
        for (int x = 0; x < BOARDWIDTH; ++x) {
            Grid grid = chess->getGrid(x, y);
            if (grid.color == color && grid.type == King) {
                kx = x;
                ky = y;
                return true;
            }
        }
    }
    return false;
}

int SearchEngine::filePressurePenalty(colorType side, int kx, int ky) const {
    int penalty = 0;
    colorType enemy = oppositeColor(side);
    for (int sign = -1; sign <= 1; sign += 2) {
        int blockers = 0;
        int y = ky + sign;
        while (Chess::inBoard(kx, y)) {
            Grid grid = chess->getGrid(kx, y);
            if (grid.color != EMPTY) {
                if (grid.color == enemy) {
                    if (blockers == 0 && (grid.type == Rook || grid.type == King)) penalty += (grid.type == King ? 140 : 100);
                    else if (blockers == 1 && grid.type == Cannon) penalty += 85;
                    break;
                }
                ++blockers;
                if (blockers > 1) break;
            }
            y += sign;
        }
    }
    return penalty;
}

int SearchEngine::sideEvaluation(colorType side) const {
    int score = 0;
    int advisors = 0, bishops = 0, rooks = 0, cannons = 0, knights = 0, pawns = 0;
    int kx = -1, ky = -1;

    for (int y = 0; y < BOARDHEIGHT; ++y) {
        for (int x = 0; x < BOARDWIDTH; ++x) {
            Grid grid = chess->getGrid(x, y);
            if (grid.color != side) continue;
            int rel = relativeY(side, y);
            int cen = centerScore(x);
            int mobility = mobilityAt(x, y, grid);
            score += pieceValue(grid.type);

            switch (grid.type) {
            case King:
                kx = x;
                ky = y;
                score += cen * 3;
                if (x != 4) score -= 10;
                break;
            case Rook:
                ++rooks;
                score += mobility * 6 + cen * 4 + rel * 2;
                if (rel >= 5) score += 15;
                break;
            case Knight:
                ++knights;
                score += mobility * 8 + cen * 10 + rel * 4 - blockedKnightFeet(x, y) * 18;
                if (rel >= 4 && cen >= 2) score += 12;
                break;
            case Cannon:
                ++cannons;
                score += mobility * 5 + cen * 8 + (rel >= 4 ? 16 : 0);
                if (y == (side == RED ? 2 : 7) && (x == 1 || x == 7)) score += 6;
                break;
            case Pawn:
                ++pawns;
                score += rel * 18 + cen * 5;
                if (rel >= 5) score += 76 + cen * 13;
                if (rel >= 7) score += 35;
                if (x == 4 && rel >= 5) score += 24;
                break;
            case Assistant:
                ++advisors;
                score += 8 + (Chess::inKingArea(x, y, side) ? 12 : 0);
                if (x == 4 && (y == (side == RED ? 1 : 8))) score += 6;
                break;
            case Bishop:
                ++bishops;
                score += 8 + (rel <= 4 ? 10 : 0);
                break;
            default:
                break;
            }
        }
    }

    score += advisors * 20 + bishops * 16;
    if (advisors < 2) score -= (2 - advisors) * 38;
    if (bishops < 2) score -= (2 - bishops) * 30;
    if (rooks == 0) score -= 65;
    if (rooks >= 1 && cannons + knights >= 2) score += 25;
    if (cannons == 2 && knights == 0) score -= 20;
    if (knights == 2 && cannons == 0) score += 10;
    if (pawns == 0) score -= 25;

    if (kx >= 0) {
        score -= filePressurePenalty(side, kx, ky);
        int homeY = side == RED ? 0 : 9;
        if (std::abs(ky - homeY) >= 2 && advisors + bishops <= 2) score -= 40;
    }
    return score;
}

int SearchEngine::evaluate() {
    int redScore = sideEvaluation(RED);
    int blackScore = sideEvaluation(BLACK);
    int value = redScore - blackScore;
    if (chess->currentColor() == BLACK) value = -value;
    value += 12;
    if (chess->currentKingAttacked()) value -= 75;
    return value;
}

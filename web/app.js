// ============================================================
// 中国象棋 Web 前端逻辑
// 设计原则：前端不含任何规则逻辑，只负责渲染与交互。
// 合法走法、终局判定全部来自后端引擎 API（/api/state 等）。
// ============================================================

"use strict";

// ---------- 常量 ----------
const BOARD_W = 9, BOARD_H = 10;   // 9 列 10 行
const CELL = 64, MARGIN = 40;      // 逻辑像素：格距与边距
const CANVAS_W = MARGIN * 2 + CELL * 8;   // 592
const CANVAS_H = MARGIN * 2 + CELL * 9;   // 656

// 棋子文字（简体）：r=红方 b=黑方，K帅 A仕 B相 N马 R车 C炮 P兵
const PIECE_CHARS = {
    r: { K: "帅", A: "仕", B: "相", N: "马", R: "车", C: "炮", P: "兵" },
    b: { K: "将", A: "士", B: "象", N: "马", R: "车", C: "炮", P: "卒" }
};

// ---------- 全局状态 ----------
let state = null;          // 后端返回的完整局面状态
let selected = null;       // 当前选中的格子 {x, y} 或 null
let busy = false;          // 是否有请求进行中（防止连点）
let mode = "pve_red";      // pve_red / pve_black / pvp
let flipped = false;       // 是否旋转棋盘（人执黑时黑在下）

const canvas = document.getElementById("board");
const ctx = canvas.getContext("2d");

// ============================================================
// 一、API 层：与 C++ 引擎服务通信
// ============================================================

async function apiGet(path) {
    const resp = await fetch(path);
    if (!resp.ok) throw new Error("HTTP " + resp.status);
    return await resp.json();
}

async function apiPost(path, body) {
    const resp = await fetch(path, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body || {})
    });
    if (!resp.ok) throw new Error("HTTP " + resp.status);
    return await resp.json();
}

// ============================================================
// 二、坐标换算：引擎坐标 (x,y) <-> 画布像素
// ============================================================

// 引擎坐标：x 0..8（a..i 列），y 0..9；红方 y=0..4。
// 默认视角红在下：画布行 = 9 - y。执黑时整体旋转 180 度。
function toPixel(x, y) {
    const gx = flipped ? BOARD_W - 1 - x : x;
    const gy = flipped ? y : BOARD_H - 1 - y;
    return { px: MARGIN + gx * CELL, py: MARGIN + gy * CELL };
}

function fromPixel(px, py) {
    for (let x = 0; x < BOARD_W; ++x) {
        for (let y = 0; y < BOARD_H; ++y) {
            const p = toPixel(x, y);
            if (Math.abs(px - p.px) <= CELL * 0.46 && Math.abs(py - p.py) <= CELL * 0.46) {
                return { x, y };
            }
        }
    }
    return null;
}

// ============================================================
// 三、绘制
// ============================================================

function setupCanvas() {
    const dpr = window.devicePixelRatio || 1;
    canvas.width = CANVAS_W * dpr;
    canvas.height = CANVAS_H * dpr;
    canvas.style.width = CANVAS_W + "px";
    canvas.style.height = CANVAS_H + "px";
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

// 画一条棋盘线段（两端点为交叉点坐标）
function line(x1, y1, x2, y2) {
    const a = toPixel(x1, y1), b = toPixel(x2, y2);
    ctx.beginPath();
    ctx.moveTo(a.px, a.py);
    ctx.lineTo(b.px, b.py);
    ctx.stroke();
}

// 楚河汉界之间的竖线需要断开：画左右两边竖线时分开处理
function drawGrid() {
    ctx.strokeStyle = "#5a3a1a";
    ctx.lineWidth = 1.5;

    // 10 条横线（整线）
    for (let y = 0; y < BOARD_H; ++y) line(0, y, 8, y);

    // 9 条竖线：中间区域（y=4~5）只有 x=0 与 x=8 连通（楚河汉界）
    for (let x = 0; x < BOARD_W; ++x) {
        if (x === 0 || x === 8) {
            line(x, 0, x, 9);
        } else {
            line(x, 0, x, 4);
            line(x, 5, x, 9);
        }
    }

    // 九宫斜线
    line(3, 0, 5, 2); line(5, 0, 3, 2);
    line(3, 7, 5, 9); line(5, 7, 3, 9);

    // 外框加粗
    ctx.lineWidth = 3;
    const o = 4; // 外框外扩像素
    const tl = toPixel(0, 0), br = toPixel(8, 9);
    ctx.strokeRect(tl.px - o, tl.py - o, br.px - tl.px + o * 2, br.py - tl.py + o * 2);
}

// 炮位/兵位的星标记
function drawStarMarks() {
    const pts = [
        [1, 2], [7, 2], [1, 7], [7, 7],          // 炮位
        [0, 3], [2, 3], [4, 3], [6, 3], [8, 3],  // 红兵位
        [0, 6], [2, 6], [4, 6], [6, 6], [8, 6]   // 黑卒位
    ];
    ctx.strokeStyle = "#5a3a1a";
    ctx.lineWidth = 1.2;
    const g = 5, l = 9; // 间隙与臂长
    for (const [x, y] of pts) {
        const { px, py } = toPixel(x, y);
        for (const sx of [-1, 1]) for (const sy of [-1, 1]) {
            // 边线上的点只画内侧标记
            if ((x === 0 && sx < 0) || (x === 8 && sx > 0)) continue;
            ctx.beginPath();
            ctx.moveTo(px + sx * g, py + sy * (g + l));
            ctx.lineTo(px + sx * g, py + sy * g);
            ctx.lineTo(px + sx * (g + l), py + sy * g);
            ctx.stroke();
        }
    }
}

function drawRiverText() {
    ctx.fillStyle = "rgba(90,58,26,0.75)";
    ctx.font = "26px KaiTi, STKaiti, serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    const midY = (toPixel(0, 4).py + toPixel(0, 5).py) / 2;
    const left = toPixel(1, 4).px, right = toPixel(7, 4).px;
    ctx.save();
    if (flipped) {
        ctx.translate(CANVAS_W / 2, midY);
        ctx.rotate(Math.PI);
        ctx.fillText("楚 河", -110, 0);
        ctx.fillText("漢 界", 110, 0);
    } else {
        ctx.fillText("楚 河", left + 60, midY);
        ctx.fillText("漢 界", right - 60, midY);
    }
    ctx.restore();
}

function drawCoordLabels() {
    ctx.fillStyle = "rgba(60,35,10,0.8)";
    ctx.font = "12px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    const files = ["九", "八", "七", "六", "五", "四", "三", "二", "一"];
    for (let x = 0; x < BOARD_W; ++x) {
        const col = flipped ? x : 8 - x;   // 红方视角：一路在右
        // 顶部列号（红方）
        const top = toPixel(x, 0);
        ctx.fillText(files[flipped ? 8 - x : x], top.px, top.py - 24);
        // 底部数字（黑方）
        const bot = toPixel(x, 9);
        ctx.fillText(String(1 + col), bot.px, bot.py + 24);
    }
}

function pieceAt(x, y) {
    if (!state || !state.board) return "";
    return state.board[y * BOARD_W + x] || "";
}

// 画一枚棋子：圆底 + 汉字
function drawPiece(x, y, code, isSelected) {
    const { px, py } = toPixel(x, y);
    const color = code[0], type = code[1];
    const isRed = color === "r";
    const r = 26;

    // 底座阴影
    ctx.beginPath();
    ctx.arc(px + 2, py + 3, r, 0, Math.PI * 2);
    ctx.fillStyle = "rgba(0,0,0,0.35)";
    ctx.fill();

    // 木色底
    const grad = ctx.createRadialGradient(px - 8, py - 10, 4, px, py, r);
    if (isRed) {
        grad.addColorStop(0, "#fff4e0");
        grad.addColorStop(1, "#f3d5a8");
    } else {
        grad.addColorStop(0, "#f2ede2");
        grad.addColorStop(1, "#d9cfb8");
    }
    ctx.beginPath();
    ctx.arc(px, py, r, 0, Math.PI * 2);
    ctx.fillStyle = grad;
    ctx.fill();

    // 外圈与内圈
    ctx.lineWidth = 2.5;
    ctx.strokeStyle = isRed ? "#b03a1e" : "#1f5d38";
    ctx.stroke();
    ctx.beginPath();
    ctx.arc(px, py, r - 4.5, 0, Math.PI * 2);
    ctx.lineWidth = 1.2;
    ctx.stroke();

    // 选中时的高亮环
    if (isSelected) {
        ctx.beginPath();
        ctx.arc(px, py, r + 4, 0, Math.PI * 2);
        ctx.lineWidth = 3;
        ctx.strokeStyle = "#ffcf4d";
        ctx.stroke();
    }

    // 棋子文字
    ctx.fillStyle = isRed ? "#c02010" : "#0f4d2a";
    ctx.font = "bold 26px KaiTi, STKaiti, serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(PIECE_CHARS[color][type], px, py + 2);
}

// 合法落点：小圆点；能吃子的画圆环
function drawLegalTarget(x, y, isCapture) {
    const { px, py } = toPixel(x, y);
    ctx.beginPath();
    if (isCapture) {
        ctx.arc(px, py, 30, 0, Math.PI * 2);
        ctx.lineWidth = 3;
        ctx.setLineDash([7, 5]);
        ctx.strokeStyle = "rgba(220,60,40,0.9)";
    } else {
        ctx.arc(px, py, 9, 0, Math.PI * 2);
        ctx.lineWidth = 2;
        ctx.strokeStyle = "rgba(30,110,50,0.85)";
        ctx.fillStyle = "rgba(30,110,50,0.45)";
        ctx.fill();
    }
    ctx.stroke();
    ctx.setLineDash([]);
}

// 上一手痕迹：起点角框 + 终点角框
function drawLastMove(from, to) {
    ctx.lineWidth = 2;
    ctx.strokeStyle = "rgba(255,180,60,0.9)";
    for (const sq of [from, to]) {
        const { px, py } = toPixel(sq.x, sq.y);
        const s = 20, e = 8;
        for (const sx of [-1, 1]) for (const sy of [-1, 1]) {
            ctx.beginPath();
            ctx.moveTo(px + sx * s, py + sy * (s - e));
            ctx.lineTo(px + sx * s, py + sy * s);
            ctx.lineTo(px + sx * (s - e), py + sy * s);
            ctx.stroke();
        }
    }
}

function draw() {
    // 背景
    ctx.clearRect(0, 0, CANVAS_W, CANVAS_H);
    const bg = ctx.createLinearGradient(0, 0, CANVAS_W, CANVAS_H);
    bg.addColorStop(0, "#e9c489");
    bg.addColorStop(1, "#d8ab68");
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);

    drawGrid();
    drawStarMarks();
    drawRiverText();
    drawCoordLabels();

    if (state && state.lastMove) drawLastMove(state.lastMove.from, state.lastMove.to);

    // 先画棋子，再画高亮层
    for (let x = 0; x < BOARD_W; ++x) {
        for (let y = 0; y < BOARD_H; ++y) {
            const code = pieceAt(x, y);
            if (!code) continue;
            const isSel = selected && selected.x === x && selected.y === y;
            drawPiece(x, y, code, isSel);
        }
    }

    if (selected && state) {
        // 从后端给出的合法走法中过滤出"以选中子为起点"的落点
        for (const mv of state.legalMoves || []) {
            const fx = mv.from.charCodeAt(0) - 97, fy = +mv.from[1];
            const tx = mv.to.charCodeAt(0) - 97, ty = +mv.to[1];
            if (fx === selected.x && fy === selected.y) {
                drawLegalTarget(tx, ty, !!pieceAt(tx, ty));
            }
        }
    }
}

// ============================================================
// 四、交互
// ============================================================

canvas.addEventListener("click", (ev) => {
    if (busy || !state || state.result) return;
    const rect = canvas.getBoundingClientRect();
    const sq = fromPixel(ev.clientX - rect.left, ev.clientY - rect.top);
    if (!sq) return;

    const code = pieceAt(sq.x, sq.y);
    const mine = code && code[0] === state.current[0]; // "red"/"black" 首字母 r/b

    if (selected) {
        // 已有选中子：点到自己另一枚子 → 换选；否则尝试走子
        if (mine && !(sq.x === selected.x && sq.y === selected.y)) {
            selected = sq;
        } else if (!(sq.x === selected.x && sq.y === selected.y)) {
            tryMove(selected, sq);
            return;
        } else {
            selected = null; // 点自身取消选择
        }
    } else if (mine) {
        selected = sq;
    }
    draw();
});

// 尝试走子：交给后端判定合法性
async function tryMove(from, to) {
    const name = sqName(from) + "-" + sqName(to);
    busy = true;
    setStatusText("等待走子 " + name + " …");
    try {
        const resp = await apiPost("/api/move", {
            from: sqName(from), to: sqName(to), mode: mode
        });
        selected = null;
        await refreshState(resp.state);
        if (!resp.ok) {
            // 显示服务端给出的真实原因（如"人机模式下轮到电脑走棋"）
            setStatusText(resp.message || ("非法走法：" + name));
            await sleep(800);
        }
        await maybeTriggerAI();
    } catch (e) {
        showOverlay("与服务端通信失败：" + e.message);
    } finally {
        busy = false;
    }
}

function sqName(sq) {
    return String.fromCharCode(97 + sq.x) + sq.y;
}

// ============================================================
// 五、流程控制
// ============================================================

async function refreshState(s) {
    if (s) state = s;
    else {
        try {
            state = await apiGet("/api/state");
        } catch (e) {
            showOverlay("无法连接引擎服务。<br>请先运行 bin\\chess_server.exe，再刷新页面。");
            return;
        }
    }
    hideOverlay();
    flipped = (mode === "pve_black");
    updatePanel();
    draw();
    checkGameEnd();   // 对局结束时播放音效并弹出结果弹窗
}

// 人机模式下，轮到电脑且对局未结束 → 自动请求电脑走棋
// 注意：本函数总是由已持有 busy 的流程（tryMove/btnNew）内部调用，
// 因此这里不能再检查 busy，否则会被调用方的 busy 拦下，AI 永远不会应手
async function maybeTriggerAI() {
    if (!state || state.result) return;
    const aiColor = mode === "pve_red" ? "black" : mode === "pve_black" ? "red" : null;
    if (!aiColor || state.current !== aiColor) return;
    busy = true;
    setButtonsDisabled(true);
    setStatusText("电脑思考中…");
    try {
        const resp = await apiPost("/api/bestmove", { mode: mode });
        await refreshState(resp.state);
        // 服务端拒绝走棋时给出明确原因，避免"AI 没反应"的假象
        if (!resp.ok) setStatusText(resp.message || "电脑走棋失败");
    } catch (e) {
        showOverlay("电脑走棋失败：" + e.message);
    } finally {
        busy = false;
        setButtonsDisabled(false);
    }
}

function updatePanel() {
    if (!state) return;
    const isRed = state.current === "red";
    const who = state.result
        ? (state.result === "red_win" ? "红方胜！" : state.result === "black_win" ? "黑方胜！" : "和棋")
        : (isRed ? "红方行棋" : "黑方行棋");
    const turnEl = document.getElementById("turnText");
    turnEl.textContent = who;
    turnEl.className = "turn " + (state.result ? (state.result === "black_win" ? "black" : "red") : (isRed ? "red" : "black"));
    document.getElementById("checkText").textContent =
        state.result ? "" : (state.inCheck ? "将军！" : "");

    // 引擎信息
    const info = document.getElementById("engineInfo");
    if (state.engine) {
        const e = state.engine;
        const book = e.depth === 0 && e.nodes === 0 ? "（开局库）" : "";
        info.textContent = "引擎：" + e.move + book + "  分数 " + e.score +
            "  深度 " + e.depth + "  节点 " + e.nodes + "  用时 " + e.ms + "ms";
    }

    // 走子记录
    const list = document.getElementById("historyList");
    list.innerHTML = "";
    (state.history || []).forEach((h, i) => {
        const li = document.createElement("li");
        if (i === state.history.length - 1) li.className = "latest";
        // 防御：piece/capture 必须是"颜色+类型"双字符编码，异常数据显示占位符而不是崩溃
        const pc = h.piece || "";
        const pname = (PIECE_CHARS[pc[0]] && PIECE_CHARS[pc[0]][pc[1]]) || "?";
        const capPc = h.capture || "";
        const capName = (PIECE_CHARS[capPc[0]] && PIECE_CHARS[capPc[0]][capPc[1]]) || "";
        const red = pc[0] === "r";
        li.innerHTML =
            '<span class="no">' + (i + 1) + '.</span>' +
            '<span class="' + (red ? "side-r" : "side-b") + '">' +
            (red ? "红 " : "黑 ") + pname + " " +
            h.from + "-" + h.to + "</span>" +
            (h.capture ? ' <span class="cap">×' + capName + "</span>" : "") +
            (h.check ? ' <span class="cap">将</span>' : "");
        list.appendChild(li);
    });
    list.scrollTop = list.scrollHeight;

    setButtonsDisabled(busy);
}

function setStatusText(t) {
    const turnEl = document.getElementById("turnText");
    turnEl.textContent = t;
}

function setButtonsDisabled(dis) {
    for (const id of ["btnNew", "btnUndo", "btnAi"]) {
        document.getElementById(id).disabled = dis;
    }
}

function showOverlay(html) {
    const el = document.getElementById("overlay");
    document.getElementById("overlayText").innerHTML = html;
    el.classList.remove("hidden");
}

function hideOverlay() {
    document.getElementById("overlay").classList.add("hidden");
}

// ---------- 终局音效（Web Audio 合成，无需音频文件） ----------
let audioCtx = null;

function ensureAudio() {
    try {
        if (!audioCtx) {
            const AC = window.AudioContext || window.webkitAudioContext;
            if (!AC) return null;
            audioCtx = new AC();
        }
        // 浏览器自动播放策略：AudioContext 需在用户手势后 resume
        if (audioCtx.state === "suspended") audioCtx.resume().catch(() => {});
        return audioCtx;
    } catch (e) { return null; }
}

// 播放单个音符：freq 频率(Hz)、start 相对起始秒、dur 时长秒、vol 音量
function tone(freq, start, dur, vol) {
    const ac = ensureAudio();
    if (!ac) return;
    try {
        const osc = ac.createOscillator();
        const gain = ac.createGain();
        osc.type = "triangle";
        osc.frequency.value = freq;
        const t0 = ac.currentTime + start;
        gain.gain.setValueAtTime(0.0001, t0);
        gain.gain.exponentialRampToValueAtTime(vol, t0 + 0.02);
        gain.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
        osc.connect(gain);
        gain.connect(ac.destination);
        osc.start(t0);
        osc.stop(t0 + dur + 0.05);
    } catch (e) { /* 音频不可用时静默跳过 */ }
}

function playWinSound() {   // 胜利：上扬号角 C5-E5-G5-C6 + 高八度泛音 + 长尾音
    const seq = [[523.25, 0], [659.25, 0.13], [783.99, 0.26], [1046.5, 0.39]];
    for (const [f, t] of seq) {
        tone(f, t, 0.3, 0.28);
        tone(f * 2, t, 0.3, 0.07);
    }
    tone(1046.5, 0.56, 0.8, 0.24);
}

function playLoseSound() {  // 失败：低沉下行两音
    tone(311.13, 0, 0.35, 0.24);
    tone(233.08, 0.28, 0.65, 0.24);
}

function playDrawSound() {  // 和棋：平稳双音
    tone(440, 0, 0.22, 0.2);
    tone(440, 0.24, 0.45, 0.2);
}

// ---------- 对局结束弹窗 ----------
let endShownFor = "";   // 已弹窗提示过的 result，同一结果只弹一次

function checkGameEnd() {
    const result = state && state.result ? state.result : "";
    if (!result) { endShownFor = ""; return; }
    if (endShownFor === result) return;   // 避免重复弹窗
    endShownFor = result;

    let title, sub, cls, sound;
    if (result === "draw_peace" || result === "draw_repeat") {
        title = "和　棋";
        sub = "双方势均力敌，不分胜负。";
        cls = "draw";
        sound = playDrawSound;
    } else {
        const winner = result === "red_win" ? "红方" : "黑方";
        const human = mode === "pve_red" ? "red" : mode === "pve_black" ? "black" : null;
        if (!human) {           // 双人对战：只显示获胜方
            title = winner + "胜利";
            sub = "恭喜" + winner + "获胜！";
            cls = "win";
            sound = playWinSound;
        } else if (human === (result === "red_win" ? "red" : "black")) {
            title = "胜　利";   // 人机模式：人类获胜
            sub = "你执" + (human === "red" ? "红" : "黑") + "战胜了电脑，恭喜！";
            cls = "win";
            sound = playWinSound;
        } else {                // 人机模式：电脑获胜
            title = "惜　败";
            sub = winner + "获胜，再接再厉！";
            cls = "lose";
            sound = playLoseSound;
        }
    }

    const modal = document.getElementById("endModal");
    document.getElementById("endTitle").textContent = title;
    document.getElementById("endSub").textContent = sub;
    modal.classList.remove("hidden", "win", "lose", "draw");
    modal.classList.add(cls);
    sound();
}

function hideEndModal() {
    document.getElementById("endModal").classList.add("hidden");
}

function sleep(ms) { return new Promise(r => setTimeout(r, ms)); }

// ============================================================
// 六、按钮事件与启动
// ============================================================

document.getElementById("btnNew").addEventListener("click", async () => {
    if (busy) return;
    mode = document.getElementById("modeSel").value;
    busy = true;
    try {
        const resp = await apiPost("/api/new", { mode: mode });
        selected = null;
        await refreshState(resp.state);
        await maybeTriggerAI();  // 执黑时电脑（红方）先走
    } catch (e) {
        showOverlay("新对局失败：" + e.message);
    } finally {
        busy = false;
    }
});

document.getElementById("btnUndo").addEventListener("click", async () => {
    if (busy) return;
    busy = true;
    try {
        const resp = await apiPost("/api/undo", { mode: mode });
        selected = null;
        await refreshState(resp.state);
    } catch (e) {
        showOverlay("悔棋失败：" + e.message);
    } finally {
        busy = false;
    }
});

document.getElementById("btnAi").addEventListener("click", async () => {
    if (busy || !state || state.result) return;
    mode = document.getElementById("modeSel").value;
    busy = true;
    setButtonsDisabled(true);
    setStatusText("电脑思考中…");
    try {
        const resp = await apiPost("/api/bestmove", { mode: mode });
        selected = null;
        await refreshState(resp.state);
    } catch (e) {
        showOverlay("电脑走棋失败：" + e.message);
    } finally {
        busy = false;
        setButtonsDisabled(false);
    }
});

// 弹窗按钮：再来一局 / 继续观棋
document.getElementById("btnAgain").addEventListener("click", () => {
    hideEndModal();
    document.getElementById("btnNew").click();
});

document.getElementById("btnClose").addEventListener("click", hideEndModal);

// 切换对局模式 = 立即开新局（否则旧局面仍是旧模式，AI 不会按新模式先手）
document.getElementById("modeSel").addEventListener("change", () => {
    document.getElementById("btnNew").click();
});

setupCanvas();
refreshState();

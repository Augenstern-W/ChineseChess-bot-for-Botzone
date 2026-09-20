# 中国象棋（Chinese Chess）完整项目文档

> 一个 **"一套引擎、四种玩法"** 的中国象棋（Xiangqi）完整项目：
> 从 AI 搜索引擎、规则引擎，到 Win32 图形界面、控制台程序、HTTP 服务器 + 浏览器前端、Botzone 竞赛平台 Bot，
> 全部用 **标准 C++14 + MinGW** 实现，零第三方依赖，一键构建，单文件发布。

---

## 目录

- [1. 项目总览](#1-项目总览)
- [2. 快速开始（构建与运行）](#2-快速开始构建与运行)
- [3. 中国象棋规则速览](#3-中国象棋规则速览)
- [4. 玩法详解](#4-玩法详解)
  - [4.1 图形界面对弈（chess_gui.exe，推荐）](#41-图形界面对弈chess_guiexe推荐)
  - [4.2 控制台对弈（chess_console.exe）](#42-控制台对弈chess_consoleexe)
  - [4.3 本地 Web 对战（chess_server.exe）](#43-本地-web-对战chess_serverexe)
  - [4.4 Botzone 平台对战（botzone_submit.cpp）](#44-botzone-平台对战botzone_submitcpp)
  - [4.5 引擎自检（selftest.exe）](#45-引擎自检selftestexe)
- [5. 项目结构（逐文件详解）](#5-项目结构逐文件详解)
- [6. 架构设计：为什么"一套引擎、四种前端"](#6-架构设计为什么一套引擎四种前端)
- [7. 坐标系统与记谱法](#7-坐标系统与记谱法)
- [8. 引擎技术详解（核心知识点）](#8-引擎技术详解核心知识点)
  - [8.1 棋盘表示与数据结构](#81-棋盘表示与数据结构chess-类)
  - [8.2 Zobrist 哈希](#82-zobrist-哈希)
  - [8.3 走法生成与合法性检测](#83-走法生成与合法性检测)
  - [8.4 禁着与和棋规则](#84-禁着与和棋规则)
  - [8.5 搜索引擎](#85-搜索引擎searchengine)
  - [8.6 局面评估函数](#86-局面评估函数)
- [9. 各前端技术详解](#9-各前端技术详解)
  - [9.1 Win32 GDI 图形界面](#91-win32-gdi-图形界面gui_maincpp)
  - [9.2 控制台程序](#92-控制台程序console_maincpp)
  - [9.3 Winsock HTTP 服务器](#93-winsock-http-服务器server_maincpp)
  - [9.4 浏览器前端（web/）](#94-浏览器前端web)
  - [9.5 Botzone 平台适配](#95-botzone-平台适配botzone_maincpp)
- [10. 构建系统详解](#10-构建系统详解)
- [11. HTTP API 完整文档](#11-http-api-完整文档)
- [12. Botzone 交互协议](#12-botzone-交互协议)
- [13. 常量与参数速查表](#13-常量与参数速查表)
- [14. 知识点地图（教学索引）](#14-知识点地图教学索引)
- [15. 已知行为与注意事项](#15-已知行为与注意事项)
- [16. 常见问题 FAQ](#16-常见问题-faq)

---

## 1. 项目总览

本项目包含 **五个可运行产物**，全部 **共用同一套引擎代码**（`engine/` 目录）：

| 产物 | 文件 | 界面 | 依赖 | 适用场景 |
| ---- | ---- | ---- | ---- | -------- |
| **图形界面对弈** | `bin\chess_gui.exe` | Win32 原生窗口（GDI 绘制） | 无（单文件静态链接） | 日常下棋，**双击即玩，推荐** |
| **控制台对弈** | `bin\chess_console.exe` | 终端文字棋盘 | 无（单文件静态链接） | 无 GUI 环境 / 远程 SSH / 喜欢命令行 |
| **本地 Web 对战** | `bin\chess_server.exe` | 浏览器 Canvas 界面 | 内置 HTTP 服务（启动后自动开浏览器） | 喜欢网页界面、演示前后端架构 |
| **引擎自检** | `bin\selftest.exe` | 终端日志 | 无 | 验证引擎可用（AI 自对弈） |
| **Botzone Bot** | `botzone\botzone_submit.cpp` | 平台侧 | 平台提供 jsoncpp | 参加 Botzone 中国象棋比赛 |

引擎核心能力（所有版本完全一致）：

- **搜索算法**：迭代加深 + Negamax Alpha-Beta（PVS 主变量搜索）+ 置换表 + 静态搜索（Quiescence）+ LMR + 杀手着法/历史启发 +渴望窗口
- **评估函数**：子力价值 + 机动性 + 位置分（PST）+ 王安全（肋道线压力、士象完整度）+ 兵形/结构分
- **规则完整性**：全部棋子走法（含蹩马腿、塞象眼、炮翻山）、将军检测、飞将（对将）禁手、**长将禁着**、**三次重复禁着**、60 回合无吃子和棋
- **开局库**：前 1~2 步使用固定着法（中炮体系），避免开局波动
- **时间管理**：Botzone 版按平台 `time_limit` 动态分配；本地版默认 800ms/步

> **关于根目录 `ChineseChess.cpp`**：这是项目最初的原始单文件版本（引擎 + Botzone I/O 全部内联在一个文件里），
> **原样保留存档，不做任何修改**。所有新功能一律在 `engine/` + 各入口文件中开发；
> Botzone 提交以聚合脚本生成的 `botzone/botzone_submit.cpp` 为准。

---

## 2. 快速开始（构建与运行）

### 2.1 一键构建（Windows）

双击或命令行运行 **`build.bat`**，脚本会自动使用项目内置的 `tools\mingw64` 编译器
（WinLibs GCC 16.2.0 绿色版），**无需安装任何环境**，依次完成 6 步：

| 步骤 | 产物 | 说明 |
| ---- | ---- | ---- |
| 1/6 | `server\web_resources.h` | PowerShell 脚本把 `web/` 三个前端文件嵌入为 C++ 原始字符串 |
| 2/6 | `bin\selftest.exe` | 引擎自检（AI 自对弈，验证引擎可用） |
| 3/6 | `bin\chess_gui.exe` | 图形界面对弈（`-mwindows` 窗口程序） |
| 4/6 | `bin\chess_console.exe` | 控制台对弈 |
| 5/6 | `bin\chess_server.exe` | 本地 Web 对战服务（链接 `ws2_32`、`shell32`） |
| 6/6 | `botzone\botzone_submit.cpp` | 聚合 engine + botzone 入口为平台要求的单文件 |

构建成功的标志是最后输出 `Build OK` 与产物清单。

### 2.2 运行（任选其一）

```bash
bin\chess_gui.exe        # ① 图形界面（推荐）：双击即可
bin\chess_console.exe    # ② 控制台：双击或命令行
start_server.bat         # ③ Web 版：启动服务并自动打开浏览器
bin\selftest.exe         # ④ 引擎自检
# ⑤ Botzone：把 botzone\botzone_submit.cpp 全文粘贴到平台提交框
```

四个 exe 均为 **静态链接**（`-static -static-libgcc -static-libstdc++`），
不依赖任何 DLL / 运行库，可直接拷贝到其他 Windows 机器运行。

---

## 3. 中国象棋规则速览

如果你已熟悉象棋规则可跳过本节；下面对照本项目实现来介绍规则。

### 3.1 棋盘

- **9 列 × 10 行**，棋子走在**交叉点**上（不是格子里），共 90 个交叉点。
- 中间第 4~5 行之间是"**楚河汉界**"（河界），只有兵（卒）过河后和所有棋子过河后行为改变。
- 双方各有 **九宫**（3×3 区域，带斜线）：红方九宫 `x∈[3,5], y∈[0,2]`，黑方 `x∈[3,5], y∈[7,9]`。
- 红方位于 `y∈[0,4]`（下），黑方位于 `y∈[5,9]`（上），**红先黑后**。

### 3.2 棋子与走法（本项目编码）

每方 16 子，本项目内部用 `stoneType` 枚举 + `colorType` 颜色表示：

| 编码 | 红 / 黑 | 每方数量 | 走法规则（本项目实现要点） |
| ---- | ------- | -------- | -------------------------- |
| `K` | 帥 / 將 | 1 | 九宫内**直行一步**；两条帅不可在同一列直接照面（"飞将"，实现为对将即受攻） |
| `A` | 仕 / 士 | 2 | 九宫内**斜行一步** |
| `B` | 相 / 象 | 2 | 田字斜走两格，**塞象眼**（田字中心有子则不可走），**不可过河** |
| `N` | 傌 / 馬 | 2 | 先直一格再斜一格（"马走日"），**蹩马腿**（直一格处有子则该方向不可走） |
| `R` | 俥 / 車 | 2 | 直线任意距离滑行，不可越子，吃线上第一个敌子 |
| `C` | 炮 / 砲 | 2 | 走法同车；**吃子必须隔恰好一个子**（"炮翻山"/隔山打） |
| `P` | 兵 / 卒 | 5 | 过河前只能**前进一步**；过河后可**前进或横走**一步，永不后退 |

### 3.3 胜负与和棋（本项目判定）

| 结果 | 判定条件（实现位置） |
| ---- | -------------------- |
| 一方胜 | 轮到走棋的一方**无任何合法着法**（被将死或困毙，均判负） |
| 飞将 | 同列双方将帅中间无子时，"移动后造成照面"视为送将，非法 |
| 和棋（无吃子） | 连续 **60 回合**无吃子（`peaceCount >= 59`，`exceedMaxPeaceState`） |
| 和棋（重复） | 当前无吃子片段内**三次重复局面**（`positionRepeatCount() >= 3`） |
| 长将禁着 | 己方连续走出**无吃子将军**步数超过上限（=2）后，继续"不吃子将军"的着法被剔除（若该步直接取胜则放行） |

> 注：本项目实现的禁着是**教学级简化**——长将按"连续无吃子将军"计数，未实现长捉（无吃子威胁）判定。

---

## 4. 玩法详解

### 4.1 图形界面对弈（chess_gui.exe，推荐）

双击 `bin\chess_gui.exe` 弹出原生 Windows 窗口，**无需网络、无需浏览器**。

#### 窗口布局

```
┌────────────────────────────┬──────────────┐
│                            │ 人机·执红  人机·执黑 │
│                            │ 双人对战  悔  棋   │
│        木色棋盘（GDI 绘制）      │ 再来一局  重  置   │
│     （交叉点棋盘 + 圆形棋子）      │──────────────│
│                            │ 中国象棋       │
│                            │ 轮到红方走棋     │
│                            │ 将军！（如有）    │
│                            │ AI: score=...  │
│                            │ 走子记录        │
│                            │ 1. 炮 h2-e2    │
│                            │ 2. 馬 b9-c7    │
└────────────────────────────┴──────────────┘
```

#### 六个按钮

| 按钮 | 行为 |
| ---- | ---- |
| **人机·执红** | 开新局，你执红先手，电脑执黑 |
| **人机·执黑** | 开新局，你执黑后手，电脑执红**先走第一步**；棋盘自动翻转 180°（己方在下） |
| **双人对战** | 开新局，同屏双人轮流走子（棋盘红下黑上不翻转） |
| **悔  棋** | 人机模式一次撤销**两步**（电脑应手 + 你的上一步）；双人模式撤销一步；AI 思考中不可用 |
| **再来一局** | **仅终局后可用**，按当前模式重开 |
| **重  置** | **随时可用**，中途弃局按当前模式重开（与"再来一局"的唯一区别：重置不等终局） |

> AI 思考期间 6 个按钮全部禁用，防止并发操作破坏状态；思考结束自动恢复。

#### 鼠标操作

1. **点击己方棋子** → 选中：棋子出现**金色圆圈**高亮，同时显示全部合法落点；
2. 合法落点提示：空点显示**绿色实心圆点**，可吃子的点显示**红色圆环**；
3. **点击合法落点** → 走子；点击其他己方棋子 → 改选；点击棋盘外/非法处 → 取消选中；
4. 轮到电脑、AI 思考中、对局已结束时点击无效。

#### 棋盘细节

- 木色底 + 深棕线条：横线 10 条贯通；竖线中间在楚河汉界断开（两侧边线贯通）；九宫斜线；炮位/兵位"交叉角"标记；
- 楚河漢界文字；**最后一步**的起点与终点显示暗红色小方块；
- 执黑时整个棋盘（含坐标）自动翻转，你的棋子永远在下方。

#### 侧栏信息

- 当前状态：轮次 / 将军提示 / 电脑思考中 / 终局结果（红色）；
- **将军！**：上一步形成将军时显示；
- **AI 搜索信息**：`AI: score=分数 depth=深度 nodes=节点数 耗时ms`；
- **走子记录**：格式 `序号. 棋子名 起点-终点`（如 `1. 炮 h2-e2`），自动滚动显示最近若干条。

#### 终局

无合法着法（将杀/困毙）、60 回合无吃子、三次重复局面时，弹出 Windows 消息框提示
"红方胜！/ 黑方胜！/ 和棋"，状态栏同步显示，"再来一局"按钮点亮。

---

### 4.2 控制台对弈（chess_console.exe）

```bash
bin\chess_console.exe          # 交互式菜单选择模式
bin\chess_console.exe red      # 人机，玩家执红先手
bin\chess_console.exe black    # 人机，玩家执黑后手（电脑先走红棋）
bin\chess_console.exe pvp      # 双人轮流走子
```

棋盘以中文棋子打印（红：帥仕相傌俥炮兵；黑：將士象馬車砲卒；空点 `．`），
执黑时棋盘自动翻转（行序倒置、列标 `i~a` 倒排，己方在下）。

**指令**（回车确认）：

| 指令 | 说明 |
| ---- | ---- |
| `h2e2` | 走棋：起点+终点各两字符（Botzone 坐标），如 `h2e2` = 红炮二平五 |
| `undo` | 悔棋：人机模式一次撤销两步（电脑应手+你的着法），双人模式一步 |
| `new`  | 新对局（终局后同样输入 new 重开） |
| `quit` | 退出 |

- 电脑走棋会打印搜索信息 `(score=分数 depth=深度 nodes=节点数 耗时ms)`；
- 将军时打印 `*** 将军！ ***`；非法着法会提示原因（格式错误/不合法）；
- 终局（无着法/60 回合无吃子/三次重复）自动判定并提示，输入 `new` 或 `quit`。

---

### 4.3 本地 Web 对战（chess_server.exe）

运行 `start_server.bat`（或手动 `bin\chess_server.exe 8080`），
服务启动后**自动打开默认浏览器**访问 `http://localhost:8080`。

**页面功能**：

- **模式选择**：`人机 · 我执红先手` / `人机 · 我执黑后手`（棋盘自动旋转 180°）/ `双人对战`；
- **新对局 / 悔棋**（人机模式自动回退到轮到你）/ **电脑代走**（让引擎替当前行棋方走一步，双人模式也能用）；
- 点击己方棋子 → 高亮全部合法落点（可吃子显示红色虚线环）→ 点击落点走子；上一手起点终点有标记；
- 侧栏：当前轮次与将军提示、引擎思考信息（分数/深度/节点数/耗时）、走子记录（含吃子与将军标记）；
- 终局弹出居中卡片（再来一局 / 继续观棋）；服务端不可达时显示遮罩提示。

**架构**（详见 [第 9.3 节](#93-winsock-http-服务器server_maincpp) 与 [第 11 节](#11-http-api-完整文档)）：
前端**不做任何规则判断**，走法合法性、将军、终局全部由 C++ 服务端计算，
前端只负责 Canvas 渲染、交互过滤与展示——**规则只实现一份**。

---

### 4.4 Botzone 平台对战（botzone_submit.cpp）

把 `botzone\botzone_submit.cpp` 全文粘贴到 [Botzone](https://www.botzone.org.cn) 中国象棋的 C++ 提交框即可
（平台自动提供 `jsoncpp/json.h`，本地用 `third_party/jsoncpp_stub` 桩做语法检查）。

- 平台每回合以**一行 JSON** 下发全部历史（对手着法序列 + 己方应手序列），
  程序在本地**重放重建局面**后搜索出最佳着法，输出一行 JSON（详见[第 12 节](#12-botzone-交互协议)）；
- 思考时限取平台 `time_limit` 字段并扣减安全余量（见 [8.5 时间管理](#时间管理)）；
- 无合法着法时输出 `"-1"/"-1"` 认负。

### 4.5 引擎自检（selftest.exe）

```bash
bin\selftest.exe        # AI 自对弈默认 30 步，每步 200ms，打印分数/深度/节点数/耗时
bin\selftest.exe 60     # 自对弈 60 步
```

输出每步 `ply n: h2-e2 score= ... depth= ... nodes= ... time=...ms`，最后打印 `selftest OK`。
用于重构后快速验证引擎行为正常（无崩溃、分数合理、深度正常）。

---

## 5. 项目结构（逐文件详解）

```
ChineseChess/
├── ChineseChess.cpp            # 【存档】最初的原始单文件版本（引擎+Botzone I/O 内联），
│                               #   原样保留不再维护；学习"项目演化史"可对照阅读
├── build.bat                   # 一键构建脚本（6 步，自动用内置 mingw64）
├── start_server.bat             # 一键启动 Web 对战（起服务 + 开浏览器）
├── README.md                   # 本文档
│
├── engine/                     # ★ 引擎层：与平台、界面完全无关，可独立复用
│   ├── chess.h                 #   棋盘类对外接口：Move/Grid/枚举/坐标转换/走法与规则 API
│   ├── chess.cpp               #   实现：90 格数组棋盘、走法生成、将军/禁着/重复检测、
│   │                           #     Zobrist 增量哈希、make/undo 增量走子
│   ├── search.h                #   搜索引擎对外接口：SearchEngine、SearchInfo、常量
│   └── search.cpp              #   实现：迭代加深、PVS、置换表、静态搜索、LMR、
│                               #     杀手/历史启发、评估函数、开局库、时间管理
│
├── gui/
│   └── gui_main.cpp            # Win32 GDI 图形界面入口（单 exe）：窗口/消息循环/双缓冲
│                               #   绘制/鼠标交互/AI 后台线程/6 按钮/走子记录
├── console/
│   └── console_main.cpp        # 控制台入口：文字棋盘打印、指令解析、UTF-8 输出
│
├── server/
│   ├── server_main.cpp         # Web 服务入口：Winsock HTTP 服务器 + 极简 JSON + API 路由
│   └── web_resources.h         # 【自动生成】web/ 三个文件的内嵌副本（勿手改）
│
├── web/                        # 浏览器前端（规则零实现，只做展示与交互）
│   ├── index.html              #   页面骨架：canvas + 控制面板 + 终局弹窗
│   ├── style.css               #   界面样式
│   └── app.js                  #   Canvas 棋盘渲染、fetch 调 API、选子走子交互
│
├── botzone/
│   ├── botzone_main.cpp        # Botzone 平台 I/O 入口（依赖平台 jsoncpp）
│   └── botzone_submit.cpp      # 【自动生成】聚合单文件提交版（勿手改）
│
├── test/
│   └── selftest.cpp            # 引擎自检：AI 自对弈 N 步打印搜索信息
│
├── third_party/
│   └── jsoncpp_stub/           # jsoncpp 桩（仅本地编译 Botzone 版做语法检查用，
│                               #   不参与任何产物；真库由 Botzone 平台提供）
│
└── tools/
    ├── mingw64/                # WinLibs GCC 16.2.0 绿色版编译器（含 gdb、make 等）
    ├── gen_web_res.ps1         # 把 web/*.html/css/js 生成为 C++ 原始字符串 → web_resources.h
    └── make_botzone.ps1        # 把 chess.h/cpp + search.h/cpp + botzone_main.cpp 拼接
                                #   并剔除项目内 include → botzone_submit.cpp
```

**产物目录**（`build.bat` 生成）：

```
bin/
├── selftest.exe        # 引擎自检
├── chess_gui.exe       # 图形界面对弈（推荐，单文件）
├── chess_console.exe   # 控制台对弈（单文件）
└── chess_server.exe    # Web 对战服务（单文件，网页已内嵌）
```

---

## 6. 架构设计：为什么"一套引擎、四种前端"

```
                    ┌──────────────────────────────┐
                    │         engine/ 引擎层         │
                    │  Chess：棋盘 + 走法 + 规则      │
                    │  SearchEngine：搜索 + 评估      │
                    │ （不依赖任何界面/平台/网络库）      │
                    └──────────────┬───────────────┘
                                   │ 仅 include 头文件 + 链接两个 cpp
        ┌──────────────┬───────────┼───────────────┬──────────────┐
        ▼              ▼           ▼               ▼              ▼
   gui_main.cpp  console_main  server_main    botzone_main   selftest.cpp
   Win32 窗口     控制台循环     HTTP 服务      平台 I/O       自对弈
   后台线程搜索    同步搜索      单线程会话      重放+搜索      批量搜索
```

**设计原则**：

1. **规则只写一份**：走法合法性、将军、终局、禁着全部在 `engine/` 实现；
   GUI/控制台/Web 前端**零规则代码**，只调用引擎 API —— 避免多前端规则不一致的经典 bug；
2. **引擎与平台解耦**：`engine/` 不 include 任何 Windows / jsoncpp 头文件，
   同一份代码既能编译成 Botzone 平台提交版，也能编译成本地 GUI / 控制台 / 服务；
3. **增量式接口**：`makeMoveAssumeLegal / undoMove` 用 Undo 栈实现 O(1) 走子与悔棋，
   搜索、悔棋功能、Botzone 重放、Web 会话全部复用同一机制；
4. **每个入口只有一个职责**：入口文件只做"I/O 与界面"，不做棋。

**引擎层对外最常用的 API 一览**（各入口实际只用这几个）：

```cpp
Chess chess;                                      // 初始局面
chess.generateMovesWithForbidden(moves, true);    // 全部合法着法（已过滤送将/长将/三次重复）
chess.isLegalMoveWithForbidden(m);                // 校验一步是否合法
bool check = chess.moveGivesCheck(m);             // 该步是否形成将军（走之前查询）
chess.makeMoveAssumeLegal(m, true);               // 执行走子（recordCheck 记录将军信息）
chess.undoMove();                                 // 悔一步
chess.exceedMaxPeaceState();                      // 60 回合无吃子和棋？
chess.positionRepeatCount() >= 3                  // 三次重复和棋？
SearchEngine engine;  engine.setTimeLimitMs(800);
SearchInfo info;  Move m = engine.findBestMove(chess, info);   // AI 走一步（含分数/深度/节点数/耗时）
```

---

## 7. 坐标系统与记谱法

- 棋盘 `9 × 10`：列 `x ∈ [0, 8]` 对应字母 `a~i`，行 `y ∈ [0, 9]` 对应数字 `0~9`；
- 坐标字符串 = 列字母 + 行数字，如 `"h2"`、`"e9"`；着法 = 起点 + 终点，如红炮二平五 = `h2-e2`；
- **红方在 `y ∈ [0, 4]`（含九宫 `x∈[3,5], y∈[0,2]`），红先**；黑方在 `y ∈ [5, 9]`；
- 引擎内部一维存储：`pos = y * 9 + x`（`xy2pos`/`pos2x`/`pos2y`）；
- GUI/控制台/Web 三种前端在"执黑"时都只做**显示层翻转**，引擎坐标恒定不变——
  这样 AI、历史记录、悔棋逻辑完全不用关心视角。

---

## 8. 引擎技术详解（核心知识点）

> 本节是项目的技术核心，按"数据结构 → 走法生成 → 规则 → 搜索 → 评估"展开。
> 对应源码：`engine/chess.h/.cpp`（棋盘与规则）、`engine/search.h/.cpp`（搜索与评估）。

### 8.1 棋盘表示与数据结构（`Chess` 类）

**核心决策：一维数组 + 增量状态 + Undo 栈**。

- `std::array<Grid, 90> board`：一维数组存储，`pos = y * 9 + x`。
  一维比二维 `board[x][y]` 缓存友好，且索引计算简单；
- `Grid { stoneType type; colorType color; }`：每格 8 字节；
  `stoneType`: None/King/Bishop/Knight/Rook/Pawn/Cannon/Assistant；
  `colorType`: BLACK=0, RED=1, EMPTY=2；
- **增量维护的状态**（走子时 O(1) 更新，悔棋时从栈恢复）：

| 成员 | 作用 |
| ---- | ---- |
| `currColor` | 当前行棋方 |
| `currTurnId` | 总步数 |
| `peaceCount` | 连续无吃子回合数（判 60 回合和棋） |
| `kingPos[2]` | 双方将/帅位置缓存 → `isKingAttacked` O(1) 定位王，无需扫描 |
| `boardHash` | 增量维护的 Zobrist 哈希（走子异或更新，不重算） |

- **Undo 机制**（`UndoRecord` + `undoStack`）：`makeMoveAssumeLegal` 执行前把
  （着法、移动子、被吃子、行棋方、步数、和平计数、双王位置、哈希）整体压栈，
  `undoMove` 弹出恢复。**搜索树里百万次走子/回退不需要复制棋盘**，悔棋功能也直接复用；
- 历史辅助序列：
  - `stateKeys`：每步走完后的局面哈希序列（三次重复检测的回溯依据）；
  - `lastMoveEaten` / `lastMoveChecked`：每步是否吃子/将军（长将判定的回溯依据）。

### 8.2 Zobrist 哈希

**知识点：如何用 64 位哈希 O(1) 识别"同一个局面"。**

- 固定种子（`202605130971`）+ **splitMix64** 生成三张确定性的随机数表：
  `ZOBRIST[颜色][子种][90格]`、`ZOBRIST_SIDE`（行棋方）、`ZOBRIST_PEACE[64]`（无吃子计数）；
- 初始化时把全部棋子逐个异或进 `boardHash`；此后每步走子只异或
  "离开格、到达格、被吃子、行棋方、和平计数"这几项 → **增量更新**；
- 搜索键 `searchKey() = boardHash ^ side ^ PEACE[peaceCount]`——
  **同一个摆法但轮到不同方走、或和平进度不同，视为不同局面**，保证置换表与重复检测正确；
- 固定种子保证每次运行行为一致（可复现调试），splitMix64 保证随机质量。

### 8.3 走法生成与合法性检测

**伪合法 → 合法 → 无禁着，三层过滤。**

第一层 `generateMoves`（按子种生成伪合法走法，方向偏移表全局共享）：

| 子种 | 实现要点 |
| ---- | -------- |
| 帅/将 | 九宫内四方向一步（`inKingArea` 限定） |
| 士 | 九宫内四斜向一步 |
| 象 | 四斜向两格：先查**不过河**，再查**塞象眼**（田字中心） |
| 马 | 8 个"日"字目标：先查**蹩马腿**（直一格处），再查目标非己子 |
| 车 | 四方向直线滑行至出界/被挡，遇敌子生成吃子后停止 |
| 炮 | 平移同车；吃子用 `hasScreen` **状态机**：扫过第 1 个子后进入"架炮"态，遇到的下一个子若是敌子则可吃 |
| 兵/卒 | `crossedRiver` 判断：未过河只前进一步；过河后前进 + 左右横走 |

参数 `capturesOnly=true` 只生成吃子（供静态搜索避免横向爆炸）；`mustDefend=true` 触发第二层过滤。

第二层（合法性 = 不送将）：对每个候选走法执行 `make → 检查己方王是否被攻击 → undo`。
攻击检测 `attacked(color, x, y)` 是**反向探测**：

- 四方向直线扫描：第一个遇到的子是敌车 → 受攻；是敌炮且再隔一子 → 受攻；是敌王（同列照面）→ 受攻（飞将规则）；
- 马：从目标格反向查 8 个马位（注意**马腿在马那边**，与走法生成的腿位置对称）；
- 兵/卒：检查其"前进方向"与过河横击方向；士/象：仅在其活动范围（九宫/己方半场）内反向检查。

第三层 `generateMovesWithForbidden` = 合法着法 ∩ 非长将 ∩ 非三次重复（见下节），
供**根节点、人类走子校验、终局判定**使用；搜索树内部只需前两层（性能考量）。

### 8.4 禁着与和棋规则

| 规则 | 实现 | 逻辑 |
| ---- | ---- | ---- |
| 长将禁着 | `longCheckAfterMove` | 若该步**不吃子**且**将军**，且己方此前已连续走出 `MAX_CONSECUTIVE_QUIET_CHECKS`(=2) 步"无吃子将军"，则该步构成三连将，从合法着法中剔除（若该步直接取胜则放行） |
| 三次重复 | `repeatAfterMove` | 走完该步后，在当前**无吃子片段**内（沿 `lastMoveEaten` 回溯到上一吃子步），同奇偶步（即同一行棋方面对的）`stateKeys` 相同 key 出现 ≥3 次则剔除该着法 |
| 60 回合和棋 | `exceedMaxPeaceState` | `peaceCount >= PEACE_LIMIT(59)` 时：搜索直接返回 0 分（视为和棋），前端判定和局 |

### 8.5 搜索引擎（`SearchEngine`）

**主流程 `findBestMove`：**

1. 生成根节点合法着法并排序，默认取排序首位作为兜底（保证任何情况下都有的走）；
2. **开局库**查询（见下），命中即返回；
3. **迭代加深** `depth = 1, 2, 3, ...`：
   - 上一层结果着法优先搜索（`previousBest` 提到根着法列表首位）；
   - `depth >= 4` 起使用**渴望窗口**（aspiration window）：以 `lastScore ± 120` 为界搜，落窗则 `delta` 倍增重搜，`delta > 2000` 退化为全窗口；
   - 超时抛出 `SearchTimeout` 异常 → 终止加深，返回**上一个完整深度**的最佳着法；
   - 分数进入将杀区间（`|score| > MATE_SCORE - 1024`）提前结束（已经找到杀棋，不需要更深）。

**Negamax 框架**（双方视角统一为"当前行棋方分数最大化"，红黑对称免写双份）：

- 终止条件：无王（将杀，返回 `±(MATE_SCORE - ply)` —— **ply 修正保证优先走最短杀**）、
  60 回合和棋（返回 0）、达到 `MAX_SEARCH_PLY(128)`；
- **将军延伸**：`depth<=0` 且正被将军 → 强制 `depth=1`（必须解将后再进静态搜索，避免"将军"被当叶子误评）；
  走子后若形成将军且 `nextDepth <= 2` → `nextDepth++`（将军着法延伸一层）；
- **PVS（主变量搜索）**：第一个着法用全窗口 `(alpha, beta)` 搜，其余用零窗口 `(alpha, alpha+1)` 试探，
  仅当试探失败（`alpha < score < beta`）才用全窗口重搜——多数后续着法是坏的，零窗口裁剪效率极高；
- **LMR（Late Move Reductions）**：`depth>=3` 且非首着、非吃子、双方均不被将军时，
  第 4 着起减 1 层、第 10 着起减 2 层搜；零窗口试探意外失败则恢复原深度重搜——
  直觉：排序靠后的安静着法大概率不重要，赌错了再补搜；
- **置换表（TT）**：`2^19` 项，`key & TT_MASK` 直接寻址（免取模除法）；
  条目含 `{key, depth, value, flag(EXACT/LOWER/UPPER), bestMove}`；
  探查时区分边界类型：EXACT 直接返回；LOWER 只抬 alpha；UPPER 只压 beta；
  将杀分数存取时做 ply 修正（`scoreToTT`/`scoreFromTT`），否则"距杀 3 步"换个 ply 就变成"距杀 5 步"；
  替换策略：深度更浅 / 不同 key / EXACT 标志时覆盖；TT 命中着法作为 `hashMove` 交给排序器；
- **截断奖励**：beta 截断且非吃子 → 记杀手着法（每 ply 存 2 个）+ 历史启发累加 `depth² × 32`（超阈值全表减半防溢出）。

**静态搜索（quiescence）**——解决"水平线效应"（搜索在刚吃完子的时刻停住，评估严重失真）：

- 非将军局面：先算 stand-pat（当前静态评估）作下界截断；然后**只扩展吃子着法**；
  配合 **delta 剪枝**：`standPat + 被吃子价值 + 180 < alpha` 直接跳过（吃了也追不回来）；
  `qply >= 8` 硬上限防爆炸；
- 被将军局面：生成**全部**解将着法（不能只看吃子），无解则返回将杀分。

**走法排序（orderMoves）**——Alpha-Beta 的效率完全取决于剪枝顺序，分值从高到低：

1. TT 最佳着法（`+1e8`）
2. 吃子：MVV-LVA 风格 `1e7 + 被吃子价值×24 − 损失子价值`（"吃大子用小子"排前），吃王额外 `+5e7`
3. 杀手着法（`+9e6 / +8.5e6`，同层已验证能引发截断的安静着法）
4. 历史启发分（累计截断收益）
5. 位置微调：兵按推进/中路、马炮按中路、车小幅中路加分（让搜索初期倾向好着法）

**时间管理：**

- `std::chrono::steady_clock` 计时（单调钟，不受系统改时间影响）；
- **每 2048 个节点检查一次**超时（每次都查系统时钟太慢），超限抛 `SearchTimeout` 异常层层退出——
  用异常做超时控制是棋类引擎的常见做法，比每层判断标志位干净；
- Botzone 版：预算来自 JSON `time_limit`，`raw >= 100` 时取 `min(raw-70, raw×80%)`（留通信余量），
  否则 `max(30, raw-10)`，最后截断到 `[30, 5000]` ms；环境变量 `BOTZONE_AI_TIME_MS` 可覆盖；
- 本地版：`setTimeLimitMs` 直接设置（默认 800ms，自检用 200ms）。

**开局库（findOpeningBook）：**

- 仅在未被将军时启用；红方第 0 步候选 `h2e2`（中炮）、`b2e2`、`b0c2`（马八进七）、`h0g2`（马二进三），按序取第一个合法者（实际恒为 `h2e2`）；
- 黑方第 1 步候选 `h7e7`、`b7e7`、`b9c7`、`h9g7`；
- 若当前存在**大子被吃**（车/马/炮级）机会则放弃开局库——开局库只管前 1~2 步，救命要紧。

### 8.6 局面评估函数

对双方分别计分后取 `红 − 黑`，再按行棋方取正负（Negamax 视角），附加 **+12 先手（tempo）**、
当前正被将军 **−75**。

**子力基础分**：帅 20000 / 车 1000 / 炮 500 / 马 450 / 相·士 125 / 兵 105。
（帅的价值用"丢帅即输"的天文数字表达，让搜索自然把保帅放最高优先级。）

**各项位置/结构加成**（PST 思想：同样的子在不同的格子价值不同）：

| 子力 | 加成项 |
| ---- | ------ |
| 帅 | 中路小加成；不在肋线小惩罚；**肋道线受车/对将/隔子炮压制**时按 `filePressurePenalty` 扣分（100~140/85）；脱离本营且士象 ≤2 时 `-40`（王安全） |
| 车 | 机动性×6 + 中路×4 + 推进×2；过河 `+15` |
| 马 | 机动性×8 + 中路×10 + 推进×4；**四周每个蹩腿格 `-18`**；过河且近中路 `+12` |
| 炮 | 机动性×5 + 中路×8；过河 `+16`；初始巡河炮位 `+6` |
| 兵 | 推进×18 + 中路×5；**过河后大幅加分**（`+76 + 中路×13`），第 7 行以上再 `+35`，中兵 `+24` |
| 士/象 | 在位加成；**士象不成双**分别按 38/30 每缺一个惩罚 |

**整体结构分**：无车 `-65`；有车且炮马 ≥2 `+25`；双炮无马 `-20`；双马无炮 `+10`；无兵 `-25`；士象存量加成等。

**机动性 `mobilityAt`**：与走法生成同规则统计各子可落点数（车吃子点计 2、炮隔山打点计 2）。
这是评估中开销最大的部分——它让引擎天然倾向"活子多"的局面，但也决定了搜索速度的上限。

---

## 9. 各前端技术详解

### 9.1 Win32 GDI 图形界面（gui_main.cpp）

**知识点：Win32 窗口生命周期、GDI 绘图、双缓冲、后台线程 + 消息通信。**

**窗口骨架**（一切 Windows 图形程序的通用范式）：

```
WinMain
 ├─ RegisterClassW(WNDCLASSW)     注册窗口类（指定消息回调 wndProc）
 ├─ CreateWindowExW               创建窗口（AdjustWindowRect 先算好含边框的外形尺寸）
 ├─ createButtons                 创建 6 个 BUTTON 子控件（WM_SETFONT 设微软雅黑）
 ├─ ShowWindow / UpdateWindow
 └─ GetMessageW 消息循环           TranslateMessage + DispatchMessageW
```

- 入口必须是 `WinMain`（而非 `main`），配合编译选项 `-mwindows`（窗口子系统，不弹控制台黑框）；
- `AdjustWindowRect`：客户区要 812×648，但窗口外框/标题栏占尺寸，用它在创建前把窗口矩形"撑大"到位；
- 所有消息（绘制/鼠标/按钮/自定义）汇入 `wndProc` 的 `switch(msg)`，默认走 `DefWindowProcW`。

**双缓冲绘制（防闪烁）**：`WM_PAINT` 时不直接往屏幕 DC 画（几百个 GDI 图元会闪）：

```
BeginPaint → CreateCompatibleDC + CreateCompatibleBitmap（内存画布）
→ paint(mem) 全部画进内存 → BitBlt 一次性拷贝到屏幕 → 清理 GDI 对象 → EndPaint
```

GDI 对象（`HPEN/HBRUSH/HFONT/HBITMAP`）必须 `SelectObject` 换入换出并 `DeleteObject` 释放——
经典 RAII 缺席场景，泄漏了也不会立刻崩，所以代码里统一"旧对象先存后换"。

**棋盘绘制**（`paintBoard`）：MoveToEx/LineTo 画横竖线与九宫斜线（竖线在河界断开）、
画炮兵位交叉角标记、写"楚河漢界"、画上一步暗红小方块、金色圈选中高亮、
`Ellipse` 画圆形棋子（双圈描边）+ 居中汉字（帥仕相傌俥炮兵 / 將士象馬車砲卒）、
绿点/红环标注合法落点。

**视角翻转**：不转棋盘数据，只转**显示坐标**——

```cpp
pxX(x) = MARGIN + (flip ? (8 - x) : x) * CELL;      // 引擎坐标 → 像素
pxY(y) = MARGIN + (flip ? y : (9 - y)) * CELL;
pixelToCell()                                        // 反向：鼠标像素 → 引擎坐标
```

**AI 后台线程**（关键并发设计，界面不卡顿）：

```
maybeStartAI（主线程）
 ├─ new Chess(g_chess)          ★ 值拷贝一份局面快照传给线程——线程与 UI 完全无共享，天然线程安全
 ├─ CreateThread(aiThread, snapshot)
 └─ 禁用全部 6 按钮，状态栏"电脑思考中…"

aiThread（工作线程）
 ├─ g_engine.findBestMove(*snapshot, info)      800ms 搜索（只碰快照）
 ├─ delete snapshot
 └─ PostMessage(hwnd, WM_AI_DONE, 0, result)    ★ 结果用 new 出来的指针经消息队列交还主线程

wndProc 收 WM_AI_DONE（主线程）
 ├─ 取出 AiResult，恢复按钮
 └─ applyMove(r->move)                          走子、记录、终局判定，全部在主线程完成
```

- 自定义消息 `WM_APP + 1`；跨线程传结果的标准姿势：**堆上 new + PostMessage + 主线程 delete**，
  避免 `SendMessage`（跨线程直接调用有死锁风险）；
- **为什么快照可行**：`Chess` 是值语义（数组 + POD 成员 + vector Undo 栈），拷贝即完整局面；
  搜索线程持有拷贝，主线程继续响应 UI，互不干扰。

**其他实现细节**：

- 鼠标命中：`WM_LBUTTONDOWN` + `GET_X_LPARAM/GET_Y_LPARAM`（windowsx.h）拿像素 → `pixelToCell` 反算格子 →
  "点落点走子 / 点己子选中并过滤出该子的合法着法"；
- **走子记录必须用 `std::wstring` 拼接而不是 `swprintf(L"%s")`**——
  MinGW 的 `-std=c++14` 启用 ANSI 标准版宽字符 printf，`%s` 按**窄字符串**解析（MSVC 按宽解析），
  中文棋子名会乱码；这是 MinGW 与 MSVC 的著名差异坑；
- 按钮状态机：AI 思考中禁用全部 6 个；终局启用"再来一局"；悔棋后禁用"再来一局"；"重置"永远可用；
- 终局用 `MessageBoxW` 弹窗；字体统一 `CreateFontW` 微软雅黑（负高度 = 字符高度）。

### 9.2 控制台程序（console_main.cpp）

**知识点：UTF-8 控制台、主循环状态机、命令解析。**

- `SetConsoleOutputCP(CP_UTF8)`：Windows 终端默认 GBK，不设置则中文棋子乱码；
- `std::ios::sync_with_stdio(false)`：解除 C++ 流与 C stdio 同步，加速 cin/cout；
- 主循环是一个清晰的**状态机**：打印棋盘 → 终局判定（无着法/60 回合/重复）→
  轮到电脑则同步搜索走子（打印搜索信息）→ 轮到人则读指令分发（`undo`/`new`/`quit`/着法）；
- 执黑视角：打印时行序倒置（`y = 9 - i`）且列标倒排（`i h g f ...`），引擎坐标不变；
- 悔棋逻辑与 GUI 一致：人机模式连撤两步（先撤电脑应手，再撤玩家着法）。

### 9.3 Winsock HTTP 服务器（server_main.cpp）

**知识点：Socket 编程、HTTP/1.1 协议细节、极简 JSON、会话设计、单文件发布的资源嵌入。**

**服务器骨架**：

```
WSAStartup(MAKEWORD(2,2))         初始化 Winsock
socket(AF_INET, SOCK_STREAM)      TCP 套接字
setsockopt(SO_REUSEADDR)          快速重启时端口可复用（避免 TIME_WAIT 卡 2 分钟）
bind(127.0.0.1:8080)              ★ 只绑 INADDR_LOOPBACK，仅本机可访问（安全）
listen(8)
ShellExecuteA("open", url)        双击即用：自动打开默认浏览器
循环：accept → SO_RCVTIMEO 5s → handleRequest → closesocket
```

单线程顺序处理：本地单用户场景足够，且 `GameSession` 无需加锁；
引擎思考期间不接收新请求，前端以"电脑思考中"提示。

**HTTP 请求解析**（`readRequest`）处理了三个真实的协议坑：

1. **`Expect: 100-continue`**：部分客户端（如 .NET HttpWebRequest）发大 body 前先探路等 `100 Continue`，
   服务器不应答则**双方互等死锁**（单线程服务被永久堵死）→ 显式回 `HTTP/1.1 100 Continue\r\n\r\n`；
2. **`Content-Length` 定长读 body**：头与体按 `\r\n\r\n` 分隔，body 按 Content-Length 继续收满；
3. **防御**：头/体各限 64KB，坏连接靠 `SO_RCVTIMEO` 5 秒超时兜底，防半开连接耗尽单线程。

**安全与静态文件**：

- `safeStaticPath`：剥离查询串、拒绝含 `..` 的路径（**目录穿越攻击**防护）、默认补 `/index.html`；
- MIME 手写三行：`.html/.css/.js` → `text/html`、`text/css`、`application/javascript`（均 utf-8）；
- **磁盘优先、内嵌兜底**：开发时改 `web/` 目录立即生效；发布单文件 exe（目录不存在）时回退到内嵌资源。

**资源嵌入（单文件 exe 的秘密）**：`tools/gen_web_res.ps1` 把三个前端文件包成
C++ **原始字符串字面量**（raw string literal）写入 `web_resources.h`：

```cpp
static const char INDEX_HTML[] = R"html(<!DOCTYPE html>...)html";
```

原始字符串无需转义引号/反斜杠，HTML/CSS/JS 原文直接嵌入；脚本还会校验内容不含终止序列 `)html"`。

**极简 JSON**：不引第三方库——

- 输出用 `JsonOut`（拼接式：`key()` 自动补逗号、`value()` 转义引号反斜杠、`raw()` 嵌对象）；
- 输入用 `jsonFindString`（只提取 `"key":"value"` 字符串字段，够 `from/to/mode` 用）；
- 生产级正确性提示：这是**够用即可**的解析器，展示"按需造轮子"的边界。

**会话与 API 门禁**（`GameSession`）：

- 人机模式下 `/api/move` 校验"当前行棋方必须是人类执子方"、`/api/bestmove` 校验"必须是电脑执子方"——
  前后端**双向防呆**，绕过前端直接 curl 也不能替对方走棋；
- `/api/undo` 循环撤销直到"轮到人类"（人机模式一次 API 撤两步，pvp 撤一步）；
- `/api/bestmove` 有**双保险**：引擎返回的着法还要验证 source 格确有当前行棋方棋子，防止历史记录写入空编码；
- 每步记录 `{piece, from, to, capture, check}`（棋子编码如 `"rC"` = 红炮），前端直接展示。

### 9.4 浏览器前端（web/）

**知识点：Canvas 2D 绘图、fetch API、无框架 DOM 操作。**

- `index.html`：左侧 `<canvas id="board" width="592" height="656">`，右侧控制面板
  （模式下拉、新对局/悔棋/电脑代走按钮、引擎信息、走子记录 `<ol>`），底部终局弹窗卡片；
- `app.js`（Canvas 渲染 + 交互，**零规则代码**）：
  - 启动 `GET /api/state` 拉取局面；棋盘/棋子/标记全部用 Canvas 2D API 绘制，
    棋子用服务端编码（`"rC"` 等）映射字符；
  - 点击 → 换算成交叉点坐标 → 从 `state.legalMoves` **前端过滤**出该子着法并高亮
    （合法性判断本身仍由服务端完成，前端只是"筛选展示"）；
  - 走子 `POST /api/move`，电脑走棋 `POST /api/bestmove`，悔棋 `POST /api/undo`；
    每次操作后以响应中的 `state` 全量刷新界面（无状态同步难题）；
  - `state.result` 非空时弹出终局卡片；请求失败显示连接遮罩。

### 9.5 Botzone 平台适配（botzone_main.cpp）

**知识点：请求-响应式对战平台的"局面重建"范式。**

- Botzone 不保存程序内存：每回合重新启动程序，把**全部历史**（对手 `requests[]` + 己方 `responses[]`）以一行 JSON 喂入；
  程序 `getInputBotzone` 按序 `makeMoveAssumeLegal` **重放**在本地重建当前局面；
- `"source": "-1"` 表示 pass，重放时跳过；
- `parseTimeLimitMs`：按第 8.5 节公式把平台 `time_limit` 换算成搜索预算，赋给 `GLOBAL_TIME_LIMIT_MS`
  （`SearchEngine` 构造时读取）；
- `giveOutputBotzone`：合法着法为空输出 `"-1"/"-1"`；否则搜索，且输出前**再验证**着法在根着法列表内
  （不在则兜底取列表第一个），确保永不输出平台不认的着法；
- `tools/make_botzone.ps1` 把 5 个源文件拼接成单文件，并剔除项目内 `#include "chess.h"/"search.h"`（已内联）。

---

## 10. 构建系统详解

**build.bat 编译命令逐参数解读**（以 GUI 版为例）：

```bash
g++ -O2 -std=c++14 -mwindows -static -static-libgcc -static-libstdc++ \
    -Iengine gui/gui_main.cpp engine/chess.cpp engine/search.cpp \
    -o bin/chess_gui.exe
```

| 参数 | 含义 |
| ---- | ---- |
| `-O2` | 二级优化（棋类引擎对性能敏感，评估/搜索热点靠它提速数倍） |
| `-std=c++14` | 标准 C++14（`auto`、lambda、`std::array`、`std::to_wstring` 等） |
| `-mwindows` | Windows GUI 子系统：入口用 `WinMain`，运行不弹控制台黑框（仅 GUI 版） |
| `-static -static-libgcc -static-libstdc++` | 静态链接运行库：exe 不依赖 libstdc++-6.dll 等，**单文件可拷走** |
| `-Iengine` | 头文件搜索路径（各入口 `#include "search.h"` 直接命中） |
| `-lws2_32 -lshell32` | 服务器版链接 Winsock 与 ShellExecute（`#pragma comment` 是 MSVC 专属，MinGW 用命令行） |

**两个代码生成脚本**：

- `gen_web_res.ps1`：web 资源 → C++ 原始字符串 → `server/web_resources.h`
  （校验内容不含 `)xxx"` 终止序列；UTF-8 无 BOM 输出）；
- `make_botzone.ps1`：按 `chess.h → chess.cpp → search.h → search.cpp → botzone_main.cpp` 顺序拼接，
  剔除项目内双引号 include → `botzone_submit.cpp`。

**工程实践要点**（本项目踩过的坑，亦是知识点）：

- 编译前若 `chess_gui.exe` 正在运行会被锁定，需先结束进程再覆盖编译；
- `#pragma comment(lib, ...)` 属 MSVC 专属，MinGW 下需 `#ifdef _MSC_VER` 包裹否则 `-Wunknown-pragmas` 告警；
- 严格模式检查：`g++ -Wall -Wextra` 逐目标编译，项目当前**零警告**（含符号比较一致性处理）；
- VS Code 红色波浪线是 IntelliSense 误报（真实编译零警告），在 `.vscode/c_cpp_properties.json`
  中把 `compilerPath` 指向 `tools/mingw64/bin/g++.exe`、`includePath` 加上 `engine/` 即可消除。

---

## 11. HTTP API 完整文档

所有接口统一返回 `{"ok":true/false, "message":"...", "state":{...}}`。

| 接口 | 方法 | 请求体 | 说明 |
| ---- | ---- | ------ | ---- |
| `/api/state` | GET | — | 当前完整局面 |
| `/api/new` | POST | `{"mode":"pve_red"}` | 新对局；mode 亦可为 `pve_black` / `pvp` |
| `/api/move` | POST | `{"from":"h2","to":"e2"}` | 人类走子；非法/轮次错误返回 `ok:false` + 原因 |
| `/api/undo` | POST | `{}` | 悔棋；人机模式自动回退到轮到人类 |
| `/api/bestmove` | POST | `{}` | 引擎替当前行棋方（电脑方）走一步 |

`state` 结构：

```json
{
  "board":      ["rR", "", "bP", "..."],   // 90 格（下标 = y*9+x），"颜色+类型"编码，空格为 ""
  "current":    "red",                     // 当前行棋方 red/black
  "inCheck":    false,                     // 当前方面临将军
  "result":     "",                        // red_win / black_win / draw_peace / draw_repeat / ""
  "lastMove":   {"from":"h2","to":"e2"},   // 上一手（可为 null）
  "legalMoves": [{"from":"h2","to":"e2"}], // 当前方全部合法走法（服务端已过滤禁着）
  "history":    [{"piece":"rC","from":"h2","to":"e2","capture":"bP","check":true}],
  "engine":     {"move":"h2-e2","score":120,"depth":15,"nodes":521000,"ms":800}
}
```

棋子编码：首字符颜色 `r`/`b`，次字符类型 `K`(帅) `A`(士) `B`(象) `N`(马) `R`(车) `C`(炮) `P`(兵)。

静态文件：`GET /` → index.html，`GET /style.css`、`GET /app.js`（磁盘 web/ 目录优先，单文件 exe 回退内嵌）。

---

## 12. Botzone 交互协议

### 12.1 输入（一行 JSON）

```json
{
  "requests":  [{"source":"h2","target":"h9"}, "..."],
  "responses": [{"source":"h9","target":"e9"}, "..."],
  "time_limit": 1000
}
```

- `requests[i]`：第 i 回合**对手**（或我方执红时的空过）的着法；`responses[i]`：我方第 i 回合的应手；
- 程序按序重放 `requests + responses` 在本地重建完整局面（`getInputBotzone`）；
- `"source": "-1"` 表示 pass，重放跳过；
- `time_limit`：本步时限（毫秒），扣减安全余量后作为搜索预算。

### 12.2 输出（一行 JSON）

```json
{"response": {"source": "h2", "target": "e2"}}
```

无任何合法着法时 `source`/`target` 均输出 `"-1"`。

### 12.3 坐标约定

- 列 `x∈[0,8]` ↔ 字母 `a~i`；行 `y∈[0,9]` ↔ 数字；红方 `y∈[0,4]`，**红先**；
- 例：红炮二平五 = `"h2" → "e2"`。

---

## 13. 常量与参数速查表

| 常量 | 值 | 含义 |
| ---- | -- | ---- |
| `DEFAULT_TIME_MS` | 800 | 默认搜索时限（ms） |
| `PEACE_LIMIT` | 59 | 无吃子回合达到该值判和（搜索返回 0） |
| `MAX_CONSECUTIVE_QUIET_CHECKS` | 2 | 己方连续无吃子将军步数上限，超出后继续"不吃子将军"被禁 |
| `MAX_SEARCH_PLY` | 128 | 搜索最大深度层数 |
| `TT_BITS / TT_SIZE` | 19 / 2^19 | 置换表索引位数与容量 |
| `MATE_SCORE` | 10000000 | 将杀基准分（ply 修正） |
| `INF_SCORE` | 1000000000 | 无穷大分数 |
| 渴望窗口 delta | 120 | 初始窗口半宽，失败倍增，>2000 退化全窗口 |
| 静态搜索 qply 上限 | 8 | 静态搜索最大扩展深度 |
| delta 剪枝余量 | 180 | 静态搜索吃子剪枝缓冲 |
| LMR 触发 | depth≥3，第 4/10 着起 | 安静着法减 1/2 层 |
| 时间检查粒度 | 每 2048 节点 | 超时检查频率 |
| GUI 布局 | CELL=56 / MARGIN=40 / PIECE_R=26 / SIDE_W=250 | 交叉点间距 / 边距 / 棋子半径 / 侧栏宽（像素） |
| HTTP 限制 | 头/体各 64KB，收包超时 5s | 单线程服务的防御参数 |

---

## 14. 知识点地图（教学索引）

本项目覆盖的知识点 ↔ 代码位置对照（按学习路线排序）：

| 知识领域 | 知识点 | 代码位置 |
| -------- | ------ | -------- |
| C++ 基础 | 枚举/结构体/类封装、`std::array/vector/string`、值语义拷贝 | `engine/chess.h` |
| C++ 进阶 | 增量更新 + Undo 栈（命令模式）、异常做超时控制、lambda/`auto` | `chess.cpp`、`search.cpp` |
| 数据结构 | Zobrist 哈希、置换表（直接寻址 + 替换策略）、杀手着法、历史启发表 | `search.cpp` |
| 算法 | 迭代加深、Negamax + Alpha-Beta、PVS、渴望窗口、静态搜索、LMR、MVV-LVA、delta 剪枝、将军延伸、短杀优先（mate-ply） | `search.cpp` |
| 博弈规则建模 | 伪合法→合法→无禁着三层过滤、反向攻击探测、长将/重复/无吃子和棋 | `chess.cpp` |
| 评估函数设计 | 子力 + 机动性 + PST + 王安全 + 结构分、tempo | `search.cpp` |
| Win32 编程 | 窗口类注册/消息循环/`WndProc`、GDI 画笔刷字体、双缓冲、`WM_APP` 自定义消息、`CreateThread` + `PostMessage` 线程通信、`AdjustWindowRect`、`MessageBoxW` | `gui/gui_main.cpp` |
| Windows 平台坑 | MinGW vs MSVC 宽字符 `%s` 差异（乱码根因）、控制台 UTF-8 代码页、`-mwindows`、静态链接 | `gui_main.cpp`、`console_main.cpp` |
| 网络编程 | Winsock 全流程、`SO_REUSEADDR`/`SO_RCVTIMEO`、仅监听回环地址 | `server_main.cpp` |
| HTTP 协议 | 请求行/头/体解析、`Content-Length`、`Expect: 100-continue`、MIME、目录穿越防护 | `server_main.cpp` |
| 前端 | Canvas 2D、fetch + JSON、DOM 事件、服务端权威 + 全量状态刷新 | `web/` |
| 工程实践 | 一引擎多前端架构、代码生成/内嵌（raw string literal）、单文件聚合提交、批处理/PowerShell 构建脚本、`-Wall -Wextra` 零警告、头文件桩 | 全项目 |

**推荐阅读顺序**：`engine/chess.h` → `engine/chess.cpp`（规则）→ `engine/search.h` → `engine/search.cpp`（搜索）
→ `test/selftest.cpp`（最薄入口示例）→ `console_main.cpp` → `gui_main.cpp` → `server_main.cpp` + `web/` → `botzone_main.cpp`。

---

## 15. 已知行为与注意事项

- **重复检测基于 `boardHash`**：状态序列按同奇偶步比较（等效"同一行棋方"），
  且只回溯到最近一次吃子，符合 Botzone 长打判定习惯；未存储完整局面，哈希碰撞理论存在但概率极低；
- **长将判定是启发式**：仅统计"连续无吃子将军"步数，未覆盖长捉（无吃子威胁）等更复杂禁着；
- **搜索内合法性**：搜索使用 `makeMoveAssumeLegal` 假定合法 + 生成时已过滤自将，搜索树内不会遇到非法局面；
- **根节点兜底**：搜索返回的着法若意外不在根着法列表中，回退为排序后的第一个合法着法；
  完全无合法着法时输出 `-1,-1`（Botzone 版）或直接判定终局（本地版）；
- **本地版会话**：单进程单对局、单线程顺序处理请求（本地单用户场景足够）；
  引擎思考期间不接收新 HTTP 请求（accept 后阻塞处理），Web 前端按钮此时置灰并显示"电脑思考中…"，
  请求排队到思考结束后依次处理，不会并发破坏棋盘状态；
- **悔棋与置换表**：undoMove 后置换表仍保留旧局面条目。表项以完整哈希键校验，错误命中的概率极低，
  且置换表只影响搜索效率、不改变 legality 判定，因此悔棋后对局正确性不受影响；
  若想彻底清空，重开一局（new 按钮 / `new` 指令 / `/api/new`）即重建 Chess 对象；
- **GUI 为单实例对局**：窗口关闭即退出进程；AI 思考在子线程，主线程只负责绘制，
  若思考中直接关闭窗口，进程正常退出（线程随主线程结束）；
- **Web 前端依赖本地服务**：index.html 通过 `fetch('/api/*')` 与服务端同源通信，
  直接双击打开 HTML 文件（file:// 协议）无法对局，必须通过 `chess_server.exe` 或 `start_server.bat` 访问。

---

## 16. 常见问题 FAQ

### 16.1 编译与运行环境

**Q1：双击 exe 时杀毒软件 / Windows Defender 报毒或删除文件？**

静态链接（`-static`）的 MinGW 程序偶尔被误报。处理方式：
- 在杀软中添加信任/排除目录 `d:\ChineseChess\`；
- 或从源码自行编译（`build.bat`），保证产物可信；
- 若 Defender 已隔离文件，先恢复并加白名单，再重新运行 `build.bat`。

**Q2：提示 "mingw32-g++.exe 不是内部或外部命令" 或编译失败？**

未安装 MinGW 或未加入 PATH。安装 TDM-GCC / MinGW-w64（含 g++），确保命令行执行 `g++ --version` 有输出。
Dev-C++ 自带的 MinGW 也可用（本项目最初即面向 Dev-C++ 环境）。

**Q3：`gen_web_res.ps1` 无法运行，报 "禁止运行脚本"？**

PowerShell 默认执行策略为 Restricted。在 build.bat 所用窗口执行：
`powershell -ExecutionPolicy Bypass -File tools\gen_web_res.ps1`（build.bat 已内置此参数）；
若手动运行脚本遇到限制，同样加 `-ExecutionPolicy Bypass`。

**Q4：编译时提示 exe 被占用（Permission denied / 无法写入）？**

正在运行的 chess_gui.exe / chess_console.exe / chess_server.exe 锁定了输出文件。
先关闭对应窗口（GUI 点 X；控制台输入 quit；服务端关闭其控制台窗口或 Ctrl+C），再重新编译。

**Q5：端口 8000 被占用，服务器启动失败？**

关闭占用进程，或修改 `server_main.cpp` 中 `PORT` 常量后重编译；
浏览器访问地址随之变为 `http://127.0.0.1:<新端口>/`。

### 16.2 显示与编码

**Q6：控制台版中文乱码？**

程序已调用 `SetConsoleOutputCP(CP_UTF8)`，源码与输出均为 UTF-8。
若仍乱码，右键控制台标题栏 → 属性 → 字体改为"新宋体/Consolas"，
或执行 `chcp 65001` 切换代码页；Windows Terminal 默认即正常。

**Q7：GUI 走子记录 / 悔棋提示出现乱码（如 "e…è½¦"）？**

此为 MinGW 的 `swprintf` 在 `-std=c++14` 下按 ANSI 标准解析 `%s`（窄字符串）导致的经典问题。
代码已通过 `std::wstring` 拼接规避；若自行改动代码引入中文，
务必使用 `std::wstring` + `L"..."` 拼接，不要向 `swprintf` 传窄字符串。

**Q8：VS Code 里大量红色波浪线，但编译完全正常？**

IntelliSense 未找到编译器与头文件路径，属于编辑器误报（本项目已 `-Wall -Wextra` 零警告）。
在项目根目录创建 `.vscode/c_cpp_properties.json`：

```json
{
  "configurations": [{
    "name": "Win32",
    "compilerPath": "C:/TDM-GCC-64/bin/g++.exe",
    "includePath": ["${workspaceFolder}/engine"],
    "cStandard": "c11",
    "cppStandard": "c++14",
    "intelliSenseMode": "windows-gcc-x64"
  }],
  "version": 4
}
```

`compilerPath` 按实际 g++ 安装路径修改（命令行 `where g++` 查询）。

### 16.3 玩法与功能

**Q9：如何修改 AI 思考时间（棋力）？**

- GUI / 控制台：`search.h` 中 `DEFAULT_TIME_MS`（默认 800ms），改大变强、改小变快；
- Web 版：请求 `POST /api/move` 时传 `"timeMs": 3000`；
- Botzone：由对局方剩余时间自动计算（见 §12），无需修改。

**Q10：为什么悔棋一次退回两步？**

人机对战中"一手"指双方各走一步。悔棋撤销 AI 的最后一手与玩家的上一手，
回到玩家上一次落子前的状态，保证仍轮到玩家走。

**Q11：GUI 有音效和动画吗？**

当前版本以稳定与零依赖为优先，仅保留立即落子 + 状态栏/弹窗提示（早期 Web 版曾内置音效，后为单文件精简移除）。
如需扩展，可在 WM_LBUTTONUP 落子处调用 `PlaySound`/`MessageBeep`（Win32）或 Web Audio API（网页版）。

**Q12：如何修改棋盘颜色、棋子大小、字体？**

均在 `gui_main.cpp` 顶部常量区：`CELL`（格距）、`PIECE_R`（棋子半径）、`MARGIN`（边距）、`SIDE_W`（侧栏宽）；
颜色在 `WM_PAINT` 的 `CreatePen`/`CreateSolidBrush` 调用处；字体为 `Microsoft YaHei`，可改成系统内其它字体名。

**Q13：可以人人对战（双人同机）吗？**

当前四个前端均为人机对战。引擎侧不限制行棋方，改造思路：
GUI/控制台跳过 AI 线程直接走子即可；Web 版可将 `/api/move` 改为回显落子。
属于良好练习，留作扩展。

**Q14：支持打开/保存棋谱（SGN/PGN）吗？**

暂未实现。走子记录已在 GUI 侧栏与 `MoveRecord`（chess.h）中维护，
可基于 `getMoveHistory` 输出 ICCS 坐标串（如 `h2e2`）实现导出，留作扩展。

### 16.4 Botzone 相关

**Q15：Botzone 提交后超时或报 WA？**

- 确认提交的是 `botzone_submit.cpp`（make_botzone.ps1 生成的单文件聚合版），不要提交源码目录中的分文件；
- 首回合需要 `0 0` 初始化已内置；若对手请求格式变化，检查 `getInputBotzone` 的解析逻辑；
- 长将/长打判负由 Botzone 裁决，本程序已按其习惯做三层禁着过滤。

**Q16：本地测试 Botzone 逻辑？**

`chess_console.exe` 与 Botzone 版共享同一引擎与协议思路：
控制台按 `turn`/`request` 逐行输入即可模拟 Botzone IO（见 §4.2 指令表）。

---

> 本项目以教学为目的，代码刻意保持"可读优先"：引擎约 2000 行、全部前端各约 500 行，
> 适合逐文件阅读与魔改。欢迎在此基础上加入开局库、NNUE 评估、UCI 协议、联机对战等进阶特性。

（完）
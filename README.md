[PROJECT_DOC.md](https://github.com/user-attachments/files/26895296/PROJECT_DOC.md)
# 踩地雷 Minesweeper (Flask × C)

> 1142 學期計算機程式語言期末專題:以 Python Flask 作為 Web 前端、C 語言作為核心引擎，完成可在瀏覽器遊玩的踩地雷。  
> 本專案版型與章節配置比照老師示範 README。

---

## 專題功能介紹

一個可在瀏覽器遊玩的經典踩地雷遊戲:

- 自訂棋盤大小（5×5 ~ 30×30）與地雷數
- 左鍵翻開格子、右鍵插旗
- 踩到 0 的格子時，採 BFS flood-fill 自動連鎖展開
- 勝負判定、地雷顯示、新局重啟
- 支援 Undo（上一步）與操作歷史顯示

核心遊戲邏輯（棋盤、翻開、展開、勝負）**完全由 C 語言實作**；  
Python Flask 僅負責 HTTP 路由、呼叫 C backend 與回傳 JSON。

---

## 使用技術

| 層級 | 技術 | 核心能力展示 |
|---|---|---|
| 前端 UI | HTML + CSS + Vanilla JS | DOM 操作、fetch API、互動式棋盤渲染 |
| Web 後端 | Python 3 + Flask | REST 路由、JSON 序列化、session 管理 |
| 語言整合 | subprocess | Flask 呼叫 C 執行檔並交換 JSON |
| 遊戲核心 | **C (C11)** | **pointer、malloc/free、struct、函式拆分** |
| 資料結構 | **動態二維陣列 + Queue + Linked List + Stack** | BFS 展開、歷史紀錄、Undo |

### C 核心引擎：符合規範重點

```c
typedef struct Cell {
    int isMine;
    int isRevealed;
    int isFlagged;
    int neighborMines;
} Cell;
```

- **Pointer**：`Cell **board` 為 pointer-to-pointer，用於動態二維陣列。
- **malloc / free**：配置列指標與每列 Cell，釋放順序反向，遵守「有多少 malloc 就有多少 free」。
- **struct**：`Cell`、`ActionNode`、`UndoNode`、`QueueNode` 皆以 struct 封裝。
- **函式拆分**：`allocate_board`、`free_board`、`flood_reveal`、`push_history`、`push_undo_snapshot` 等皆為單一職責。

---

## 系統架構與執行方式

### 架構圖

```
┌─────────────┐   HTTP (JSON)   ┌──────────────┐   subprocess   ┌──────────────────┐
│   Browser   │ ◄─────────────► │  Flask app   │ ◄────────────► │ mine_backend.exe │
│  (HTML/JS)  │                 │   (Python)   │                │       (C)        │
└─────────────┘                 └──────────────┘                └──────────────────┘
```

### 目錄結構

```text
project/
├── app.py                  # Flask 路由與 API
├── mine_backend.c          # C 核心邏輯
├── templates/
│   └── index.html          # 前端頁面
├── requirements.txt
└── PROJECT_DOC.md
```

### 如何執行（How to run）

#### 1. 安裝 Python 依賴

```bash
pip install -r requirements.txt
```

#### 2. 啟動 Flask

```bash
python app.py
```

#### 3. 開啟瀏覽器

```
http://127.0.0.1:8000
```

#### 4. 自訂 Port（選用）

```bash
set PORT=5050
python app.py
```

---

## 對外 API（Flask ↔ C）

| Method | Path | Body | 回應 |
|---|---|---|---|
| POST | `/api/new` | `{rows, cols, mines, seed}` | 新盤面 JSON |
| POST | `/api/open` | `{row, col}` | 更新後盤面 |
| POST | `/api/flag` | `{row, col}` | 更新後盤面 |
| POST | `/api/undo` | `{}` | Undo 後盤面 |

### 回傳資料欄位

| 欄位 | 型別 | 說明 |
|---|---|---|
| `rows`, `cols` | number | 棋盤大小 |
| `game_over` | boolean | 是否結束 |
| `win` | boolean | 是否勝利 |
| `can_undo` | boolean | 是否可 Undo |
| `history` | string[] | 操作歷史 |
| `board` | string[][] | 棋盤顯示內容 |

---

## Cursor Prompt 使用紀錄

本專題開發流程遵循「先架構、再實作、最後驗證」的方式。關鍵提示詞如下:

### 1. 規劃與架構
> 「我要做一個 Flask 前端 + C 後端的踩地雷，先給我分層架構，不要直接丟整包程式。」

### 2. 單功能逐步實作
> 「請實作 `Cell`、動態二維陣列，並補上 queue 做 BFS 自動展開。」
>
> 「再加入 linked list 操作歷史與 stack undo。」

### 3. 精準除錯
> 「檢查 malloc/free 是否對稱，避免 memory leak 與越界。」
>
> 「確認 API 回傳 JSON 格式一致，前端可直接渲染。」

---

## 可選擴充（加分項目）

- [ ] 多步 Undo / Redo
- [ ] 計時器與最佳紀錄
- [ ] 首點保證不踩雷（first-click safe）
- [ ] 多使用者 session 對戰模式

---

## 常見問題

### Flask 套件未安裝

若出現 `ModuleNotFoundError: No module named 'flask'`：

```bash
pip install -r requirements.txt
```

### C backend 編譯失敗

請先確認系統可執行 `gcc --version`，若無請安裝 `mingw-w64`（Windows）或 gcc 工具鏈（Linux）。

### 網址打不開

請確認終端是否顯示 `Running on http://127.0.0.1:8000`，再開啟瀏覽器連線。

---

## 授權

教學與作業展示用途。

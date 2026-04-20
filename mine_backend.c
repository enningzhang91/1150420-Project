#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_SIZE 30
#define ACTION_LEN 8

typedef struct Cell {
    int isMine;
    int isRevealed;
    int isFlagged;
    int neighborMines;
} Cell;

typedef struct ActionNode {
    char action[ACTION_LEN];
    int row;
    int col;
    struct ActionNode *next;
} ActionNode;

typedef struct Snapshot {
    int gameOver;
    int win;
    int revealedCount;
    Cell **board;
} Snapshot;

typedef struct UndoNode {
    Snapshot snapshot;
    struct UndoNode *next;
} UndoNode;

typedef struct QueueNode {
    int row;
    int col;
    struct QueueNode *next;
} QueueNode;

typedef struct {
    int rows;
    int cols;
    int mines;
    int gameOver;
    int win;
    int revealedCount;
    Cell **board;
    ActionNode *historyHead;
    ActionNode *historyTail;
    int historyCount;
    UndoNode *undoTop;
} GameState;

static void fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static int in_bounds(const GameState *g, int r, int c) {
    return r >= 0 && r < g->rows && c >= 0 && c < g->cols;
}

static Cell **allocate_board(int rows, int cols) {
    Cell **board = (Cell **)malloc((size_t)rows * sizeof(Cell *));
    if (!board) {
        fail("Allocation failed for board pointers.");
    }

    for (int r = 0; r < rows; r++) {
        board[r] = (Cell *)calloc((size_t)cols, sizeof(Cell));
        if (!board[r]) {
            for (int i = 0; i < r; i++) {
                free(board[i]);
            }
            free(board);
            fail("Allocation failed for board rows.");
        }
    }
    return board;
}

static void free_board(Cell **board, int rows) {
    if (!board) {
        return;
    }
    for (int r = 0; r < rows; r++) {
        free(board[r]);
    }
    free(board);
}

static Cell **clone_board(Cell **src, int rows, int cols) {
    Cell **copied = allocate_board(rows, cols);
    for (int r = 0; r < rows; r++) {
        memcpy(copied[r], src[r], (size_t)cols * sizeof(Cell));
    }
    return copied;
}

static void clear_history(GameState *g) {
    ActionNode *cur = g->historyHead;
    while (cur) {
        ActionNode *next = cur->next;
        free(cur);
        cur = next;
    }
    g->historyHead = NULL;
    g->historyTail = NULL;
    g->historyCount = 0;
}

static void push_history(GameState *g, const char *action, int row, int col) {
    ActionNode *node = (ActionNode *)malloc(sizeof(ActionNode));
    if (!node) {
        fail("Allocation failed for history node.");
    }
    snprintf(node->action, ACTION_LEN, "%s", action);
    node->row = row;
    node->col = col;
    node->next = NULL;

    if (!g->historyHead) {
        g->historyHead = node;
        g->historyTail = node;
    } else {
        g->historyTail->next = node;
        g->historyTail = node;
    }
    g->historyCount++;
}

static void clear_undo(GameState *g) {
    UndoNode *cur = g->undoTop;
    while (cur) {
        UndoNode *next = cur->next;
        free_board(cur->snapshot.board, g->rows);
        free(cur);
        cur = next;
    }
    g->undoTop = NULL;
}

static void push_undo_snapshot(GameState *g) {
    UndoNode *node = (UndoNode *)malloc(sizeof(UndoNode));
    if (!node) {
        fail("Allocation failed for undo node.");
    }
    node->snapshot.gameOver = g->gameOver;
    node->snapshot.win = g->win;
    node->snapshot.revealedCount = g->revealedCount;
    node->snapshot.board = clone_board(g->board, g->rows, g->cols);
    node->next = g->undoTop;
    g->undoTop = node;
}

static int pop_undo_snapshot(GameState *g) {
    if (!g->undoTop) {
        return 0;
    }
    UndoNode *node = g->undoTop;
    g->undoTop = node->next;

    free_board(g->board, g->rows);
    g->board = node->snapshot.board;
    g->gameOver = node->snapshot.gameOver;
    g->win = node->snapshot.win;
    g->revealedCount = node->snapshot.revealedCount;
    free(node);
    return 1;
}

static void init_queue(QueueNode **head, QueueNode **tail) {
    *head = NULL;
    *tail = NULL;
}

static void enqueue(QueueNode **head, QueueNode **tail, int row, int col) {
    QueueNode *node = (QueueNode *)malloc(sizeof(QueueNode));
    if (!node) {
        fail("Allocation failed for queue node.");
    }
    node->row = row;
    node->col = col;
    node->next = NULL;
    if (!*tail) {
        *head = node;
        *tail = node;
    } else {
        (*tail)->next = node;
        *tail = node;
    }
}

static int dequeue(QueueNode **head, QueueNode **tail, int *row, int *col) {
    if (!*head) {
        return 0;
    }
    QueueNode *node = *head;
    *row = node->row;
    *col = node->col;
    *head = node->next;
    if (!*head) {
        *tail = NULL;
    }
    free(node);
    return 1;
}

static void clear_queue(QueueNode **head, QueueNode **tail) {
    int row = 0, col = 0;
    while (dequeue(head, tail, &row, &col)) {
    }
}

static void reveal_all_mines(GameState *g) {
    for (int r = 0; r < g->rows; r++) {
        for (int c = 0; c < g->cols; c++) {
            if (g->board[r][c].isMine) {
                g->board[r][c].isRevealed = 1;
            }
        }
    }
}

static void check_win(GameState *g) {
    int safeCells = g->rows * g->cols - g->mines;
    if (g->revealedCount == safeCells && !g->gameOver) {
        g->win = 1;
        g->gameOver = 1;
        reveal_all_mines(g);
    }
}

static void compute_neighbors(GameState *g) {
    int dr[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int dc[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

    for (int r = 0; r < g->rows; r++) {
        for (int c = 0; c < g->cols; c++) {
            int count = 0;
            for (int i = 0; i < 8; i++) {
                int nr = r + dr[i];
                int nc = c + dc[i];
                if (in_bounds(g, nr, nc) && g->board[nr][nc].isMine) {
                    count++;
                }
            }
            g->board[r][c].neighborMines = count;
        }
    }
}

static void place_mines(GameState *g, int seed) {
    int total = g->rows * g->cols;
    int placed = 0;
    if (seed == 0) {
        seed = (int)time(NULL);
    }
    srand(seed);

    while (placed < g->mines) {
        int pos = rand() % total;
        int r = pos / g->cols;
        int c = pos % g->cols;
        if (!g->board[r][c].isMine) {
            g->board[r][c].isMine = 1;
            placed++;
        }
    }
}

static void flood_reveal(GameState *g, int startRow, int startCol) {
    int dr[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int dc[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

    QueueNode *head = NULL;
    QueueNode *tail = NULL;
    init_queue(&head, &tail);
    enqueue(&head, &tail, startRow, startCol);

    int row = 0, col = 0;
    while (dequeue(&head, &tail, &row, &col)) {
        Cell *cell = &g->board[row][col];
        if (cell->isRevealed || cell->isFlagged) {
            continue;
        }

        cell->isRevealed = 1;
        g->revealedCount++;
        if (cell->neighborMines != 0) {
            continue;
        }

        for (int i = 0; i < 8; i++) {
            int nr = row + dr[i];
            int nc = col + dc[i];
            if (in_bounds(g, nr, nc) && !g->board[nr][nc].isRevealed && !g->board[nr][nc].isMine) {
                enqueue(&head, &tail, nr, nc);
            }
        }
    }

    clear_queue(&head, &tail);
}

static void free_game(GameState *g) {
    free_board(g->board, g->rows);
    g->board = NULL;
    clear_history(g);
    clear_undo(g);
}

static void init_game(GameState *g, int rows, int cols, int mines, int seed) {
    if (rows < 5 || rows > MAX_SIZE || cols < 5 || cols > MAX_SIZE) {
        fail("Rows/Cols must be between 5 and 30.");
    }
    if (mines < 1 || mines >= rows * cols) {
        fail("Mines count is invalid.");
    }

    memset(g, 0, sizeof(GameState));
    g->rows = rows;
    g->cols = cols;
    g->mines = mines;
    g->board = allocate_board(rows, cols);
    place_mines(g, seed);
    compute_neighbors(g);
}

static void open_cell(GameState *g, int row, int col) {
    if (!in_bounds(g, row, col) || g->gameOver) {
        return;
    }
    Cell *cell = &g->board[row][col];
    if (cell->isFlagged || cell->isRevealed) {
        return;
    }

    push_undo_snapshot(g);
    push_history(g, "open", row, col);

    if (cell->isMine) {
        cell->isRevealed = 1;
        g->gameOver = 1;
        g->win = 0;
        reveal_all_mines(g);
        return;
    }

    flood_reveal(g, row, col);
    check_win(g);
}

static void toggle_flag(GameState *g, int row, int col) {
    if (!in_bounds(g, row, col) || g->gameOver) {
        return;
    }
    Cell *cell = &g->board[row][col];
    if (cell->isRevealed) {
        return;
    }

    push_undo_snapshot(g);
    push_history(g, "flag", row, col);
    cell->isFlagged = !cell->isFlagged;
}

static void save_state(const char *path, const GameState *g) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fail("Cannot open state file for write.");
    }

    fprintf(f, "%d %d %d %d %d %d\n", g->rows, g->cols, g->mines, g->gameOver, g->win, g->revealedCount);
    for (int r = 0; r < g->rows; r++) {
        for (int c = 0; c < g->cols; c++) {
            const Cell *cell = &g->board[r][c];
            fprintf(f, "%d %d %d %d\n", cell->isMine, cell->isRevealed, cell->isFlagged, cell->neighborMines);
        }
    }

    fprintf(f, "HISTORY %d\n", g->historyCount);
    ActionNode *cur = g->historyHead;
    while (cur) {
        fprintf(f, "%s %d %d\n", cur->action, cur->row, cur->col);
        cur = cur->next;
    }

    if (g->undoTop) {
        fprintf(f, "UNDO 1\n");
        fprintf(f, "%d %d %d\n", g->undoTop->snapshot.gameOver, g->undoTop->snapshot.win, g->undoTop->snapshot.revealedCount);
        for (int r = 0; r < g->rows; r++) {
            for (int c = 0; c < g->cols; c++) {
                const Cell *cell = &g->undoTop->snapshot.board[r][c];
                fprintf(f, "%d %d %d %d\n", cell->isMine, cell->isRevealed, cell->isFlagged, cell->neighborMines);
            }
        }
    } else {
        fprintf(f, "UNDO 0\n");
    }

    fclose(f);
}

static void load_state(const char *path, GameState *g) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fail("No game state found. Please create a new game.");
    }

    memset(g, 0, sizeof(GameState));
    if (fscanf(f, "%d %d %d %d %d %d", &g->rows, &g->cols, &g->mines, &g->gameOver, &g->win, &g->revealedCount) != 6) {
        fclose(f);
        fail("State file is invalid.");
    }
    g->board = allocate_board(g->rows, g->cols);

    for (int r = 0; r < g->rows; r++) {
        for (int c = 0; c < g->cols; c++) {
            Cell *cell = &g->board[r][c];
            if (fscanf(f, "%d %d %d %d", &cell->isMine, &cell->isRevealed, &cell->isFlagged, &cell->neighborMines) != 4) {
                fclose(f);
                free_game(g);
                fail("State file is invalid.");
            }
        }
    }

    char section[16];
    int historyCount = 0;
    if (fscanf(f, "%15s %d", section, &historyCount) != 2 || strcmp(section, "HISTORY") != 0) {
        fclose(f);
        free_game(g);
        fail("State file is invalid.");
    }

    for (int i = 0; i < historyCount; i++) {
        char action[ACTION_LEN];
        int row = 0, col = 0;
        if (fscanf(f, "%7s %d %d", action, &row, &col) != 3) {
            fclose(f);
            free_game(g);
            fail("State file is invalid.");
        }
        push_history(g, action, row, col);
    }

    int hasUndo = 0;
    if (fscanf(f, "%15s %d", section, &hasUndo) == 2 && strcmp(section, "UNDO") == 0 && hasUndo == 1) {
        UndoNode *node = (UndoNode *)malloc(sizeof(UndoNode));
        if (!node) {
            fclose(f);
            free_game(g);
            fail("Allocation failed for undo.");
        }
        node->next = NULL;
        node->snapshot.board = allocate_board(g->rows, g->cols);
        if (fscanf(f, "%d %d %d", &node->snapshot.gameOver, &node->snapshot.win, &node->snapshot.revealedCount) != 3) {
            fclose(f);
            free_board(node->snapshot.board, g->rows);
            free(node);
            free_game(g);
            fail("State file is invalid.");
        }
        for (int r = 0; r < g->rows; r++) {
            for (int c = 0; c < g->cols; c++) {
                Cell *cell = &node->snapshot.board[r][c];
                if (fscanf(f, "%d %d %d %d", &cell->isMine, &cell->isRevealed, &cell->isFlagged, &cell->neighborMines) != 4) {
                    fclose(f);
                    free_board(node->snapshot.board, g->rows);
                    free(node);
                    free_game(g);
                    fail("State file is invalid.");
                }
            }
        }
        g->undoTop = node;
    }

    fclose(f);
}

static void print_json(const GameState *g) {
    printf("{\"rows\":%d,\"cols\":%d,\"game_over\":%s,\"win\":%s,\"can_undo\":%s,\"history\":[",
           g->rows,
           g->cols,
           g->gameOver ? "true" : "false",
           g->win ? "true" : "false",
           g->undoTop ? "true" : "false");

    ActionNode *cur = g->historyHead;
    int first = 1;
    while (cur) {
        if (!first) {
            printf(",");
        }
        printf("\"%s(%d,%d)\"", cur->action, cur->row, cur->col);
        first = 0;
        cur = cur->next;
    }

    printf("],\"board\":[");
    for (int r = 0; r < g->rows; r++) {
        printf("[");
        for (int c = 0; c < g->cols; c++) {
            Cell cell = g->board[r][c];
            if (cell.isFlagged && !cell.isRevealed) {
                printf("\"F\"");
            } else if (!cell.isRevealed) {
                printf("\"#\"");
            } else if (cell.isMine) {
                printf("\"*\"");
            } else {
                printf("\"%d\"", cell.neighborMines);
            }
            if (c < g->cols - 1) {
                printf(",");
            }
        }
        printf("]");
        if (r < g->rows - 1) {
            printf(",");
        }
    }
    printf("]}");
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: backend <init|open|flag|undo> <state_file> [...]\n");
        return 1;
    }

    const char *cmd = argv[1];
    const char *stateFile = argv[2];
    GameState state;

    if (strcmp(cmd, "init") == 0) {
        if (argc < 7) {
            fprintf(stderr, "init needs: state_file rows cols mines seed\n");
            return 1;
        }
        init_game(&state, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
        save_state(stateFile, &state);
        print_json(&state);
        free_game(&state);
        return 0;
    }

    load_state(stateFile, &state);

    if (strcmp(cmd, "open") == 0) {
        if (argc < 5) {
            fprintf(stderr, "open needs: state_file row col\n");
            free_game(&state);
            return 1;
        }
        open_cell(&state, atoi(argv[3]), atoi(argv[4]));
    } else if (strcmp(cmd, "flag") == 0) {
        if (argc < 5) {
            fprintf(stderr, "flag needs: state_file row col\n");
            free_game(&state);
            return 1;
        }
        toggle_flag(&state, atoi(argv[3]), atoi(argv[4]));
    } else if (strcmp(cmd, "undo") == 0) {
        pop_undo_snapshot(&state);
    } else {
        fprintf(stderr, "Unknown command.\n");
        free_game(&state);
        return 1;
    }

    save_state(stateFile, &state);
    print_json(&state);
    free_game(&state);
    return 0;
}

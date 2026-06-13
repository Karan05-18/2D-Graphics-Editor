#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_SHAPES 50
#define MAX_HIST 10
#define W 50
#define H 20

typedef enum { LINE, RECT, CIRC, TRI } ShapeType;

typedef struct {
    ShapeType type;
    int id, x1, y1, x2, y2, x3, y3, r, filled;
    char chr;
} Shape;

typedef struct {
    Shape shapes[MAX_SHAPES];
    int count;
} State;

State history[MAX_HIST];
int hist_idx = -1, hist_cnt = 0;

void push_state(State* s) {
    if (hist_idx < MAX_HIST - 1) {
        history[++hist_idx] = *s;
        hist_cnt = hist_idx + 1;
    } else {
        for (int i = 1; i < MAX_HIST; i++) history[i - 1] = history[i];
        history[MAX_HIST - 1] = *s;
        hist_idx = MAX_HIST - 1;
    }
}

void draw_pixel(char canvas[H][W], int x, int y, char c) {
    if (x >= 0 && x < W && y >= 0 && y < H) canvas[y][x] = c;
}

void draw_line(char canvas[H][W], int x1, int y1, int x2, int y2, char c) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        draw_pixel(canvas, x1, y1, c);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

void draw_circle(char canvas[H][W], int cx, int cy, int r, char c) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (y >= x) {
        int px[] = {cx + x, cx - x, cx + x, cx - x, cx + y, cx - y, cx + y, cx - y};
        int py[] = {cy + y, cy + y, cy - y, cy - y, cy + x, cy + x, cy - x, cy - x};
        for (int i = 0; i < 8; i++) draw_pixel(canvas, px[i], py[i], c);
        x++;
        if (d > 0) { y--; d += 4 * (x - y) + 10; }
        else d += 4 * x + 6;
    }
}

int in_tri(int px, int py, int x1, int y1, int x2, int y2, int x3, int y3) {
    long d1 = (long)(px - x1) * (y2 - y1) - (long)(py - y1) * (x2 - x1);
    long d2 = (long)(px - x2) * (y3 - y2) - (long)(py - y2) * (x3 - x2);
    long d3 = (long)(px - x3) * (y1 - y3) - (long)(py - y3) * (x1 - x3);
    return !(((d1 < 0) || (d2 < 0) || (d3 < 0)) && ((d1 > 0) || (d2 > 0) || (d3 > 0)));
}

void render(State* s, const char* msg, int err) {
    char canvas[H][W];
    memset(canvas, '.', sizeof(canvas));

    for (int i = 0; i < s->count; i++) {
        Shape* sh = &s->shapes[i];
        if (sh->type == LINE) {
            draw_line(canvas, sh->x1, sh->y1, sh->x2, sh->y2, sh->chr);
        } else if (sh->type == RECT) {
            for (int y = 0; y < sh->y2; y++) {
                for (int x = 0; x < sh->x2; x++) {
                    int px = sh->x1 + x, py = sh->y1 + y;
                    if (sh->filled || y == 0 || y == sh->y2 - 1 || x == 0 || x == sh->x2 - 1) {
                        draw_pixel(canvas, px, py, sh->chr);
                    }
                }
            }
        } else if (sh->type == CIRC) {
            draw_circle(canvas, sh->x1, sh->y1, sh->r, sh->chr);
        } else if (sh->type == TRI) {
            if (sh->filled) {
                int minx = sh->x1 < sh->x2 ? (sh->x1 < sh->x3 ? sh->x1 : sh->x3) : (sh->x2 < sh->x3 ? sh->x2 : sh->x3);
                int maxx = sh->x1 > sh->x2 ? (sh->x1 > sh->x3 ? sh->x1 : sh->x3) : (sh->x2 > sh->x3 ? sh->x2 : sh->x3);
                int miny = sh->y1 < sh->y2 ? (sh->y1 < sh->y3 ? sh->y1 : sh->y3) : (sh->y2 < sh->y3 ? sh->y2 : sh->y3);
                int maxy = sh->y1 > sh->y2 ? (sh->y1 > sh->y3 ? sh->y1 : sh->y3) : (sh->y2 > sh->y3 ? sh->y2 : sh->y3);
                for (int y = miny; y <= maxy; y++) {
                    for (int x = minx; x <= maxx; x++) {
                        if (in_tri(x, y, sh->x1, sh->y1, sh->x2, sh->y2, sh->x3, sh->y3)) {
                            draw_pixel(canvas, x, y, sh->chr);
                        }
                    }
                }
            } else {
                draw_line(canvas, sh->x1, sh->y1, sh->x2, sh->y2, sh->chr);
                draw_line(canvas, sh->x2, sh->y2, sh->x3, sh->y3, sh->chr);
                draw_line(canvas, sh->x3, sh->y3, sh->x1, sh->y1, sh->chr);
            }
        }
    }

    printf("\033[H\033[2J");
    printf("\033[1;36m┌──────────────────────────────────────────────────┬────────────────────────┐\033[0m\n");
    printf("\033[1;36m│\033[1;33m             ★ 2D GRAPHICS EDITOR ★             \033[1;36m│\033[1;35m SHAPES LIST:            \033[1;36m│\033[0m\n");
    printf("\033[1;36m├──────────────────────────────────────────────────┼────────────────────────┤\033[0m\n");

    int list_idx = 0;
    for (int y = 0; y < H; y++) {
        printf("\033[1;36m│\033[0m");
        for (int x = 0; x < W; x++) {
            char c = canvas[y][x];
            if (c == '.') printf("\033[90m.\033[0m");
            else printf("\033[97m%c\033[0m", c);
        }
        printf("\033[1;36m│\033[0m");

        char list_buf[40] = "";
        while (list_idx < s->count) {
            Shape* sh = &s->shapes[list_idx++];
            char* t = (sh->type == LINE) ? "Line" : (sh->type == RECT) ? "Rect" : (sh->type == CIRC) ? "Circ" : "Tri ";
            sprintf(list_buf, " %d: %s '%c' (%d,%d)", sh->id, t, sh->chr, sh->x1, sh->y1);
            break;
        }
        printf("%-24s\033[1;36m│\033[0m\n", list_buf);
    }
    printf("\033[1;36m└──────────────────────────────────────────────────┴────────────────────────┘\033[0m\n");
    if (msg && msg[0]) {
        printf(err ? "\033[1;31mError: %s\033[0m\n" : "\033[1;32m%s\033[0m\n", msg);
    } else {
        printf("\033[90mReady. Try: add line, add rect, add circle, add tri, move, undo, redo, clear\033[0m\n");
    }
}

int main() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &m)) SetConsoleMode(h, m | 4);
    SetConsoleOutputCP(65001);
#endif

    State s = {0};
    int next_id = 1;
    push_state(&s);

    char buf[256], msg[128] = "";
    int err = 0;

    while (1) {
        render(&s, msg, err);
        printf("\neditor> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;

        int len = strlen(buf);
        while (len > 0 && isspace((unsigned char)buf[len - 1])) buf[--len] = '\0';
        if (len == 0) continue;

        err = 0; msg[0] = '\0';
        char cmd[32] = "", type[32] = "";
        int parsed = sscanf(buf, "%s %s", cmd, type);
        if (parsed <= 0) continue;

        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            break;
        } else if (strcmp(cmd, "undo") == 0) {
            if (hist_idx > 0) { s = history[--hist_idx]; strcpy(msg, "Undo done"); }
            else { err = 1; strcpy(msg, "Nothing to undo"); }
        } else if (strcmp(cmd, "redo") == 0) {
            if (hist_idx < hist_cnt - 1) { s = history[++hist_idx]; strcpy(msg, "Redo done"); }
            else { err = 1; strcpy(msg, "Nothing to redo"); }
        } else if (strcmp(cmd, "clear") == 0) {
            s.count = 0; push_state(&s); strcpy(msg, "Canvas cleared");
        } else if (strcmp(cmd, "add") == 0 && s.count < MAX_SHAPES) {
            Shape* sh = &s.shapes[s.count];
            char chr;
            int n = 0;
            if (strcmp(type, "line") == 0) {
                if (sscanf(buf, "add line %d %d %d %d %c", &sh->x1, &sh->y1, &sh->x2, &sh->y2, &chr) == 5) {
                    sh->type = LINE; sh->chr = chr; n = 1;
                }
            } else if (strcmp(type, "rect") == 0) {
                sh->filled = 0;
                int count = sscanf(buf, "add rect %d %d %d %d %c %d", &sh->x1, &sh->y1, &sh->x2, &sh->y2, &chr, &sh->filled);
                if (count >= 5) {
                    sh->type = RECT; sh->chr = chr; n = 1;
                }
            } else if (strcmp(type, "circle") == 0) {
                if (sscanf(buf, "add circle %d %d %d %c", &sh->x1, &sh->y1, &sh->r, &chr) == 4) {
                    sh->type = CIRC; sh->chr = chr; n = 1;
                }
            } else if (strcmp(type, "tri") == 0) {
                sh->filled = 0;
                int count = sscanf(buf, "add tri %d %d %d %d %d %d %c %d", &sh->x1, &sh->y1, &sh->x2, &sh->y2, &sh->x3, &sh->y3, &chr, &sh->filled);
                if (count >= 7) {
                    sh->type = TRI; sh->chr = chr; n = 1;
                }
            }
            if (n) {
                sh->id = next_id++;
                s.count++;
                push_state(&s);
                sprintf(msg, "Added shape %d", sh->id);
            } else {
                err = 1; strcpy(msg, "Invalid shape syntax");
            }
        } else if (strcmp(cmd, "move") == 0) {
            int id, dx, dy, found = 0;
            if (sscanf(buf, "move %d %d %d", &id, &dx, &dy) == 3) {
                for (int i = 0; i < s.count; i++) {
                    if (s.shapes[i].id == id) {
                        s.shapes[i].x1 += dx; s.shapes[i].y1 += dy;
                        s.shapes[i].x2 += dx; s.shapes[i].y2 += dy;
                        s.shapes[i].x3 += dx; s.shapes[i].y3 += dy;
                        found = 1; break;
                    }
                }
            }
            if (found) { push_state(&s); sprintf(msg, "Moved shape %d", id); }
            else { err = 1; strcpy(msg, "Shape not found / invalid params"); }
        } else if (strcmp(cmd, "remove") == 0) {
            int id, found = -1;
            if (sscanf(buf, "remove %d", &id) == 1) {
                for (int i = 0; i < s.count; i++) {
                    if (s.shapes[i].id == id) { found = i; break; }
                }
            }
            if (found >= 0) {
                for (int i = found; i < s.count - 1; i++) s.shapes[i] = s.shapes[i + 1];
                s.count--; push_state(&s); sprintf(msg, "Removed shape %d", id);
            } else { err = 1; strcpy(msg, "Shape not found"); }
        } else {
            err = 1; strcpy(msg, "Unknown command");
        }
    }
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_SHAPES 100
#define MAX_HIST 20
#define MAX_W 100
#define MAX_H 40
#define DEFAULT_W 50
#define DEFAULT_H 20

typedef enum { LINE, RECT, CIRC, TRI, TEXT } ShapeType;

typedef struct {
    ShapeType type;
    int id, x1, y1, x2, y2, x3, y3, r, filled;
    char chr;
    char text[32];
} Shape;

typedef struct {
    Shape shapes[MAX_SHAPES];
    int count;
    int canvas_width;
    int canvas_height;
    char bg_char;
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

void draw_pixel(char canvas[MAX_H][MAX_W], int x, int y, char c) {
    if (x >= 0 && x < MAX_W && y >= 0 && y < MAX_H) canvas[y][x] = c;
}

void draw_line(char canvas[MAX_H][MAX_W], int x1, int y1, int x2, int y2, char c) {
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

void draw_circle(char canvas[MAX_H][MAX_W], int cx, int cy, int r, char c, int cw, int ch) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (y >= x) {
        int px[] = {cx + x, cx - x, cx + x, cx - x, cx + y, cx - y, cx + y, cx - y};
        int py[] = {cy + y, cy + y, cy - y, cy - y, cy + x, cy + x, cy - x, cy - x};
        for (int i = 0; i < 8; i++) {
            if (px[i] >= 0 && px[i] < cw && py[i] >= 0 && py[i] < ch) {
                canvas[py[i]][px[i]] = c;
            }
        }
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

int save_file(State* s, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return 0;
    fprintf(f, "%d %d %c %d\n", s->canvas_width, s->canvas_height, s->bg_char, s->count);
    for (int i = 0; i < s->count; i++) {
        Shape* sh = &s->shapes[i];
        fprintf(f, "%d %d %d %d %d %d %d %d %d %d %c ", sh->id, sh->type, sh->x1, sh->y1, sh->x2, sh->y2, sh->x3, sh->y3, sh->r, sh->filled, sh->chr);
        if (sh->type == TEXT) fprintf(f, "%s\n", sh->text);
        else fprintf(f, "\n");
    }
    fclose(f);
    return 1;
}

int load_file(State* s, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return 0;
    State temp = {0};
    int count = 0;
    if (fscanf(f, "%d %d %c %d\n", &temp.canvas_width, &temp.canvas_height, &temp.bg_char, &count) != 4) {
        fclose(f);
        return 0;
    }
    int max_id = 0;
    for (int i = 0; i < count; i++) {
        Shape* sh = &temp.shapes[temp.count];
        int t;
        if (fscanf(f, "%d %d %d %d %d %d %d %d %d %d %c ", &sh->id, &t, &sh->x1, &sh->y1, &sh->x2, &sh->y2, &sh->x3, &sh->y3, &sh->r, &sh->filled, &sh->chr) != 11) {
            fclose(f);
            return 0;
        }
        sh->type = (ShapeType)t;
        if (sh->id > max_id) max_id = sh->id;
        if (sh->type == TEXT) {
            char buf[64];
            if (fgets(buf, sizeof(buf), f)) {
                int len = strlen(buf);
                while (len > 0 && isspace((unsigned char)buf[len - 1])) buf[--len] = '\0';
                strncpy(sh->text, buf, 31);
                sh->text[31] = '\0';
            }
        } else {
            int c = fgetc(f);
            while (c != '\n' && c != EOF) c = fgetc(f);
        }
        temp.count++;
        if (temp.count >= MAX_SHAPES) break;
    }
    fclose(f);
    *s = temp;
    return max_id + 1;
}

int parse_add_text(const char* input, int* x, int* y, char* text_out) {
    const char* p = strstr(input, "text");
    if (!p) return 0;
    p += 4;
    while (*p && isspace((unsigned char)*p)) p++;
    char* end;
    *x = (int)strtol(p, &end, 10);
    if (p == end) return 0;
    p = end;
    while (*p && isspace((unsigned char)*p)) p++;
    *y = (int)strtol(p, &end, 10);
    if (p == end) return 0;
    p = end;
    while (*p && isspace((unsigned char)*p)) p++;
    strncpy(text_out, p, 31);
    text_out[31] = '\0';
    return 1;
}

void render(State* s, const char* msg, int err, int show_help) {
    if (show_help) {
        printf("\033[H\033[2J");
        printf("\033[1;36m┌──────────────────────────────────────────────────────────────────────────────┐\033[0m\n");
        printf("\033[1;36m│\033[1;33m                         ★ COMMAND MANUAL GUIDE ★                           \033[1;36m│\033[0m\n");
        printf("\033[1;36m├──────────────────────────────────────────────────────────────────────────────┤\033[0m\n");
        printf("  add line <x1> <y1> <x2> <y2> <char>       - Draw line\n");
        printf("  add rect <x> <y> <w> <h> <char> [fill]    - Draw rectangle\n");
        printf("  add circle <cx> <cy> <r> <char>           - Draw circle\n");
        printf("  add tri <x1> <y1> ... <char> [fill]       - Draw triangle\n");
        printf("  add text <x> <y> <text_string>            - Plot text\n");
        printf("  move <id> <dx> <dy>                       - Translate shape coords\n");
        printf("  color <id> <char>                         - Change shape drawing symbol\n");
        printf("  remove <id>                               - Delete shape\n");
        printf("  undo / redo / clear                       - Edit history actions\n");
        printf("  canvas <w> <h> <bg_char>                  - Resize canvas grid\n");
        printf("  save <file> / load <file>                 - Serialization utilities\n");
        printf("  exit                                      - Quit program\n");
        printf("\033[1;36m└──────────────────────────────────────────────────────────────────────────────┘\033[0m\n");
        printf("Press [ENTER] to return to the editor.\n");
        return;
    }

    char canvas[MAX_H][MAX_W];
    for (int y = 0; y < s->canvas_height; y++) {
        for (int x = 0; x < s->canvas_width; x++) {
            canvas[y][x] = s->bg_char;
        }
    }

    for (int i = 0; i < s->count; i++) {
        Shape* sh = &s->shapes[i];
        if (sh->type == LINE) {
            draw_line(canvas, sh->x1, sh->y1, sh->x2, sh->y2, sh->chr);
        } else if (sh->type == RECT) {
            for (int y = 0; y < sh->y2; y++) {
                for (int x = 0; x < sh->x2; x++) {
                    int px = sh->x1 + x, py = sh->y1 + y;
                    if (sh->filled || y == 0 || y == sh->y2 - 1 || x == 0 || x == sh->x2 - 1) {
                        if (px >= 0 && px < s->canvas_width && py >= 0 && py < s->canvas_height) {
                            canvas[py][px] = sh->chr;
                        }
                    }
                }
            }
        } else if (sh->type == CIRC) {
            draw_circle(canvas, sh->x1, sh->y1, sh->r, sh->chr, s->canvas_width, s->canvas_height);
        } else if (sh->type == TRI) {
            if (sh->filled) {
                int minx = sh->x1 < sh->x2 ? (sh->x1 < sh->x3 ? sh->x1 : sh->x3) : (sh->x2 < sh->x3 ? sh->x2 : sh->x3);
                int maxx = sh->x1 > sh->x2 ? (sh->x1 > sh->x3 ? sh->x1 : sh->x3) : (sh->x2 > sh->x3 ? sh->x2 : sh->x3);
                int miny = sh->y1 < sh->y2 ? (sh->y1 < sh->y3 ? sh->y1 : sh->y3) : (sh->y2 < sh->y3 ? sh->y2 : sh->y3);
                int maxy = sh->y1 > sh->y2 ? (sh->y1 > sh->y3 ? sh->y1 : sh->y3) : (sh->y2 > sh->y3 ? sh->y2 : sh->y3);
                for (int y = miny; y <= maxy; y++) {
                    for (int x = minx; x <= maxx; x++) {
                        if (in_tri(x, y, sh->x1, sh->y1, sh->x2, sh->y2, sh->x3, sh->y3)) {
                            if (x >= 0 && x < s->canvas_width && y >= 0 && y < s->canvas_height) {
                                canvas[y][x] = sh->chr;
                            }
                        }
                    }
                }
            } else {
                draw_line(canvas, sh->x1, sh->y1, sh->x2, sh->y2, sh->chr);
                draw_line(canvas, sh->x2, sh->y2, sh->x3, sh->y3, sh->chr);
                draw_line(canvas, sh->x3, sh->y3, sh->x1, sh->y1, sh->chr);
            }
        } else if (sh->type == TEXT) {
            int len = strlen(sh->text);
            for (int j = 0; j < len; j++) {
                int px = sh->x1 + j, py = sh->y1;
                if (px >= 0 && px < s->canvas_width && py >= 0 && py < s->canvas_height) {
                    canvas[py][px] = sh->text[j];
                }
            }
        }
    }

    printf("\033[H\033[2J");
    printf("\033[1;36m┌");
    for (int i = 0; i < s->canvas_width; i++) printf("─");
    printf("┬────────────────────────┐\033[0m\n");
    printf("\033[1;36m│\033[1;33m%-*s\033[1;36m│\033[1;35m SHAPES LIST:            \033[1;36m│\033[0m\n", s->canvas_width, "   ★ 2D VECTOR EDITOR ★");
    printf("\033[1;36m├");
    for (int i = 0; i < s->canvas_width; i++) printf("─");
    printf("┼────────────────────────┤\033[0m\n");

    int list_idx = 0;
    for (int y = 0; y < s->canvas_height; y++) {
        printf("\033[1;36m│\033[0m");
        for (int x = 0; x < s->canvas_width; x++) {
            char c = canvas[y][x];
            if (c == s->bg_char) printf("\033[90m%c\033[0m", c);
            else printf("\033[97m%c\033[0m", c);
        }
        printf("\033[1;36m│\033[0m");

        char list_buf[32] = "";
        while (list_idx < s->count) {
            Shape* sh = &s->shapes[list_idx++];
            char* t = (sh->type == LINE) ? "Line" : (sh->type == RECT) ? "Rect" : (sh->type == CIRC) ? "Circ" : (sh->type == TRI) ? "Tri " : "Text";
            sprintf(list_buf, " %d: %s '%c' (%d,%d)", sh->id, t, sh->type == TEXT ? '"' : sh->chr, sh->x1, sh->y1);
            break;
        }
        printf("%-24s\033[1;36m│\033[0m\n", list_buf);
    }
    printf("\033[1;36m└");
    for (int i = 0; i < s->canvas_width; i++) printf("─");
    printf("┴────────────────────────┘\033[0m\n");

    if (msg && msg[0]) {
        printf(err ? "\033[1;31mError: %s\033[0m\n" : "\033[1;32m%s\033[0m\n", msg);
    } else {
        printf("\033[90mReady. Try: add line, add rect, add circle, add tri, add text, undo, redo, help\033[0m\n");
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
    s.canvas_width = DEFAULT_W;
    s.canvas_height = DEFAULT_H;
    s.bg_char = '.';
    int next_id = 1;
    push_state(&s);

    char buf[256], msg[128] = "";
    int err = 0, show_help = 0;

    while (1) {
        render(&s, msg, err, show_help);
        printf("\neditor> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;

        int len = strlen(buf);
        while (len > 0 && isspace((unsigned char)buf[len - 1])) buf[--len] = '\0';
        
        if (len == 0) {
            if (show_help) { show_help = 0; strcpy(msg, "Returned to canvas"); err = 0; }
            continue;
        }

        if (show_help) show_help = 0;

        err = 0; msg[0] = '\0';
        char cmd[32] = "", type[32] = "";
        int parsed = sscanf(buf, "%s %s", cmd, type);
        if (parsed <= 0) continue;

        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            break;
        } else if (strcmp(cmd, "help") == 0) {
            show_help = 1;
            strcpy(msg, "Showing help guide");
        } else if (strcmp(cmd, "undo") == 0) {
            if (hist_idx > 0) { s = history[--hist_idx]; strcpy(msg, "Undo done"); }
            else { err = 1; strcpy(msg, "Nothing to undo"); }
        } else if (strcmp(cmd, "redo") == 0) {
            if (hist_idx < hist_cnt - 1) { s = history[++hist_idx]; strcpy(msg, "Redo done"); }
            else { err = 1; strcpy(msg, "Nothing to redo"); }
        } else if (strcmp(cmd, "clear") == 0) {
            s.count = 0; push_state(&s); strcpy(msg, "Canvas cleared");
        } else if (strcmp(cmd, "canvas") == 0) {
            int w_val, h_val;
            char bg_val = s.bg_char;
            int c_cnt = sscanf(buf, "canvas %d %d %c", &w_val, &h_val, &bg_val);
            if (c_cnt >= 2) {
                if (w_val > 0 && w_val <= MAX_W && h_val > 0 && h_val <= MAX_H) {
                    s.canvas_width = w_val; s.canvas_height = h_val; s.bg_char = bg_val;
                    push_state(&s);
                    sprintf(msg, "Canvas resized to %dx%d background '%c'", w_val, h_val, bg_val);
                } else {
                    err = 1; sprintf(msg, "Dimensions must be 1x1 to %dx%d", MAX_W, MAX_H);
                }
            } else { err = 1; strcpy(msg, "Usage: canvas <w> <h> [bg_char]"); }
        } else if (strcmp(cmd, "save") == 0) {
            char file[64] = "";
            if (sscanf(buf, "save %63s", file) == 1) {
                if (save_file(&s, file)) sprintf(msg, "Saved layout to '%s'", file);
                else { err = 1; sprintf(msg, "Failed to save file '%s'", file); }
            } else { err = 1; strcpy(msg, "Usage: save <filename>"); }
        } else if (strcmp(cmd, "load") == 0) {
            char file[64] = "";
            if (sscanf(buf, "load %63s", file) == 1) {
                int loaded_id = load_file(&s, file);
                if (loaded_id > 0) {
                    next_id = loaded_id; push_state(&s);
                    sprintf(msg, "Loaded layout from '%s'", file);
                } else { err = 1; sprintf(msg, "Failed to load file '%s'", file); }
            } else { err = 1; strcpy(msg, "Usage: load <filename>"); }
        } else if (strcmp(cmd, "color") == 0) {
            int id, found = 0;
            char new_c;
            if (sscanf(buf, "color %d %c", &id, &new_c) == 2) {
                for (int i = 0; i < s.count; i++) {
                    if (s.shapes[i].id == id) {
                        if (s.shapes[i].type == TEXT) { err = 1; strcpy(msg, "Cannot color text shapes"); }
                        else { s.shapes[i].chr = new_c; found = 1; }
                        break;
                    }
                }
            }
            if (found) { push_state(&s); sprintf(msg, "Color of shape %d changed to '%c'", id, new_c); }
            else if (!err) { err = 1; strcpy(msg, "Shape not found / invalid params"); }
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
                if (count >= 5) { sh->type = RECT; sh->chr = chr; n = 1; }
            } else if (strcmp(type, "circle") == 0) {
                if (sscanf(buf, "add circle %d %d %d %c", &sh->x1, &sh->y1, &sh->r, &chr) == 4) {
                    sh->type = CIRC; sh->chr = chr; n = 1;
                }
            } else if (strcmp(type, "tri") == 0) {
                sh->filled = 0;
                int count = sscanf(buf, "add tri %d %d %d %d %d %d %c %d", &sh->x1, &sh->y1, &sh->x2, &sh->y2, &sh->x3, &sh->y3, &chr, &sh->filled);
                if (count >= 7) { sh->type = TRI; sh->chr = chr; n = 1; }
            } else if (strcmp(type, "text") == 0) {
                if (parse_add_text(buf, &sh->x1, &sh->y1, sh->text)) {
                    sh->type = TEXT; sh->chr = '"'; n = 1;
                }
            }
            if (n) {
                sh->id = next_id++; s.count++; push_state(&s);
                sprintf(msg, "Added shape %d", sh->id);
            } else if (!err) { err = 1; strcpy(msg, "Invalid shape syntax"); }
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
        } else { err = 1; strcpy(msg, "Unknown command"); }
    }
    return 0;
}
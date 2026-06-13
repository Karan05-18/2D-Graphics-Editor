#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#endif

#define MAX_SHAPES 100
#define MAX_HISTORY 20
#define MAX_WIDTH 100
#define MAX_HEIGHT 100
#define DEFAULT_WIDTH 50
#define DEFAULT_HEIGHT 20

typedef enum {
    SHAPE_LINE,
    SHAPE_RECTANGLE,
    SHAPE_CIRCLE,
    SHAPE_TRIANGLE,
    SHAPE_TEXT
} ShapeType;

typedef struct {
    int x1, y1, x2, y2, x3, y3; // Coordinates depending on shape type
    int r;                      // Circle radius
    int w, h;                   // Rect width/height
    char text[64];              // Text string
    char draw_char;             // Drawing character symbol
    int is_filled;              // Filled vs outline
} ShapeParams;

typedef struct {
    int id;
    ShapeType type;
    ShapeParams params;
    int is_active;
} Shape;

typedef struct {
    Shape shapes[MAX_SHAPES];
    int shape_count;
    int canvas_width;
    int canvas_height;
    char bg_char;
} EditorState;

// Global history stack
EditorState history[MAX_HISTORY];
int history_index = -1;
int history_count = 0;

// Push state onto history stack
void push_history(const EditorState* state) {
    if (history_index < MAX_HISTORY - 1) {
        history_index++;
        history[history_index] = *state;
        history_count = history_index + 1;
    } else {
        // Shift history left to discard oldest state
        for (int i = 1; i < MAX_HISTORY; i++) {
            history[i - 1] = history[i];
        }
        history[MAX_HISTORY - 1] = *state;
        history_index = MAX_HISTORY - 1;
        history_count = MAX_HISTORY;
    }
}

// Trim whitespace from string
char* trim(char* str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Drawing Algorithms
void draw_line(char canvas[MAX_HEIGHT][MAX_WIDTH], int width, int height, int x1, int y1, int x2, int y2, char c) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x1 >= 0 && x1 < width && y1 >= 0 && y1 < height) {
            canvas[y1][x1] = c;
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void draw_rectangle(char canvas[MAX_HEIGHT][MAX_WIDTH], int width, int height, int x, int y, int w, int h, char c, int filled) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int px = x + j;
            int py = y + i;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                if (filled || i == 0 || i == h - 1 || j == 0 || j == w - 1) {
                    canvas[py][px] = c;
                }
            }
        }
    }
}

void draw_circle_points(char canvas[MAX_HEIGHT][MAX_WIDTH], int width, int height, int cx, int cy, int x, int y, char c) {
    int px[8] = {cx + x, cx - x, cx + x, cx - x, cx + y, cx - y, cx + y, cx - y};
    int py[8] = {cy + y, cy + y, cy - y, cy - y, cy + x, cy + x, cy - x, cy - x};
    for (int i = 0; i < 8; i++) {
        if (px[i] >= 0 && px[i] < width && py[i] >= 0 && py[i] < height) {
            canvas[py[i]][px[i]] = c;
        }
    }
}

void draw_circle(char canvas[MAX_HEIGHT][MAX_WIDTH], int width, int height, int cx, int cy, int r, char c) {
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;
    draw_circle_points(canvas, width, height, cx, cy, x, y, c);
    while (y >= x) {
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
        draw_circle_points(canvas, width, height, cx, cy, x, y, c);
    }
}

int is_point_in_triangle(int px, int py, int x1, int y1, int x2, int y2, int x3, int y3) {
    long d1 = (long)(px - x1) * (y2 - y1) - (long)(py - y1) * (x2 - x1);
    long d2 = (long)(px - x2) * (y3 - y2) - (long)(py - y2) * (x3 - x2);
    long d3 = (long)(px - x3) * (y1 - y3) - (long)(py - y3) * (x1 - x3);

    int has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    int has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

void draw_triangle(char canvas[MAX_HEIGHT][MAX_WIDTH], int width, int height, int x1, int y1, int x2, int y2, int x3, int y3, char c, int filled) {
    if (filled) {
        int min_x = x1;
        if (x2 < min_x) min_x = x2;
        if (x3 < min_x) min_x = x3;

        int max_x = x1;
        if (x2 > max_x) max_x = x2;
        if (x3 > max_x) max_x = x3;

        int min_y = y1;
        if (y2 < min_y) min_y = y2;
        if (y3 < min_y) min_y = y3;

        int max_y = y1;
        if (y2 > max_y) max_y = y2;
        if (y3 > max_y) max_y = y3;

        if (min_x < 0) min_x = 0;
        if (max_x >= width) max_x = width - 1;
        if (min_y < 0) min_y = 0;
        if (max_y >= height) max_y = height - 1;

        for (int y = min_y; y <= max_y; y++) {
            for (int x = min_x; x <= max_x; x++) {
                if (is_point_in_triangle(x, y, x1, y1, x2, y2, x3, y3)) {
                    canvas[y][x] = c;
                }
            }
        }
    } else {
        draw_line(canvas, width, height, x1, y1, x2, y2, c);
        draw_line(canvas, width, height, x2, y2, x3, y3, c);
        draw_line(canvas, width, height, x3, y3, x1, y1, c);
    }
}

void draw_text(char canvas[MAX_HEIGHT][MAX_WIDTH], int width, int height, int x, int y, const char* text) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        int px = x + i;
        if (px >= 0 && px < width && y >= 0 && y < height) {
            canvas[y][px] = text[i];
        }
    }
}

// Rendering Editor state to screen (TUI)
void render_editor(const EditorState* state, const char* status_msg, int status_is_error) {
    printf("\033[H\033[2J"); // Clear screen & home cursor

    // Pre-render canvas buffer
    char canvas[MAX_HEIGHT][MAX_WIDTH];
    for (int y = 0; y < state->canvas_height; y++) {
        for (int x = 0; x < state->canvas_width; x++) {
            canvas[y][x] = state->bg_char;
        }
    }

    for (int i = 0; i < state->shape_count; i++) {
        if (state->shapes[i].is_active) {
            const Shape* s = &state->shapes[i];
            switch (s->type) {
                case SHAPE_LINE:
                    draw_line(canvas, state->canvas_width, state->canvas_height,
                              s->params.x1, s->params.y1, s->params.x2, s->params.y2, s->params.draw_char);
                    break;
                case SHAPE_RECTANGLE:
                    draw_rectangle(canvas, state->canvas_width, state->canvas_height,
                                   s->params.x1, s->params.y1, s->params.w, s->params.h, s->params.draw_char, s->params.is_filled);
                    break;
                case SHAPE_CIRCLE:
                    draw_circle(canvas, state->canvas_width, state->canvas_height,
                                s->params.x1, s->params.y1, s->params.r, s->params.draw_char);
                    break;
                case SHAPE_TRIANGLE:
                    draw_triangle(canvas, state->canvas_width, state->canvas_height,
                                  s->params.x1, s->params.y1, s->params.x2, s->params.y2, s->params.x3, s->params.y3,
                                  s->params.draw_char, s->params.is_filled);
                    break;
                case SHAPE_TEXT:
                    draw_text(canvas, state->canvas_width, state->canvas_height,
                              s->params.x1, s->params.y1, s->params.text);
                    break;
            }
        }
    }

    // Title / Header
    printf("\033[1;36m┌──────────────────────────────────────────────────────────────────────────────┐\033[0m\n");
    printf("\033[1;36m│\033[0m \033[1;33m  ★ 2D VECTOR GRAPHICS EDITOR ★   \033[0m│ Canvas: \033[1;32m%dx%d\033[0m │ Shapes: \033[1;32m%d\033[0m \033[1;36m│\033[0m\n",
           state->canvas_width, state->canvas_height, state->shape_count);
    printf("\033[1;36m├────────────────────────────────────────┬─────────────────────────────────────┤\033[0m\n");

    // Side-by-side printing
    #define RIGHT_PANEL_MAX_LINES 100
    char right_panel[RIGHT_PANEL_MAX_LINES][128];
    int right_line_count = 0;

    sprintf(right_panel[right_line_count++], "\033[1;35m[Active Shapes List]\033[0m");
    int active_shape_printed = 0;
    for (int i = 0; i < state->shape_count; i++) {
        if (state->shapes[i].is_active) {
            const Shape* s = &state->shapes[i];
            char shape_info[128] = {0};
            switch (s->type) {
                case SHAPE_LINE:
                    sprintf(shape_info, " ID %2d: Line '%c' (%d,%d)->(%d,%d)", s->id, s->params.draw_char, s->params.x1, s->params.y1, s->params.x2, s->params.y2);
                    break;
                case SHAPE_RECTANGLE:
                    sprintf(shape_info, " ID %2d: Rect '%c' (%d,%d) %dx%d %s", s->id, s->params.draw_char, s->params.x1, s->params.y1, s->params.w, s->params.h, s->params.is_filled ? "filled" : "outline");
                    break;
                case SHAPE_CIRCLE:
                    sprintf(shape_info, " ID %2d: Circ '%c' Center(%d,%d) r=%d", s->id, s->params.draw_char, s->params.x1, s->params.y1, s->params.r);
                    break;
                case SHAPE_TRIANGLE:
                    sprintf(shape_info, " ID %2d: Tri  '%c' (%d,%d), (%d,%d), (%d,%d) %s", s->id, s->params.draw_char, s->params.x1, s->params.y1, s->params.x2, s->params.y2, s->params.x3, s->params.y3, s->params.is_filled ? "filled" : "outline");
                    break;
                case SHAPE_TEXT:
                    sprintf(shape_info, " ID %2d: Text       (%d,%d) \"%s\"", s->id, s->params.x1, s->params.y1, s->params.text);
                    break;
            }
            shape_info[35] = '\0'; // Truncate to avoid line wrapping
            strcpy(right_panel[right_line_count++], shape_info);
            active_shape_printed++;
        }
    }
    if (active_shape_printed == 0) {
        sprintf(right_panel[right_line_count++], "  (no shapes added)");
    }

    sprintf(right_panel[right_line_count++], "-------------------------------------");
    sprintf(right_panel[right_line_count++], "\033[1;35m[History / Canvas Settings]\033[0m");
    sprintf(right_panel[right_line_count++], "  Undo index: %d / %d", history_index, history_count - 1);
    sprintf(right_panel[right_line_count++], "  Background char: '%c'", state->bg_char);
    sprintf(right_panel[right_line_count++], "-------------------------------------");
    sprintf(right_panel[right_line_count++], "\033[1;35m[Commands Cheat Sheet]\033[0m");
    sprintf(right_panel[right_line_count++], "  add line <x1> <y1> <x2> <y2> <char>");
    sprintf(right_panel[right_line_count++], "  add rect <x> <y> <w> <h> <char> [fill]");
    sprintf(right_panel[right_line_count++], "  add circle <cx> <cy> <r> <char>");
    sprintf(right_panel[right_line_count++], "  add tri <x1> <y1> ... <char> [fill]");
    sprintf(right_panel[right_line_count++], "  add text <x> <y> <text>");
    sprintf(right_panel[right_line_count++], "  move <id> <dx> <dy> | remove <id>");
    sprintf(right_panel[right_line_count++], "  resize <id> <args...> | color <id> <ch>");
    sprintf(right_panel[right_line_count++], "  undo | redo | clear | save/load <file>");
    sprintf(right_panel[right_line_count++], "  canvas <w> <h> <bg> | export <file> | help");

    int max_rows = state->canvas_height;
    int total_lines = max_rows > right_line_count ? max_rows : right_line_count;

    for (int y = 0; y < total_lines; y++) {
        // Canvas Row
        if (y < state->canvas_height) {
            printf("\033[1;36m│\033[0m ");
            for (int x = 0; x < state->canvas_width; x++) {
                char c = canvas[y][x];
                if (c == state->bg_char) {
                    printf("\033[90m%c\033[0m", c);
                } else if (c >= '0' && c <= '9') {
                    printf("\033[93m%c\033[0m", c);
                } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                    printf("\033[92m%c\033[0m", c);
                } else {
                    printf("\033[97m%c\033[0m", c);
                }
            }
            // Fill padding if canvas width is less than column spacing (40)
            for (int p = state->canvas_width; p < 38; p++) {
                printf(" ");
            }
            printf(" ");
        } else {
            printf("\033[1;36m│\033[0m ");
            for (int p = 0; p < 39; p++) {
                printf(" ");
            }
        }

        // Split Divider
        printf("\033[1;36m│\033[0m ");

        // Right panel row
        if (y < right_line_count) {
            printf("%s", right_panel[y]);
        }
        printf("\n");
    }

    // Bottom Border
    printf("\033[1;36m└────────────────────────────────────────┴─────────────────────────────────────┘\033[0m\n");

    // Status message display
    if (status_msg && strlen(status_msg) > 0) {
        if (status_is_error) {
            printf("\033[1;31m[ERROR] %s\033[0m\n", status_msg);
        } else {
            printf("\033[1;32m[SUCCESS] %s\033[0m\n", status_msg);
        }
    } else {
        printf("\033[90m[READY] Use commands or type 'help' for details.\033[0m\n");
    }
}

// Interactive Detailed Help Guide
void print_help_screen() {
    printf("\033[H\033[2J");
    printf("\033[1;36m┌──────────────────────────────────────────────────────────────────────────────┐\033[0m\n");
    printf("\033[1;36m│\033[0m                        \033[1;33m★ DETAILED COMMAND GUIDE ★\033[0m                            \033[1;36m│\033[0m\n");
    printf("\033[1;36m├──────────────────────────────────────────────────────────────────────────────┤\033[0m\n");
    printf("\033[1;35m  SHAPE CREATION:\033[0m\n");
    printf("    \033[1;32madd line <x1> <y1> <x2> <y2> <char>\033[0m\n");
    printf("      Draws a line between (x1, y1) and (x2, y2). Example: add line 5 2 20 8 #\n");
    printf("    \033[1;32madd rect <x> <y> <w> <h> <char> [filled: 0 or 1]\033[0m\n");
    printf("      Draws a rectangle at (x,y). Last param is optional fill. Example: add rect 2 2 10 5 * 1\n");
    printf("    \033[1;32madd circle <cx> <cy> <r> <char>\033[0m\n");
    printf("      Draws a circle with center (cx, cy) and radius r. Example: add circle 15 10 5 @\n");
    printf("    \033[1;32madd tri <x1> <y1> <x2> <y2> <x3> <y3> <char> [filled: 0 or 1]\033[0m\n");
    printf("      Draws a triangle with 3 vertices. Example: add tri 2 18 10 5 18 18 + 0\n");
    printf("    \033[1;32madd text <x> <y> <message string>\033[0m\n");
    printf("      Renders a text message starting at (x, y). Example: add text 5 0 Welcome!\n\n");

    printf("\033[1;35m  EDITING & MANIPULATION:\033[0m\n");
    printf("    \033[1;32mlist\033[0m                  - Displays details of all current active shapes.\n");
    printf("    \033[1;32mremove <id>\033[0m           - Deletes the shape with specified numeric ID.\n");
    printf("    \033[1;32mmove <id> <dx> <dy>\033[0m       - Offsets coordinates of a shape. Example: move 2 3 -1\n");
    printf("    \033[1;32mcolor <id> <char>\033[0m     - Changes the symbol character of a shape. Example: color 1 $\n");
    printf("    \033[1;32mresize <id> <params...>\033[0m - Modifies shape size. Parameters: \n");
    printf("                          • Line: <x2> <y2> (new target endpoint)\n");
    printf("                          • Rect: <w> <h> (new width and height)\n");
    printf("                          • Circle: <r> (new radius)\n");
    printf("                          • Triangle: <x2> <y2> <x3> <y3> (new endpoints)\n\n");

    printf("\033[1;35m  SYSTEM UTILITIES:\033[0m\n");
    printf("    \033[1;32mundo / redo\033[0m           - Step backwards or forwards through your edit history.\n");
    printf("    \033[1;32mcanvas <w> <h> <bg>\033[0m   - Reconfigures canvas grid dimensions and bg symbol. (Max 100x100)\n");
    printf("    \033[1;32msave <filename>\033[0m       - Saves shape data to a text file. Example: save sketch.txt\n");
    printf("    \033[1;32mload <filename>\033[0m       - Restores shape data from a text file. Example: load sketch.txt\n");
    printf("    \033[1;32mexport <filename>\033[0m     - Exports the raw rendered ASCII picture to a text file.\n");
    printf("    \033[1;32mclear\033[0m                 - Resets workspace, deleting all active shapes.\n");
    printf("    \033[1;32mexit\033[0m                  - Quits the application.\n");
    printf("\033[1;36m└──────────────────────────────────────────────────────────────────────────────┘\033[0m\n");
    printf("Press \033[1;33m[ENTER]\033[0m to return to the canvas, or enter a command below:\n");
}

// Save Vector data to file
int save_editor(const EditorState* state, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return 0;

    fprintf(f, "%d %d %c\n", state->canvas_width, state->canvas_height, state->bg_char);
    
    int active_count = 0;
    for (int i = 0; i < state->shape_count; i++) {
        if (state->shapes[i].is_active) active_count++;
    }
    
    fprintf(f, "%d\n", active_count);
    
    for (int i = 0; i < state->shape_count; i++) {
        if (state->shapes[i].is_active) {
            const Shape* s = &state->shapes[i];
            fprintf(f, "%d %d %c %d ", s->id, s->type, s->params.draw_char, s->params.is_filled);
            switch (s->type) {
                case SHAPE_LINE:
                    fprintf(f, "%d %d %d %d\n", s->params.x1, s->params.y1, s->params.x2, s->params.y2);
                    break;
                case SHAPE_RECTANGLE:
                    fprintf(f, "%d %d %d %d\n", s->params.x1, s->params.y1, s->params.w, s->params.h);
                    break;
                case SHAPE_CIRCLE:
                    fprintf(f, "%d %d %d\n", s->params.x1, s->params.y1, s->params.r);
                    break;
                case SHAPE_TRIANGLE:
                    fprintf(f, "%d %d %d %d %d %d\n", s->params.x1, s->params.y1, s->params.x2, s->params.y2, s->params.x3, s->params.y3);
                    break;
                case SHAPE_TEXT:
                    fprintf(f, "%d %d %s\n", s->params.x1, s->params.y1, s->params.text);
                    break;
            }
        }
    }
    
    fclose(f);
    return 1;
}

// Load Vector data from file (robust line-by-line parsing using sscanf and %n)
int load_editor(EditorState* state, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return 0;

    char line[256];
    
    // Line 1: canvas_width canvas_height bg_char
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    EditorState temp;
    memset(&temp, 0, sizeof(temp));
    char bg;
    if (sscanf(line, "%d %d %c", &temp.canvas_width, &temp.canvas_height, &bg) != 3) {
        fclose(f);
        return 0;
    }
    temp.bg_char = bg;
    
    // Line 2: shape_count
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    int count = 0;
    if (sscanf(line, "%d", &count) != 1) {
        fclose(f);
        return 0;
    }
    
    temp.shape_count = 0;
    int max_id = 0;
    
    for (int i = 0; i < count; i++) {
        if (!fgets(line, sizeof(line), f)) {
            break;
        }
        
        int id, type_int, is_filled;
        char draw_char;
        int n_parsed = 0;
        
        // Parse the common prefix
        if (sscanf(line, "%d %d %c %d%n", &id, &type_int, &draw_char, &is_filled, &n_parsed) < 4) {
            continue;
        }
        
        Shape* s = &temp.shapes[temp.shape_count];
        s->id = id;
        if (id > max_id) max_id = id;
        s->type = (ShapeType)type_int;
        s->params.draw_char = draw_char;
        s->params.is_filled = is_filled;
        s->is_active = 1;
        
        char* params_str = line + n_parsed;
        while (*params_str && isspace((unsigned char)*params_str)) {
            params_str++;
        }
        
        int valid = 0;
        if (s->type == SHAPE_LINE) {
            if (sscanf(params_str, "%d %d %d %d", &s->params.x1, &s->params.y1, &s->params.x2, &s->params.y2) == 4) {
                valid = 1;
            }
        } else if (s->type == SHAPE_RECTANGLE) {
            if (sscanf(params_str, "%d %d %d %d", &s->params.x1, &s->params.y1, &s->params.w, &s->params.h) == 4) {
                valid = 1;
            }
        } else if (s->type == SHAPE_CIRCLE) {
            if (sscanf(params_str, "%d %d %d", &s->params.x1, &s->params.y1, &s->params.r) == 3) {
                valid = 1;
            }
        } else if (s->type == SHAPE_TRIANGLE) {
            if (sscanf(params_str, "%d %d %d %d %d %d", &s->params.x1, &s->params.y1, &s->params.x2, &s->params.y2, &s->params.x3, &s->params.y3) == 6) {
                valid = 1;
            }
        } else if (s->type == SHAPE_TEXT) {
            int text_x, text_y;
            int text_n = 0;
            if (sscanf(params_str, "%d %d%n", &text_x, &text_y, &text_n) >= 2) {
                s->params.x1 = text_x;
                s->params.y1 = text_y;
                char* text_ptr = params_str + text_n;
                while (*text_ptr && isspace((unsigned char)*text_ptr)) {
                    text_ptr++;
                }
                
                // Copy the rest of the string
                strncpy(s->params.text, text_ptr, sizeof(s->params.text) - 1);
                s->params.text[sizeof(s->params.text) - 1] = '\0';
                
                // Trim trailing newline/carriage return
                int len = strlen(s->params.text);
                while (len > 0 && (s->params.text[len - 1] == '\n' || s->params.text[len - 1] == '\r')) {
                    s->params.text[len - 1] = '\0';
                    len--;
                }
                valid = 1;
            }
        }
        
        if (valid) {
            temp.shape_count++;
            if (temp.shape_count >= MAX_SHAPES) break;
        }
    }
    
    fclose(f);
    *state = temp;
    return max_id + 1;
}

// Export ASCII drawing to file
int export_canvas(const EditorState* state, const char* filename) {
    FILE* f = fopen(filename, "w");
    if (!f) return 0;

    char canvas[MAX_HEIGHT][MAX_WIDTH];
    for (int y = 0; y < state->canvas_height; y++) {
        for (int x = 0; x < state->canvas_width; x++) {
            canvas[y][x] = state->bg_char;
        }
    }

    for (int i = 0; i < state->shape_count; i++) {
        if (state->shapes[i].is_active) {
            const Shape* s = &state->shapes[i];
            switch (s->type) {
                case SHAPE_LINE:
                    draw_line(canvas, state->canvas_width, state->canvas_height,
                              s->params.x1, s->params.y1, s->params.x2, s->params.y2, s->params.draw_char);
                    break;
                case SHAPE_RECTANGLE:
                    draw_rectangle(canvas, state->canvas_width, state->canvas_height,
                                   s->params.x1, s->params.y1, s->params.w, s->params.h, s->params.draw_char, s->params.is_filled);
                    break;
                case SHAPE_CIRCLE:
                    draw_circle(canvas, state->canvas_width, state->canvas_height,
                                s->params.x1, s->params.y1, s->params.r, s->params.draw_char);
                    break;
                case SHAPE_TRIANGLE:
                    draw_triangle(canvas, state->canvas_width, state->canvas_height,
                                  s->params.x1, s->params.y1, s->params.x2, s->params.y2, s->params.x3, s->params.y3,
                                  s->params.draw_char, s->params.is_filled);
                    break;
                case SHAPE_TEXT:
                    draw_text(canvas, state->canvas_width, state->canvas_height,
                              s->params.x1, s->params.y1, s->params.text);
                    break;
            }
        }
    }

    for (int y = 0; y < state->canvas_height; y++) {
        for (int x = 0; x < state->canvas_width; x++) {
            fputc(canvas[y][x], f);
        }
        fputc('\n', f);
    }

    fclose(f);
    return 1;
}

// Special parser to extract add text parameters correctly supporting spaces
int parse_add_text(const char* input_str, int* x, int* y, char* text_out, int max_text_len) {
    const char* p = strstr(input_str, "add");
    if (!p) return 0;
    p = strstr(p, "text");
    if (!p) return 0;
    p += 4; // Skip "text"
    
    while (*p && isspace((unsigned char)*p)) p++;
    char* endp;
    *x = (int)strtol(p, &endp, 10);
    if (p == endp) return 0;
    p = endp;
    
    while (*p && isspace((unsigned char)*p)) p++;
    *y = (int)strtol(p, &endp, 10);
    if (p == endp) return 0;
    p = endp;
    
    while (*p && isspace((unsigned char)*p)) p++;
    
    strncpy(text_out, p, max_text_len - 1);
    text_out[max_text_len - 1] = '\0';
    
    int len = strlen(text_out);
    while (len > 0 && (text_out[len - 1] == '\n' || text_out[len - 1] == '\r')) {
        text_out[len - 1] = '\0';
        len--;
    }
    return 1;
}

int main() {
    // Enable ANSI Virtual Terminal Colors on Windows Console
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    SetConsoleOutputCP(CP_UTF8);
#endif

    EditorState state;
    memset(&state, 0, sizeof(state));
    state.canvas_width = DEFAULT_WIDTH;
    state.canvas_height = DEFAULT_HEIGHT;
    state.bg_char = '.';
    state.shape_count = 0;

    int next_id = 1;
    push_history(&state);

    char input[256];
    char status_msg[128] = "Welcome to the 2D Vector Graphics Editor! Type 'help' for command manual.";
    int status_is_error = 0;
    int running = 1;
    int showing_help = 0;

    while (running) {
        if (showing_help) {
            print_help_screen();
        } else {
            render_editor(&state, status_msg, status_is_error);
        }

        // Print input prompt
        if (showing_help) {
            printf("\nhelp_editor> ");
        } else {
            printf("\neditor> ");
        }
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        // Make copies and trim
        char trimmed[256];
        strncpy(trimmed, input, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        char* cleaned = trim(trimmed);

        if (strlen(cleaned) == 0) {
            if (showing_help) {
                showing_help = 0;
                status_is_error = 0;
                strcpy(status_msg, "Returned to canvas.");
            }
            continue;
        }

        // Parse token
        char input_copy[256];
        strcpy(input_copy, cleaned);
        char* token = strtok(input_copy, " \t\r\n");
        if (!token) continue;

        // Reset help mode if a new command is typed
        if (showing_help && strcmp(token, "help") != 0) {
            showing_help = 0;
        }

        status_is_error = 0;
        status_msg[0] = '\0';

        if (strcmp(token, "exit") == 0 || strcmp(token, "quit") == 0) {
            running = 0;
            printf("\nExiting editor. Goodbye!\n");
        } 
        else if (strcmp(token, "help") == 0) {
            showing_help = 1;
            strcpy(status_msg, "Displaying help manual.");
        } 
        else if (strcmp(token, "undo") == 0) {
            if (history_index > 0) {
                history_index--;
                state = history[history_index];
                strcpy(status_msg, "Undo successful.");
            } else {
                status_is_error = 1;
                strcpy(status_msg, "Nothing to undo.");
            }
        } 
        else if (strcmp(token, "redo") == 0) {
            if (history_index < history_count - 1) {
                history_index++;
                state = history[history_index];
                strcpy(status_msg, "Redo successful.");
            } else {
                status_is_error = 1;
                strcpy(status_msg, "Nothing to redo.");
            }
        } 
        else if (strcmp(token, "clear") == 0) {
            state.shape_count = 0;
            push_history(&state);
            strcpy(status_msg, "Workspace cleared.");
        } 
        else if (strcmp(token, "list") == 0) {
            strcpy(status_msg, "Active shapes list updated.");
        } 
        else if (strcmp(token, "canvas") == 0) {
            int w, h;
            char bg = '\0';
            char* w_tok = strtok(NULL, " \t\r\n");
            char* h_tok = strtok(NULL, " \t\r\n");
            char* bg_tok = strtok(NULL, " \t\r\n");

            if (!w_tok || !h_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: canvas <w> <h> [bg_char]");
            } else {
                w = atoi(w_tok);
                h = atoi(h_tok);
                if (bg_tok) bg = bg_tok[0];
                else bg = state.bg_char;

                if (w <= 0 || w > MAX_WIDTH || h <= 0 || h > MAX_HEIGHT) {
                    status_is_error = 1;
                    sprintf(status_msg, "Canvas dimensions must be between 1x1 and %dx%d.", MAX_WIDTH, MAX_HEIGHT);
                } else {
                    state.canvas_width = w;
                    state.canvas_height = h;
                    state.bg_char = bg;
                    push_history(&state);
                    sprintf(status_msg, "Canvas resized to %dx%d, background: '%c'.", w, h, bg);
                }
            }
        } 
        else if (strcmp(token, "remove") == 0) {
            char* id_tok = strtok(NULL, " \t\r\n");
            if (!id_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: remove <shape_id>");
            } else {
                int target_id = atoi(id_tok);
                int found = 0;
                for (int i = 0; i < state.shape_count; i++) {
                    if (state.shapes[i].is_active && state.shapes[i].id == target_id) {
                        state.shapes[i].is_active = 0;
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    push_history(&state);
                    sprintf(status_msg, "Shape ID %d removed successfully.", target_id);
                } else {
                    status_is_error = 1;
                    sprintf(status_msg, "Active shape with ID %d not found.", target_id);
                }
            }
        } 
        else if (strcmp(token, "move") == 0) {
            char* id_tok = strtok(NULL, " \t\r\n");
            char* dx_tok = strtok(NULL, " \t\r\n");
            char* dy_tok = strtok(NULL, " \t\r\n");

            if (!id_tok || !dx_tok || !dy_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: move <id> <dx> <dy>");
            } else {
                int target_id = atoi(id_tok);
                int dx = atoi(dx_tok);
                int dy = atoi(dy_tok);
                int found = 0;
                
                for (int i = 0; i < state.shape_count; i++) {
                    if (state.shapes[i].is_active && state.shapes[i].id == target_id) {
                        Shape* s = &state.shapes[i];
                        s->params.x1 += dx;
                        s->params.y1 += dy;
                        s->params.x2 += dx;
                        s->params.y2 += dy;
                        s->params.x3 += dx;
                        s->params.y3 += dy;
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    push_history(&state);
                    sprintf(status_msg, "Shape ID %d translated by (%d, %d).", target_id, dx, dy);
                } else {
                    status_is_error = 1;
                    sprintf(status_msg, "Active shape with ID %d not found.", target_id);
                }
            }
        } 
        else if (strcmp(token, "color") == 0 || strcmp(token, "char") == 0) {
            char* id_tok = strtok(NULL, " \t\r\n");
            char* char_tok = strtok(NULL, " \t\r\n");

            if (!id_tok || !char_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: color/char <id> <new_char>");
            } else {
                int target_id = atoi(id_tok);
                char new_c = char_tok[0];
                int found = 0;

                for (int i = 0; i < state.shape_count; i++) {
                    if (state.shapes[i].is_active && state.shapes[i].id == target_id) {
                        if (state.shapes[i].type == SHAPE_TEXT) {
                            status_is_error = 1;
                            strcpy(status_msg, "Cannot color a text shape (it uses standard text chars).");
                        } else {
                            state.shapes[i].params.draw_char = new_c;
                            found = 1;
                        }
                        break;
                    }
                }
                if (found) {
                    push_history(&state);
                    sprintf(status_msg, "Shape ID %d color changed to '%c'.", target_id, new_c);
                } else if (!status_is_error) {
                    status_is_error = 1;
                    sprintf(status_msg, "Active shape with ID %d not found.", target_id);
                }
            }
        }
        else if (strcmp(token, "resize") == 0) {
            char* id_tok = strtok(NULL, " \t\r\n");
            if (!id_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: resize <id> <new_params...>");
            } else {
                int target_id = atoi(id_tok);
                int found = 0;
                
                for (int i = 0; i < state.shape_count; i++) {
                    if (state.shapes[i].is_active && state.shapes[i].id == target_id) {
                        Shape* s = &state.shapes[i];
                        found = 1;
                        
                        if (s->type == SHAPE_LINE) {
                            char* x2_tok = strtok(NULL, " \t\r\n");
                            char* y2_tok = strtok(NULL, " \t\r\n");
                            if (!x2_tok || !y2_tok) {
                                status_is_error = 1;
                                strcpy(status_msg, "Resize line requires: resize <id> <new_x2> <new_y2>");
                            } else {
                                s->params.x2 = atoi(x2_tok);
                                s->params.y2 = atoi(y2_tok);
                            }
                        } else if (s->type == SHAPE_RECTANGLE) {
                            char* w_tok = strtok(NULL, " \t\r\n");
                            char* h_tok = strtok(NULL, " \t\r\n");
                            if (!w_tok || !h_tok) {
                                status_is_error = 1;
                                strcpy(status_msg, "Resize rectangle requires: resize <id> <new_w> <new_h>");
                            } else {
                                int w_val = atoi(w_tok);
                                int h_val = atoi(h_tok);
                                if (w_val <= 0 || h_val <= 0) {
                                    status_is_error = 1;
                                    strcpy(status_msg, "Width and height must be positive.");
                                } else {
                                    s->params.w = w_val;
                                    s->params.h = h_val;
                                }
                            }
                        } else if (s->type == SHAPE_CIRCLE) {
                            char* r_tok = strtok(NULL, " \t\r\n");
                            if (!r_tok) {
                                status_is_error = 1;
                                strcpy(status_msg, "Resize circle requires: resize <id> <new_r>");
                            } else {
                                int r_val = atoi(r_tok);
                                if (r_val <= 0) {
                                    status_is_error = 1;
                                    strcpy(status_msg, "Radius must be positive.");
                                } else {
                                    s->params.r = r_val;
                                }
                            }
                        } else if (s->type == SHAPE_TRIANGLE) {
                            char* x2_tok = strtok(NULL, " \t\r\n");
                            char* y2_tok = strtok(NULL, " \t\r\n");
                            char* x3_tok = strtok(NULL, " \t\r\n");
                            char* y3_tok = strtok(NULL, " \t\r\n");
                            if (!x2_tok || !y2_tok || !x3_tok || !y3_tok) {
                                status_is_error = 1;
                                strcpy(status_msg, "Resize triangle requires: resize <id> <x2> <y2> <x3> <y3>");
                            } else {
                                s->params.x2 = atoi(x2_tok);
                                s->params.y2 = atoi(y2_tok);
                                s->params.x3 = atoi(x3_tok);
                                s->params.y3 = atoi(y3_tok);
                            }
                        } else if (s->type == SHAPE_TEXT) {
                            status_is_error = 1;
                            strcpy(status_msg, "Resize not supported for Text. Add a new text shape.");
                        }
                        break;
                    }
                }
                if (found && !status_is_error) {
                    push_history(&state);
                    sprintf(status_msg, "Shape ID %d resized successfully.", target_id);
                } else if (!found) {
                    status_is_error = 1;
                    sprintf(status_msg, "Active shape with ID %d not found.", target_id);
                }
            }
        }
        else if (strcmp(token, "save") == 0) {
            char* file_tok = strtok(NULL, " \t\r\n");
            if (!file_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: save <filename>");
            } else {
                if (save_editor(&state, file_tok)) {
                    sprintf(status_msg, "Saved shape state successfully to '%s'.", file_tok);
                } else {
                    status_is_error = 1;
                    sprintf(status_msg, "Failed to open or write to file '%s'.", file_tok);
                }
            }
        } 
        else if (strcmp(token, "load") == 0) {
            char* file_tok = strtok(NULL, " \t\r\n");
            if (!file_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: load <filename>");
            } else {
                int res = load_editor(&state, file_tok);
                if (res > 0) {
                    next_id = res;
                    push_history(&state);
                    sprintf(status_msg, "Loaded shapes from '%s' successfully.", file_tok);
                } else {
                    status_is_error = 1;
                    sprintf(status_msg, "Failed to load shape state from file '%s'. Ensure format is correct.", file_tok);
                }
            }
        } 
        else if (strcmp(token, "export") == 0) {
            char* file_tok = strtok(NULL, " \t\r\n");
            if (!file_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: export <filename>");
            } else {
                if (export_canvas(&state, file_tok)) {
                    sprintf(status_msg, "Exported canvas successfully as picture file to '%s'.", file_tok);
                } else {
                    status_is_error = 1;
                    sprintf(status_msg, "Failed to export drawing to file '%s'.", file_tok);
                }
            }
        } 
        else if (strcmp(token, "add") == 0) {
            char* type_tok = strtok(NULL, " \t\r\n");
            if (!type_tok) {
                status_is_error = 1;
                strcpy(status_msg, "Usage: add <line|rect|circle|tri|text> <args...>");
            } else if (state.shape_count >= MAX_SHAPES) {
                status_is_error = 1;
                strcpy(status_msg, "Maximum number of shapes reached (100). Remove some shapes first.");
            } else {
                Shape* s = &state.shapes[state.shape_count];
                int valid = 0;

                if (strcmp(type_tok, "line") == 0) {
                    char* x1_tok = strtok(NULL, " \t\r\n");
                    char* y1_tok = strtok(NULL, " \t\r\n");
                    char* x2_tok = strtok(NULL, " \t\r\n");
                    char* y2_tok = strtok(NULL, " \t\r\n");
                    char* char_tok = strtok(NULL, " \t\r\n");

                    if (!x1_tok || !y1_tok || !x2_tok || !y2_tok || !char_tok) {
                        status_is_error = 1;
                        strcpy(status_msg, "Usage: add line <x1> <y1> <x2> <y2> <char>");
                    } else {
                        s->id = next_id++;
                        s->type = SHAPE_LINE;
                        s->params.x1 = atoi(x1_tok);
                        s->params.y1 = atoi(y1_tok);
                        s->params.x2 = atoi(x2_tok);
                        s->params.y2 = atoi(y2_tok);
                        s->params.draw_char = char_tok[0];
                        s->params.is_filled = 0;
                        s->is_active = 1;
                        valid = 1;
                        sprintf(status_msg, "Added Line ID %d.", s->id);
                    }
                } 
                else if (strcmp(type_tok, "rect") == 0) {
                    char* x_tok = strtok(NULL, " \t\r\n");
                    char* y_tok = strtok(NULL, " \t\r\n");
                    char* w_tok = strtok(NULL, " \t\r\n");
                    char* h_tok = strtok(NULL, " \t\r\n");
                    char* char_tok = strtok(NULL, " \t\r\n");
                    char* fill_tok = strtok(NULL, " \t\r\n");

                    if (!x_tok || !y_tok || !w_tok || !h_tok || !char_tok) {
                        status_is_error = 1;
                        strcpy(status_msg, "Usage: add rect <x> <y> <w> <h> <char> [filled: 0 or 1]");
                    } else {
                        int w_val = atoi(w_tok);
                        int h_val = atoi(h_tok);
                        if (w_val <= 0 || h_val <= 0) {
                            status_is_error = 1;
                            strcpy(status_msg, "Rectangle width and height must be positive values.");
                        } else {
                            s->id = next_id++;
                            s->type = SHAPE_RECTANGLE;
                            s->params.x1 = atoi(x_tok);
                            s->params.y1 = atoi(y_tok);
                            s->params.w = w_val;
                            s->params.h = h_val;
                            s->params.draw_char = char_tok[0];
                            s->params.is_filled = fill_tok ? atoi(fill_tok) : 0;
                            s->is_active = 1;
                            valid = 1;
                            sprintf(status_msg, "Added Rectangle ID %d.", s->id);
                        }
                    }
                } 
                else if (strcmp(type_tok, "circle") == 0) {
                    char* cx_tok = strtok(NULL, " \t\r\n");
                    char* cy_tok = strtok(NULL, " \t\r\n");
                    char* r_tok = strtok(NULL, " \t\r\n");
                    char* char_tok = strtok(NULL, " \t\r\n");

                    if (!cx_tok || !cy_tok || !r_tok || !char_tok) {
                        status_is_error = 1;
                        strcpy(status_msg, "Usage: add circle <cx> <cy> <r> <char>");
                    } else {
                        int r_val = atoi(r_tok);
                        if (r_val <= 0) {
                            status_is_error = 1;
                            strcpy(status_msg, "Circle radius must be a positive value.");
                        } else {
                            s->id = next_id++;
                            s->type = SHAPE_CIRCLE;
                            s->params.x1 = atoi(cx_tok);
                            s->params.y1 = atoi(cy_tok);
                            s->params.r = r_val;
                            s->params.draw_char = char_tok[0];
                            s->params.is_filled = 0;
                            s->is_active = 1;
                            valid = 1;
                            sprintf(status_msg, "Added Circle ID %d.", s->id);
                        }
                    }
                } 
                else if (strcmp(type_tok, "tri") == 0) {
                    char* x1_tok = strtok(NULL, " \t\r\n");
                    char* y1_tok = strtok(NULL, " \t\r\n");
                    char* x2_tok = strtok(NULL, " \t\r\n");
                    char* y2_tok = strtok(NULL, " \t\r\n");
                    char* x3_tok = strtok(NULL, " \t\r\n");
                    char* y3_tok = strtok(NULL, " \t\r\n");
                    char* char_tok = strtok(NULL, " \t\r\n");
                    char* fill_tok = strtok(NULL, " \t\r\n");

                    if (!x1_tok || !y1_tok || !x2_tok || !y2_tok || !x3_tok || !y3_tok || !char_tok) {
                        status_is_error = 1;
                        strcpy(status_msg, "Usage: add tri <x1> <y1> <x2> <y2> <x3> <y3> <char> [filled: 0 or 1]");
                    } else {
                        s->id = next_id++;
                        s->type = SHAPE_TRIANGLE;
                        s->params.x1 = atoi(x1_tok);
                        s->params.y1 = atoi(y1_tok);
                        s->params.x2 = atoi(x2_tok);
                        s->params.y2 = atoi(y2_tok);
                        s->params.x3 = atoi(x3_tok);
                        s->params.y3 = atoi(y3_tok);
                        s->params.draw_char = char_tok[0];
                        s->params.is_filled = fill_tok ? atoi(fill_tok) : 0;
                        s->is_active = 1;
                        valid = 1;
                        sprintf(status_msg, "Added Triangle ID %d.", s->id);
                    }
                } 
                else if (strcmp(type_tok, "text") == 0) {
                    int x_val, y_val;
                    char text_val[64];
                    if (!parse_add_text(cleaned, &x_val, &y_val, text_val, sizeof(text_val))) {
                        status_is_error = 1;
                        strcpy(status_msg, "Usage: add text <x> <y> <text string>");
                    } else {
                        s->id = next_id++;
                        s->type = SHAPE_TEXT;
                        s->params.x1 = x_val;
                        s->params.y1 = y_val;
                        strncpy(s->params.text, text_val, sizeof(s->params.text) - 1);
                        s->params.text[sizeof(s->params.text) - 1] = '\0';
                        s->params.is_filled = 0;
                        s->is_active = 1;
                        valid = 1;
                        sprintf(status_msg, "Added Text ID %d.", s->id);
                    }
                } 
                else {
                    status_is_error = 1;
                    sprintf(status_msg, "Unknown shape type '%s'. Supported: line, rect, circle, tri, text.", type_tok);
                }

                if (valid) {
                    state.shape_count++;
                    push_history(&state);
                }
            }
        } 
        else {
            status_is_error = 1;
            sprintf(status_msg, "Unknown command '%s'. Type 'help' to view all commands.", token);
        }
    }

    return 0;
}
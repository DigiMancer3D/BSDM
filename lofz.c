#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

// --- Constants and Macros ---
#define GRID_SIZE 4
#define GRID_COUNT 16
#define CELL_W 20
#define CELL_H 10
#define BOARD_TOP 0
#define BOARD_LEFT 0
#define BOARD_W (GRID_SIZE * CELL_W)
#define BOARD_H (GRID_SIZE * CELL_H)
#define MASCOT_X 92
#define MASCOT_Y 14
#define PLAYER_CELL_X 80
#define PLAYER_CELL_Y 30
#define LINE_HEIGHT 11
#define POPUP_WIDTH 100
#define POPUP_X 14
#define POPUP_Y 48
#define SCROLL_SPEED 650 // ms per character scroll
#define CRAWL_SPEED 40   // ms per pixel scroll
#define CREDITS_COOLDOWN_MS 180000 // 3 minutes
#define CREDITS_MOVE_THRESHOLD 4
#define AUDIO_TOGGLE_TIMEOUT 1350 // 1.35 seconds

typedef enum {
    GameState_Loading,
    GameState_PressToPlay,
    GameState_IntroCrawl,
    GameState_IntroFade,
    GameState_PlayerTurn,
    GameState_FlipperTurn,
    GameState_FlipperPopup,
    GameState_Finished,
    GameState_ExitScreen,
    GameState_LostNulls,
    GameState_LostFade,
    GameState_LostMenu,
    GameState_WinScreen,
    GameState_Credits
} GameState;

typedef struct {
    int8_t grid[GRID_COUNT];
    uint8_t player_cursor;
    uint8_t flipper_cursor;
    bool solved;
    GameState state;
    uint32_t popup_timer;
    uint8_t popup_phrase;
    uint32_t scroll_start_tick;
    int scroll_offset;
    uint32_t state_tick;
    int intro_crawl_y;
    int fade_level;
    int menu_sel;
    uint8_t flipper_move_index;
    uint32_t right_key_count;
    uint32_t last_right_key_tick;
    uint32_t left_key_count;    // For audio toggle (placeholder)
    uint32_t last_left_key_tick; // For audio toggle (placeholder)
    uint32_t back_key_count;
    uint32_t last_back_key_tick;
    int mascot_bounce_offset;
    uint32_t bounce_start_tick;
    bool bounce_up;
    int8_t saved_grid[GRID_COUNT];
    uint8_t saved_player_cursor;
    uint8_t saved_flipper_cursor;
    bool saved_solved;
    uint32_t credits_access_count; // Tracks credits access attempts
    uint32_t last_credits_tick;    // Timestamp of last credits display
    uint32_t user_move_count;      // Tracks OK presses
    bool audio_enabled;            // Placeholder for future audio
} LOFZGame;

static const uint8_t flipper_move_sequence[] = {0, 1, 3, 0, 2, 4, 1, 0};
#define FLIPPER_SEQUENCE_COUNT (sizeof(flipper_move_sequence) / sizeof(flipper_move_sequence[0]))

// --- Evil Sith Flipper Mascot Bitmap (16x16 px, 1bpp) ---
static const uint8_t sith_flipper_mascot[32] = {
    0b00000111, 0b11100000, 0b00011111, 0b11111000, 0b00111111, 0b11111100, 0b01110000, 0b00001110,
    0b01100000, 0b00000110, 0b11000111, 0b11100011, 0b11001111, 0b11110011, 0b11111111, 0b11111111,
    0b11111111, 0b11111111, 0b11111110, 0b01111111, 0b01111100, 0b00111110, 0b00111000, 0b00011100,
    0b00010000, 0b00001000, 0b00100000, 0b00000100, 0b01000011, 0b11000010, 0b00011111, 0b11111000,
};

// --- Flipper AI phrases ---
static const char* flipper_phrases[] = {
    "Flipper is Playing his Move",
    "Analyzing Board...",
    "Flipper's Turn!",
    "Diffusion in Progress",
    "Watch My Move!",
    "  WMM!  ",
    "Flipper is Thinking...",
    "Hold up, its my move",
    "Wait...Oh yeah!",
    "Hmmm...",
    "That's a good one",
    "I got this",
    "Please let me move now",
    "My TURN!"
};
#define FLIPPER_PHRASE_COUNT (sizeof(flipper_phrases) / sizeof(flipper_phrases[0]))

// --- Intro and Credits Lines ---
static const char* intro_lines[] = {
    "  ",
    "LOFZ",
    "Lights Out Flipper Zero",
    "  ",
    "  ",
    "A long time ago in a",
    "galaxy far, far away....",
    "  ",
    "  ",
    "The evil ",
    "Flipper Zero, Sith Lord,",
    "  ",
    "has covered the ",
    "galaxy in light!",
    "  ",
    "No one has slept in ages",
    "  ",
    "  ",
    "You are the last Jedi,",
    "  ",
    "your mission is ",
    "to turn the",
    "lights out ",
    "and vanquish Flipper!",
    "  ",
    "  ",
    "Use D-pad to move. ",
    "OK to select.",
    "  ",
    "Back button 2x+ Exits",
    "  ",
    "Game Goal:",
    "Turn all boxes dark.",
    "  ",
    "If all tiles are lit, the Sith wins.",
    "  ",
    "May the Force be with you.",
    "  ",
    "Press OK to continue",
    "  ",
};
#define INTRO_LINES_COUNT (sizeof(intro_lines) / sizeof(intro_lines[0]))

static const char* credits_lines[] = {
    "  ",
    "LOFZ Credits",
    "  ",
    "Created by 3DPihl",
    "Inspired by Star Wars",
    "Powered by Flipper Zero",
    "  ",
    "Assisted by:",
    "Github",
    "Copilot AI",
    "Grok AI",
    "xTwitter",
    "  ",
    ":Special Thanks:",
    "Thanks to the ",
    "Flipper community",
    "for your support",
    "  ",
    "May the Force be with you!",
    "  ",
    "Press OK to continue",
    "  ",
};
#define CREDITS_LINES_COUNT (sizeof(credits_lines) / sizeof(credits_lines[0]))

// --- Drawing Helpers ---
static void draw_mascot(Canvas* canvas, int x, int y, int bounce_offset, bool bounce_up) {
    int offset = bounce_up ? -bounce_offset : bounce_offset;
    for(int row = 0; row < 16; row++) {
        uint16_t row_bits = (sith_flipper_mascot[row * 2] << 8) | sith_flipper_mascot[row * 2 + 1];
        for(int col = 0; col < 16; col++) {
            if(row_bits & (0x8000 >> col)) canvas_draw_dot(canvas, x + col, y + row + offset);
        }
    }
}

static void draw_crawl_text(Canvas* canvas, int y, const char* const* lines, int line_count) {
    canvas_set_font(canvas, FontPrimary);
    int valid_lines = 0;
    for(int i = 0; i < line_count; i++) {
        if(lines[i][0] != '\0') valid_lines++;
    }
    int valid_idx = 0;
    for(int i = 0; i < line_count; i++) {
        if(lines[i][0] == '\0') continue;
        int y_pos = y + valid_idx * LINE_HEIGHT;
        if(y_pos < -LINE_HEIGHT || y_pos > 64) {
            valid_idx++;
            continue;
        }
        float fade = 1.0f;
        if(y_pos < 10) fade = (float)y_pos / 10.0f;
        else if(y_pos > 54) fade = (float)(64 - y_pos) / 10.0f;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y_pos - 2, 128, LINE_HEIGHT - 1);
        if(fade > 0.0f) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, y_pos, AlignCenter, AlignCenter, lines[i]);
        }
        valid_idx++;
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_scrolling_text(Canvas* canvas, int x, int y, const char* text, int width, LOFZGame* game, bool update_scroll) {
    canvas_set_font(canvas, FontPrimary);
    int text_len = strlen(text);
    int chars_fit = width / 7;
    int scroll_limit = text_len - chars_fit + 1;
    if(scroll_limit < 0) scroll_limit = 0;
    int scroll = game->scroll_offset;
    if(scroll > scroll_limit) scroll = scroll_limit;
    char buf[64];
    if(scroll < text_len) {
        strncpy(buf, text + scroll, chars_fit);
        buf[chars_fit] = '\0';
    } else {
        buf[0] = '\0';
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, x - 2, y - 2, width + 4, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, x, y, buf);
    if(update_scroll && (furi_get_tick() - game->scroll_start_tick > SCROLL_SPEED)) {
        game->scroll_start_tick = furi_get_tick();
        game->scroll_offset++;
        if(game->scroll_offset > scroll_limit) game->scroll_offset = 0;
    }
}

static void draw_3d_board(Canvas* canvas, LOFZGame* game) {
    canvas_set_color(canvas, ColorWhite);
    int top_left_x = 0, top_left_y = 0;
    int top_right_x = 64, top_right_y = 0;
    int bottom_left_x = 0, bottom_left_y = 64;
    int bottom_right_x = 85, bottom_right_y = 64;

    canvas_draw_line(canvas, top_left_x, top_left_y, top_right_x, top_right_y);
    canvas_draw_line(canvas, top_left_x, top_left_y, bottom_left_x, bottom_left_y);
    canvas_draw_line(canvas, top_right_x, top_right_y, bottom_right_x, bottom_right_y);
    canvas_draw_line(canvas, bottom_left_x, bottom_left_y, bottom_right_x, bottom_right_y);

    for(uint8_t row = 0; row < GRID_SIZE; row++) {
        for(uint8_t col = 0; col < GRID_SIZE; col++) {
            int idx = row * GRID_SIZE + col;
            float t = (float)row / GRID_SIZE;
            int x1 = top_left_x + (int)((bottom_left_x - top_left_x) * t);
            int x2 = top_right_x + (int)((bottom_right_x - top_right_x) * t);
            int y = top_left_y + (int)((bottom_left_y - top_left_y) * t);
            int cell_x = x1 + (int)((x2 - x1) * (float)col / GRID_SIZE);
            int cell_x_next = x1 + (int)((x2 - x1) * (float)(col + 1) / GRID_SIZE);
            int cell_y = y;
            int cell_y_next = top_left_y + (int)((bottom_left_y - top_left_y) * (float)(row + 1) / GRID_SIZE);
            int cell_w = cell_x_next - cell_x;
            int cell_h = cell_y_next - cell_y;

            bool is_cursor = (idx == game->player_cursor && game->state == GameState_PlayerTurn);

            if(game->grid[idx] != 0) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, cell_x + 1, cell_y + 1, cell_w - 2, cell_h - 2);
            } else {
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_frame(canvas, cell_x + 1, cell_y + 1, cell_w - 2, cell_h - 2);
            }

            if(is_cursor) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_frame(canvas, cell_x, cell_y, cell_w, cell_h);
                canvas_set_color(canvas, game->grid[idx] ? ColorWhite : ColorBlack);
                canvas_draw_str(canvas, cell_x + cell_w / 2 - 2, cell_y + cell_h / 2 + 2, game->grid[idx] ? "-" : "+");
            }
        }
    }

    bool is_player_cell = (game->player_cursor == GRID_COUNT && game->state == GameState_PlayerTurn);
    if(game->grid[GRID_COUNT - 1] != 0) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, PLAYER_CELL_X + 1, PLAYER_CELL_Y + 1, CELL_W - 2, CELL_H - 2);
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_frame(canvas, PLAYER_CELL_X + 1, PLAYER_CELL_Y + 1, CELL_W - 2, CELL_H - 2);
    }
    if(is_player_cell) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, PLAYER_CELL_X, PLAYER_CELL_Y, CELL_W, CELL_H);
        canvas_set_color(canvas, game->grid[GRID_COUNT - 1] ? ColorWhite : ColorBlack);
        canvas_draw_str(canvas, PLAYER_CELL_X + CELL_W / 2 - 2, PLAYER_CELL_Y + CELL_H / 2 + 2, game->grid[GRID_COUNT - 1] ? "-" : "+");
    }

    canvas_set_color(canvas, ColorBlack);
}

static bool is_lost_nulls(LOFZGame* game) {
    for(uint8_t i = 0; i < GRID_COUNT; i++)
        if(game->grid[i] == 0) return false;
    return true;
}

static void lofz_update(LOFZGame* game, uint8_t idx) {
    static const int8_t dx[4] = {0, -1, 0, 1}; // Top, Left, Bottom, Right
    static const int8_t dy[4] = {-1, 0, 1, 0};
    uint8_t x = idx % GRID_SIZE, y = idx / GRID_SIZE;

    game->grid[idx] = (game->grid[idx] == 0) ? 1 : 0;

    if(idx == GRID_COUNT - 1) {
        game->grid[5] = (game->grid[5] == 0) ? 1 : 0;
        game->grid[6] = (game->grid[6] == 0) ? 1 : 0;
        game->grid[9] = (game->grid[9] == 0) ? 1 : 0;
        game->grid[10] = (game->grid[10] == 0) ? 1 : 0;
    } else {
        for(int n = 0; n < 4; n++) {
            int nx = x + dx[n], ny = y + dy[n];
            if(nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                game->grid[ny * GRID_SIZE + nx] = (game->grid[ny * GRID_SIZE + nx] == 0) ? 1 : 0;
            } else if(idx >= 5 && idx <= 10 && (idx == 5 || idx == 6 || idx == 9 || idx == 10)) {
                if(n == (rand() % 4)) {
                    game->grid[GRID_COUNT - 1] = (game->grid[GRID_COUNT - 1] == 0) ? 1 : 0;
                }
            }
        }
    }
}

static bool lofz_is_solved(LOFZGame* game) {
    for(uint8_t i = 0; i < GRID_COUNT; i++)
        if(game->grid[i] != 0) return false;
    return true;
}

static void lofz_flipper_move(LOFZGame* game) {
    uint8_t best_idx = 0;
    int min_dist = 999;
    for(uint8_t i = 0; i < GRID_COUNT; i++) {
        if(game->grid[i] != 0) {
            int px = game->player_cursor % GRID_SIZE, py = game->player_cursor / GRID_SIZE;
            int fx = i % GRID_SIZE, fy = i / GRID_SIZE;
            int dist = abs(px - fx) + abs(py - fy);
            if(dist < min_dist) {
                best_idx = i;
                min_dist = dist;
            }
        }
    }
    game->flipper_cursor = best_idx;
    lofz_update(game, best_idx);
    game->bounce_start_tick = furi_get_tick();
    game->mascot_bounce_offset = 6;
    game->bounce_up = (rand() % 2) == 0;
}

// --- Input Handler ---
static void lofz_input(InputEvent* event, void* ctx) {
    LOFZGame* game = (LOFZGame*)ctx;

    if(game->state == GameState_Loading || game->state == GameState_IntroFade || game->state == GameState_LostFade) return;

    if(event->type == InputTypePress) {
        // Handle audio toggle with 3x Left key (placeholder)
        if(event->key == InputKeyLeft && (game->state == GameState_PlayerTurn || game->state == GameState_PressToPlay || game->state == GameState_IntroCrawl || game->state == GameState_WinScreen || game->state == GameState_LostMenu)) {
            if(furi_get_tick() - game->last_left_key_tick <= AUDIO_TOGGLE_TIMEOUT) {
                game->left_key_count++;
                if(game->left_key_count >= 3) {
                    game->audio_enabled = !game->audio_enabled;
                    game->left_key_count = 0;
                }
            } else {
                game->left_key_count = 1;
            }
            game->last_left_key_tick = furi_get_tick();
        }

        if(game->state == GameState_PressToPlay && event->key == InputKeyOk) {
            game->state = GameState_IntroCrawl;
            game->state_tick = furi_get_tick();
            return;
        }
        if(game->state == GameState_IntroCrawl && event->key == InputKeyOk) {
            game->state = GameState_IntroFade;
            game->fade_level = 0;
            game->state_tick = furi_get_tick();
            return;
        }
        if(game->state == GameState_Credits && event->key == InputKeyOk) {
            game->state = GameState_IntroCrawl;
            game->intro_crawl_y = 64;
            game->state_tick = furi_get_tick();
            memcpy(game->grid, game->saved_grid, sizeof(game->grid));
            game->player_cursor = game->saved_player_cursor;
            game->flipper_cursor = game->saved_flipper_cursor;
            game->solved = game->saved_solved;
            return;
        }
        if(game->state == GameState_LostMenu) {
            if(event->key == InputKeyUp || event->key == InputKeyDown || event->key == InputKeyLeft || event->key == InputKeyRight) {
                game->menu_sel = 1 - game->menu_sel;
            } else if(event->key == InputKeyOk) {
                if(game->menu_sel == 0) { // Yes
                    memset(game->grid, 0, sizeof(game->grid));
                    game->grid[GRID_COUNT - 1] = 1;
                    game->player_cursor = GRID_COUNT;
                    game->flipper_cursor = 0;
                    game->solved = false;
                    game->flipper_move_index = 0;
                    game->user_move_count = 0;
                    game->state = GameState_PlayerTurn;
                } else { // No
                    game->state = GameState_ExitScreen;
                    game->state_tick = furi_get_tick();
                }
            }
            return;
        }
        if(game->state == GameState_WinScreen && event->key == InputKeyBack) {
            if(furi_get_tick() - game->last_back_key_tick <= 1800) {
                game->back_key_count++;
                if(game->back_key_count >= 2) {
                    game->state = GameState_ExitScreen;
                    game->state_tick = furi_get_tick();
                    game->back_key_count = 0;
                    return;
                }
            } else {
                game->back_key_count = 1;
            }
            game->last_back_key_tick = furi_get_tick();
            return;
        }
        if(game->state == GameState_PlayerTurn && !game->solved) {
            if(event->key == InputKeyRight) {
                if(furi_get_tick() - game->last_right_key_tick <= 1800) {
                    game->right_key_count++;
                    if(game->right_key_count >= 3) {
                        bool allow_credits = false;
                        if(furi_get_tick() - game->last_credits_tick >= CREDITS_COOLDOWN_MS || game->user_move_count >= CREDITS_MOVE_THRESHOLD) {
                            allow_credits = true;
                            game->credits_access_count = 0;
                            game->user_move_count = 0;
                        } else if(game->credits_access_count % 2 == 0) {
                            allow_credits = true;
                        }
                        if(allow_credits) {
                            game->state = GameState_Credits;
                            game->intro_crawl_y = 64;
                            game->state_tick = furi_get_tick();
                            memcpy(game->saved_grid, game->grid, sizeof(game->grid));
                            game->saved_player_cursor = game->player_cursor;
                            game->saved_flipper_cursor = game->flipper_cursor;
                            game->saved_solved = game->solved;
                            game->credits_access_count++;
                            game->last_credits_tick = furi_get_tick();
                            game->right_key_count = 0;
                            return;
                        }
                        game->right_key_count = 0;
                    }
                } else {
                    game->right_key_count = 1;
                }
                game->last_right_key_tick = furi_get_tick();
            }
            if(event->key == InputKeyBack) {
                if(furi_get_tick() - game->last_back_key_tick <= 1800) {
                    game->back_key_count++;
                    if(game->back_key_count >= 2) {
                        game->state = GameState_ExitScreen;
                        game->state_tick = furi_get_tick();
                        game->back_key_count = 0;
                        return;
                    }
                } else {
                    game->back_key_count = 1;
                }
                game->last_back_key_tick = furi_get_tick();
                return;
            }
            switch(event->key) {
                case InputKeyUp:
                    if(game->player_cursor >= GRID_SIZE) game->player_cursor -= GRID_SIZE;
                    break;
                case InputKeyDown:
                    if(game->player_cursor == GRID_COUNT) game->player_cursor = 12;
                    else if(game->player_cursor < GRID_COUNT - GRID_SIZE) game->player_cursor += GRID_SIZE;
                    break;
                case InputKeyLeft:
                    if(game->player_cursor % GRID_SIZE) game->player_cursor--;
                    break;
                case InputKeyRight:
                    if(game->player_cursor % GRID_SIZE == GRID_SIZE - 1) game->player_cursor = GRID_COUNT;
                    else if(game->player_cursor < GRID_COUNT) game->player_cursor++;
                    break;
                case InputKeyOk:
                    game->user_move_count++;
                    lofz_update(game, game->player_cursor);
                    if(lofz_is_solved(game)) {
                        game->solved = true;
                        game->state = GameState_WinScreen;
                        game->state_tick = furi_get_tick();
                    } else if(is_lost_nulls(game)) {
                        game->state = GameState_LostNulls;
                        game->state_tick = furi_get_tick();
                    } else {
                        game->state = GameState_FlipperPopup;
                        game->popup_timer = furi_get_tick();
                        game->popup_phrase = rand() % FLIPPER_PHRASE_COUNT;
                        game->scroll_offset = 0;
                        game->scroll_start_tick = furi_get_tick();
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

// --- Main Draw Function ---
static void lofz_draw(Canvas* canvas, void* ctx) {
    LOFZGame* game = (LOFZGame*)ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(game->state == GameState_Loading) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Loading...");
        return;
    }
    if(game->state == GameState_PressToPlay) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to Play");
        return;
    }
    if(game->state == GameState_IntroCrawl || game->state == GameState_Credits) {
        draw_crawl_text(canvas, game->intro_crawl_y,
                        game->state == GameState_Credits ? credits_lines : intro_lines,
                        game->state == GameState_Credits ? CREDITS_LINES_COUNT : INTRO_LINES_COUNT);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "Press OK to continue");
        return;
    }
    if(game->state == GameState_IntroFade || game->state == GameState_LostFade) {
        draw_3d_board(canvas, game);
        int lvl = game->fade_level;
        for(int i = 0; i < 128; i += 2) canvas_draw_box(canvas, 0, 0, 128, lvl);
        return;
    }
    if(game->state == GameState_LostNulls) {
        draw_3d_board(canvas, game);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_box(canvas, 13, 52, 102, 12);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 18, 61, "You Died - Flipper Wins!");
        canvas_set_color(canvas, ColorBlack);
        draw_mascot(canvas, MASCOT_X, MASCOT_Y, game->mascot_bounce_offset, game->bounce_up);
        canvas_draw_str(canvas, 92, 12, "Flipper");
        return;
    }
    if(game->state == GameState_LostMenu) {
        draw_3d_board(canvas, game);
        canvas_draw_box(canvas, 85, 48, 38, 16);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 108, 56, game->menu_sel == 0 ? ">Yes" : " Yes");
        canvas_draw_str(canvas, 88, 56, game->menu_sel == 1 ? ">No" : " No");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 15, 56, "Play again?");
        draw_mascot(canvas, MASCOT_X, MASCOT_Y, game->mascot_bounce_offset, game->bounce_up);
        canvas_draw_str(canvas, 92, 12, "Flipper");
        return;
    }
    if(game->state == GameState_WinScreen) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "You Win!");
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "The Jedi may sleep");
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "Press Back 2x to exit");
        return;
    }
    if(game->state == GameState_ExitScreen) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Enjoy your day");
        return;
    }
    if(game->state == GameState_FlipperPopup || game->state == GameState_FlipperTurn) {
        draw_3d_board(canvas, game);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_box(canvas, POPUP_X, POPUP_Y, POPUP_WIDTH, 14);
        canvas_set_color(canvas, ColorWhite);
        draw_scrolling_text(canvas, POPUP_X + 2, POPUP_Y + 11, flipper_phrases[game->popup_phrase], POPUP_WIDTH - 4, game, game->state == GameState_FlipperPopup);
        canvas_set_color(canvas, ColorBlack);
        draw_mascot(canvas, MASCOT_X, MASCOT_Y, game->mascot_bounce_offset, game->bounce_up);
        canvas_draw_str(canvas, 92, 12, "Flipper");
        return;
    }
    // Main gameplay
    draw_3d_board(canvas, game);
    draw_mascot(canvas, MASCOT_X, MASCOT_Y, game->mascot_bounce_offset, game->bounce_up);
    canvas_draw_str(canvas, 92, 12, "Flipper");
}

// --- Main App ---
int32_t lofz_app(void* p) {
    (void)p;
    LOFZGame game;
    memset(&game, 0, sizeof(game));
    game.grid[GRID_COUNT - 1] = 1; // Only offset cell starts ON
    game.player_cursor = GRID_COUNT;
    game.flipper_cursor = 0;
    game.menu_sel = 0;
    game.intro_crawl_y = 64;
    game.state = GameState_Loading;
    game.state_tick = furi_get_tick();
    game.flipper_move_index = 0;
    game.audio_enabled = true; // Placeholder for future audio

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, lofz_draw, &game);
    view_port_input_callback_set(viewport, lofz_input, &game);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    bool running = true;
    bool loading_done = false;
    uint8_t flipper_moves_left = 0;

    while(running) {
        if(game.state == GameState_Finished) break;

        if(game.mascot_bounce_offset > 0 && furi_get_tick() - game.bounce_start_tick > 300) {
            game.mascot_bounce_offset = 0;
        }

        if(game.state == GameState_Loading && !loading_done) {
            if(furi_get_tick() - game.state_tick >= 7280) {
                game.state = GameState_PressToPlay;
                loading_done = true;
            }
        } else if(game.state == GameState_IntroCrawl || game.state == GameState_Credits) {
            int valid_lines = 0;
            const char* const* lines = (game.state == GameState_Credits) ? credits_lines : intro_lines;
            int line_count = (game.state == GameState_Credits) ? CREDITS_LINES_COUNT : INTRO_LINES_COUNT;
            for(int i = 0; i < line_count; i++) {
                if(lines[i][0] != '\0') valid_lines++;
            }
            int crawl_end = -(valid_lines * LINE_HEIGHT);
            if(game.intro_crawl_y > crawl_end) {
                if(furi_get_tick() - game.state_tick > CRAWL_SPEED) {
                    game.intro_crawl_y -= 1;
                    game.state_tick = furi_get_tick();
                }
            } else if(game.state == GameState_IntroCrawl) {
                game.state = GameState_IntroFade;
                game.fade_level = 0;
                game.state_tick = furi_get_tick();
            } else {
                game.state = GameState_IntroCrawl;
                game.intro_crawl_y = 64;
                game.state_tick = furi_get_tick();
                memcpy(game.grid, game.saved_grid, sizeof(game.grid));
                game.player_cursor = game.saved_player_cursor;
                game.flipper_cursor = game.saved_flipper_cursor;
                game.solved = game.saved_solved;
            }
        } else if(game.state == GameState_IntroFade) {
            if(game.fade_level < 128) game.fade_level += 8;
            else {
                game.state = GameState_PlayerTurn;
                game.fade_level = 0;
                game.intro_crawl_y = 64;
            }
        } else if(game.state == GameState_LostNulls) {
            if(furi_get_tick() - game.state_tick > 2000) {
                game.state = GameState_LostMenu;
                game.menu_sel = 0;
                game.state_tick = furi_get_tick();
            }
        } else if(game.state == GameState_LostFade) {
            if(game.fade_level < 128) game.fade_level += 8;
            else {
                game.state = GameState_LostNulls;
                game.state_tick = furi_get_tick();
            }
        } else if(game.state == GameState_FlipperPopup && !game.solved) {
            int chars_fit = (POPUP_WIDTH - 4) / 7;
            int scroll_limit = strlen(flipper_phrases[game.popup_phrase]) - chars_fit + 1;
            if(scroll_limit < 0) scroll_limit = 0;
            uint32_t display_time = scroll_limit > 0 ? (scroll_limit * SCROLL_SPEED + 1000) : 2000;
            if(furi_get_tick() - game.popup_timer > display_time) {
                game.state = GameState_FlipperTurn;
                flipper_moves_left = flipper_move_sequence[game.flipper_move_index];
                game.flipper_move_index = (game.flipper_move_index + 1) % FLIPPER_SEQUENCE_COUNT;
                if(flipper_moves_left > 0) lofz_flipper_move(&game);
            }
        } else if(game.state == GameState_FlipperTurn && !game.solved) {
            if(flipper_moves_left > 0) {
                if(furi_get_tick() - game.bounce_start_tick > 300) {
                    lofz_flipper_move(&game);
                    flipper_moves_left--;
                }
            } else {
                if(lofz_is_solved(&game)) {
                    game.solved = true;
                    game.state = GameState_WinScreen;
                    game.state_tick = furi_get_tick();
                } else if(is_lost_nulls(&game)) {
                    game.state = GameState_LostFade;
                    game.fade_level = 0;
                    game.state_tick = furi_get_tick();
                } else {
                    game.state = GameState_PlayerTurn;
                }
            }
        } else if(game.state == GameState_ExitScreen) {
            if(furi_get_tick() - game.state_tick > 3140) {
                game.state = GameState_Finished;
            }
        }

        InputEvent event;
        if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
            lofz_input(&event, &game);
        }
        view_port_update(viewport);
        furi_delay_ms(25);
        if(game.solved) furi_delay_ms(2200);
    }

    gui_remove_view_port(gui, viewport);
    furi_record_close(RECORD_GUI);
    view_port_free(viewport);
    furi_message_queue_free(event_queue);
    return 0;
}

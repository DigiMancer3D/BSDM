/*
LOFZ [Lights Out Flipper Zero]
------------------------------

BSDM = Bi-sectional Diffusion Model

A Star Wars-inspired, 3D-styled "Lights Out" puzzle game for Flipper Zero, where the evil Flipper Zero mascot
(Sith Lord) is vanquished by the Jedi (player) by turning out all the lights. Includes intro crawl, 
"lost nulls" fail state, special animated credits sequence, and more.

Original concept & maths: DigiMancer3D
Game design, Star Wars theme, and Flipper Zero port: DigiMancer3D & GitHub Copilot AI (https://github.com/github/copilot)

MIT License
*/

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GRID_SIZE 4
#define GRID_COUNT 16
#define FLIPPER_FACE_TOP 10
#define FLIPPER_FACE_LEFT 70

typedef enum {
    GameState_IntroCrawl,
    GameState_IntroFade,
    GameState_PlayerTurn,
    GameState_FlipperTurn,
    GameState_FlipperPopup,
    GameState_Finished,
    GameState_LostNulls,
    GameState_LostFade,
    GameState_LostAnim,
    GameState_LostMenu,
    GameState_CreditsPause,
    GameState_CreditsRoll,
    GameState_CreditsFade
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
    int lost_anim_ticks;
    bool show_lost_menu;
    int menu_sel;
    int credits_y;
    bool credits_pause;
    int secret_credits_count;
    uint32_t secret_credits_last_tick;
} LOFZGame;

// --- Flipper Mascots ---
static const uint8_t flipper_mascot[8] = {
    0b00111100,
    0b01000010,
    0b10100101,
    0b10011001,
    0b10111101,
    0b10000001,
    0b01000010,
    0b00111100,
};

static const uint8_t flipper_mascot_sith[8] = { // evil sith lord (red "eyes" if possible, else normal)
    0b00111100,
    0b01000010,
    0b10110101,
    0b10011001,
    0b10111101,
    0b10000001,
    0b01000010,
    0b00111100,
};

static const uint8_t flipper_mascot_defeated[8] = { // defeated
    0b00111100,
    0b01000010,
    0b10110101,
    0b10011001,
    0b10111101,
    0b10000001,
    0b01111010,
    0b00111100,
};

// --- Flipper AI phrases ---
static const char* flipper_phrases[] = {
    "Flipper is Playing its Move",
    "Analyzing Board...",
    "Flipper's Turn!",
    "BSDM Diffusion in Progress",
    "Flipper: Watch my move!",
    "Flipper is Thinking...",
    "Hold up, my move",
    "Wait...Oh yeah!",
    "Hmmm...",
    "That's a good one",
    "I got this",
    "Please let me move now",
    "My TURN!"
};
#define FLIPPER_PHRASE_COUNT (sizeof(flipper_phrases)/sizeof(flipper_phrases[0]))

// --- 3D Board offsets ---
static const int8_t row_offsets[GRID_SIZE] = {6, 4, 2, 0};

// --- Star Wars style intro crawl ---
static const char* intro_lines[] = {
    "LOFZ",
    "Lights Out Flipper Zero",
    "",
    "A long time ago in a galaxy",
    "far, far away....",
    "",
    "The evil Flipper Zero Sith Lord",
    "has covered the galaxy in light.",
    "You are the last Jedi,",
    "your mission is to turn all the",
    "lights out and vanquish Flipper!",
    "",
    "Use D-pad to move. OK to select.",
    "",
    "Turn all numbers to ZERO.",
    "If all tiles are lit, the Sith wins.",
    "",
    "May the Force be with you.",
    "",
    "Created by DigiMancer3D & GitHub Copilot AI",
    "https://github.com/github/copilot",
    "",
    "Press OK to begin..."
};
#define INTRO_LINES_COUNT (sizeof(intro_lines)/sizeof(intro_lines[0]))

// --- Credits crawl lines ---
static const char* credits_lines[] = {
    "LOFZ [Lights Out Flipper Zero]",
    "",
    "A Star Wars-inspired Lights Out",
    "puzzle for Flipper Zero",
    "",
    "BSDM Model, Game Design,",
    "Star Wars Theme:",
    "DigiMancer3D",
    "",
    "AI Code & Porting:",
    "GitHub Copilot AI",
    "https://github.com/github/copilot",
    "",
    "Flipper Zero Mascot by Flipper Devices",
    "",
    "Thanks for playing!",
    "",
    "May the Force be with you!"
};
#define CREDITS_LINES_COUNT (sizeof(credits_lines)/sizeof(credits_lines[0]))

// --- Helper: scrolling text (Star Wars style) ---
static void draw_crawl_text(Canvas* canvas, int y, const char* const* lines, int line_count, int fade_level) {
    for(int i=0; i<line_count; i++) {
        int y_pos = y + i*14;
        if(y_pos < -10 || y_pos > 128) continue;
        // Fade: as text moves up, make it dimmer
        int fade = fade_level;
        if(y_pos < 25) fade = 2;
        else if(y_pos < 45) fade = 1;
        else fade = 0;
        switch(fade) {
            case 0: canvas_set_color(canvas, ColorWhite); break;
            case 1: canvas_set_color(canvas, ColorLightGray); break;
            case 2: canvas_set_color(canvas, ColorGray); break;
        }
        canvas_draw_str_aligned(canvas, 64, y_pos, AlignCenter, lines[i]);
    }
    canvas_set_color(canvas, ColorBlack);
}

// --- Helper: scrolling/scrolling popup text ---
static void draw_scrolling_text(Canvas* canvas, int x, int y, const char* text, int width, LOFZGame* game, bool update_scroll) {
    int text_len = strlen(text);
    int chars_fit = width / 7;
    int scroll_limit = text_len - chars_fit;
    if(scroll_limit < 0) scroll_limit = 0;
    int scroll = game->scroll_offset;
    if(scroll > scroll_limit) scroll = 0;
    char buf[64];
    if(text_len <= chars_fit) {
        strncpy(buf, text, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        canvas_draw_str(canvas, x, y, buf);
    } else {
        strncpy(buf, text + scroll, chars_fit);
        buf[chars_fit] = 0;
        canvas_draw_str(canvas, x, y, buf);
        if(update_scroll && (furi_get_tick() - game->scroll_start_tick > 650)) {
            game->scroll_start_tick = furi_get_tick();
            game->scroll_offset++;
            if(game->scroll_offset > scroll_limit) game->scroll_offset = 0;
        }
    }
}

// --- Draw Flipper Mascot ---
static void draw_flipper(Canvas* canvas, int x, int y, int type) {
    const uint8_t* mascot = (type == 2) ? flipper_mascot_defeated : (type == 1 ? flipper_mascot_sith : flipper_mascot);
    for(int row=0; row<8; row++)
        for(int col=0; col<8; col++)
            if(mascot[row] & (1 << (7-col))) canvas_draw_dot(canvas, x+col, y+row);
}

// --- Draw 3D game board ---
static void draw_3d_board(Canvas* canvas, LOFZGame* game) {
    for(uint8_t row=0; row<GRID_SIZE; row++) {
        int y = 28 + row*18;
        int x_offset = 10 + row_offsets[row];
        for(uint8_t col=0; col<GRID_SIZE; col++) {
            int idx = row*GRID_SIZE + col;
            int x = x_offset + col*22;
            if(row == 0) canvas_set_color(canvas, ColorWhite);
            else if(row == 1) canvas_set_color(canvas, ColorLightGray);
            else if(row == 2) canvas_set_color(canvas, ColorGray);
            else canvas_set_color(canvas, ColorBlack);

            if(idx == game->flipper_cursor && game->state == GameState_FlipperTurn) {
                canvas_draw_rbox(canvas, x-2, y-2, 20, 15, 4);
                canvas_set_color(canvas, ColorWhite);
            } else if(idx == game->player_cursor && game->state == GameState_PlayerTurn) {
                canvas_draw_box(canvas, x-2, y-2, 20, 15);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_draw_frame(canvas, x-2, y-2, 20, 15);
            }
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", game->grid[idx]);
            canvas_draw_str(canvas, x+2, y+11, buf);
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

// --- Check for "lost nulls" state (no zeros on grid, all >=1 or <=-1) ---
static bool is_lost_nulls(LOFZGame* game) {
    for(uint8_t i=0; i<GRID_COUNT; i++)
        if(game->grid[i] == 0) return false;
    return true;
}

// --- Main draw function ---
static void lofz_draw(Canvas* canvas, LOFZGame* game) {
    canvas_clear(canvas);
    if(game->state == GameState_IntroCrawl) {
        // 3D crawl
        draw_crawl_text(canvas, game->intro_crawl_y, intro_lines, INTRO_LINES_COUNT, 0);
        // Evil Flipper Sith
        draw_flipper(canvas, FLIPPER_FACE_LEFT-10, 22, 1);
    } else if(game->state == GameState_IntroFade || game->state == GameState_LostFade || game->state == GameState_CreditsFade) {
        // Fade to black
        draw_3d_board(canvas, game);
        int lvl = game->fade_level;
        for(int i=0; i<128; i+=2)
            canvas_draw_box(canvas, 0, 0, 128, lvl);
    } else if(game->state == GameState_CreditsRoll) {
        draw_crawl_text(canvas, game->credits_y, credits_lines, CREDITS_LINES_COUNT, 0);
    } else if(game->state == GameState_CreditsPause) {
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, "Credits Paused");
        canvas_draw_str_aligned(canvas, 64, 90, AlignCenter, "Press OK to resume");
    } else if(game->state == GameState_LostNulls) {
        // All tiles are ON, lose screen
        draw_3d_board(canvas, game);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_box(canvas, 13, 95, 102, 22);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 20, 110, "You Died - Flipper Wins!");
        canvas_set_color(canvas, ColorBlack);
    } else if(game->state == GameState_LostAnim) {
        // Animate defeated Flipper bouncing
        draw_3d_board(canvas, game);
        int anim_y = FLIPPER_FACE_TOP + (game->lost_anim_ticks % 2 ? 6 : 0);
        draw_flipper(canvas, FLIPPER_FACE_LEFT+10, anim_y, 1);
    } else if(game->state == GameState_LostMenu) {
        // Show lost menu with Flipper defeated on left, options on right
        draw_3d_board(canvas, game);
        draw_flipper(canvas, FLIPPER_FACE_LEFT+10, FLIPPER_FACE_TOP+28, 1);
        canvas_draw_box(canvas, 85, 95, 38, 25);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 88, 108, game->menu_sel == 0 ? ">Yes" : " Yes");
        canvas_draw_str(canvas, 88, 118, game->menu_sel == 1 ? ">No " : " No ");
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 15, 118, "Play again?");
    } else if(game->state == GameState_FlipperPopup) {
        // Flipper popup
        for(uint8_t i=0; i<3; i++)
            draw_3d_board(canvas, game);
        draw_flipper(canvas, FLIPPER_FACE_LEFT, FLIPPER_FACE_TOP, 1);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_box(canvas, 19, 55, 90, 28);
        canvas_set_color(canvas, ColorWhite);
        draw_scrolling_text(canvas, 24, 70, flipper_phrases[game->popup_phrase], 90, game, true);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 75, 18, "Flipper");
    } else {
        // Main game
        draw_3d_board(canvas, game);
        draw_flipper(canvas, FLIPPER_FACE_LEFT, FLIPPER_FACE_TOP, 1);
        canvas_draw_str(canvas, 75, 18, "Flipper");
        canvas_draw_str(canvas, 75, 112, "You");
        if(game->solved) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_box(canvas, 13, 92, 116, 22);
            canvas_set_color(canvas, ColorWhite);
            draw_scrolling_text(canvas, 19, 108, "You got me, I bet you can't do it again", 110, game, true);
            canvas_set_color(canvas, ColorBlack);
            draw_flipper(canvas, 123, 92, 2);
        }
    }
    canvas_draw_str(canvas, 2, 124, "LOFZ - Copilot AI & DigiMancer3D");
}

// --- Game logic functions ---
static int sign_flip(int v) { return (v == 0) ? 0 : -v; }

static void lofz_update(LOFZGame* game, uint8_t idx) {
    static const int8_t dx[4] = {1, -1, 0, 0};
    static const int8_t dy[4] = {0, 0, 1, -1};
    uint8_t x = idx % GRID_SIZE, y = idx / GRID_SIZE;
    game->grid[idx] = sign_flip(game->grid[idx]);
    for(int n=0; n<4; n++) {
        int nx = x + dx[n], ny = y + dy[n];
        if(nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE)
            game->grid[ny*GRID_SIZE + nx] = sign_flip(game->grid[ny*GRID_SIZE + nx]);
    }
}

// Flipper AI: choose nonzero cell closest to player
static void lofz_flipper_move(LOFZGame* game) {
    uint8_t best_idx = 0; int min_dist = 999;
    for(uint8_t i=0; i<GRID_COUNT; i++) {
        if(game->grid[i] != 0) {
            int px = game->player_cursor % GRID_SIZE, py = game->player_cursor / GRID_SIZE;
            int fx = i % GRID_SIZE, fy = i / GRID_SIZE;
            int dist = abs(px-fx) + abs(py-fy);
            if(dist < min_dist) { best_idx = i; min_dist = dist; }
        }
    }
    game->flipper_cursor = best_idx;
    lofz_update(game, best_idx);
}

static bool lofz_is_solved(LOFZGame* game) {
    for(uint8_t i=0; i<GRID_COUNT; i++)
        if(game->grid[i] != 0) return false;
    return true;
}

// --- Input handler, includes secret credits sequence ---
static void lofz_input(InputEvent* event, void* ctx) {
    LOFZGame* game = (LOFZGame*)ctx;
    if(event->type == InputTypePress) {
        // --- Secret credits Easter egg ---
        if(game->state == GameState_PlayerTurn && game->player_cursor == 3 && event->key == InputKeyRight) {
            uint32_t now = furi_get_tick();
            if(now - game->secret_credits_last_tick < 600) game->secret_credits_count++;
            else game->secret_credits_count = 1;
            game->secret_credits_last_tick = now;
            if(game->secret_credits_count >= 3) {
                game->state = GameState_CreditsFade;
                game->fade_level = 0;
                game->credits_y = 128;
                game->credits_pause = false;
                game->secret_credits_count = 0;
                return;
            }
        } else {
            game->secret_credits_count = 0;
        }

        if(game->state == GameState_IntroCrawl) {
            if(event->key == InputKeyOk) {
                game->state = GameState_IntroFade;
                game->fade_level = 0;
                game->state_tick = furi_get_tick();
            }
        } else if(game->state == GameState_CreditsPause) {
            if(event->key == InputKeyOk) {
                game->state = GameState_CreditsRoll;
            }
        } else if(game->state == GameState_LostMenu) {
            if(event->key == InputKeyUp || event->key == InputKeyDown) {
                game->menu_sel = 1 - game->menu_sel;
            } else if(event->key == InputKeyOk) {
                if(game->menu_sel == 0) { // Yes, restart
                    memset(game->grid, 0, sizeof(game->grid));
                    game->grid[0] = 1; game->player_cursor = 12; game->flipper_cursor = 1;
                    game->solved = false; game->state = GameState_PlayerTurn;
                } else { // No, exit
                    furi_exit();
                }
            }
        } else if(game->state == GameState_PlayerTurn && !game->solved) {
            switch(event->key) {
                case InputKeyUp:
                    if(game->player_cursor >= GRID_SIZE) game->player_cursor -= GRID_SIZE;
                    break;
                case InputKeyDown:
                    if(game->player_cursor < GRID_COUNT-GRID_SIZE) game->player_cursor += GRID_SIZE;
                    break;
                case InputKeyLeft:
                    if(game->player_cursor % GRID_SIZE) game->player_cursor--;
                    break;
                case InputKeyRight:
                    if((game->player_cursor+1)%GRID_SIZE) game->player_cursor++;
                    break;
                case InputKeyOk:
                    lofz_update(game, game->player_cursor);
                    if(lofz_is_solved(game)) {
                        game->solved = true;
                    } else if(is_lost_nulls(game)) {
                        game->state = GameState_LostFade;
                        game->fade_level = 0;
                        game->state_tick = furi_get_tick();
                    } else {
                        game->state = GameState_FlipperPopup;
                        game->popup_timer = furi_get_tick();
                        game->popup_phrase = rand()%FLIPPER_PHRASE_COUNT;
                        game->scroll_offset = 0;
                        game->scroll_start_tick = furi_get_tick();
                    }
                    break;
                default: break;
            }
        }
    }
}

// --- Main app ---
int32_t lofz_app(void* p) {
    LOFZGame game;
    memset(&game, 0, sizeof(game));
    memset(game.grid, 0, sizeof(game.grid));
    game.grid[0] = 1;
    game.player_cursor = 12;
    game.flipper_cursor = 1;
    game.menu_sel = 0;
    game.state = GameState_IntroCrawl;
    game.intro_crawl_y = 128;
    game.state_tick = furi_get_tick();
    game.credits_y = 128;

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* viewport = viewport_alloc();
    viewport_draw_callback_set(viewport, (ViewportDrawCallback)lofz_draw, &game);
    viewport_input_callback_set(viewport, lofz_input, &game);

    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    Input* input = furi_record_open("input");
    input_set_callback(input, viewport, event_queue);

    while(1) {
        if(game.state == GameState_IntroCrawl) {
            if(game.intro_crawl_y > -INTRO_LINES_COUNT*14) {
                if(furi_get_tick() - game.state_tick > 40) {
                    game.intro_crawl_y -= 1;
                    game.state_tick = furi_get_tick();
                }
            }
        } else if(game.state == GameState_IntroFade) {
            if(game.fade_level < 128) game.fade_level += 8;
            else {
                game.state = GameState_PlayerTurn;
                game.fade_level = 0;
                game.intro_crawl_y = 128;
            }
        } else if(game.state == GameState_CreditsFade) {
            if(game.fade_level < 128) game.fade_level += 8;
            else {
                game.state = GameState_CreditsRoll;
                game.credits_y = 128;
            }
        } else if(game.state == GameState_CreditsRoll) {
            if(game.credits_y > -CREDITS_LINES_COUNT*14) {
                if(furi_get_tick() - game.state_tick > 60) {
                    game.credits_y -= 1;
                    game.state_tick = furi_get_tick();
                }
            } else {
                game.state = GameState_PlayerTurn;
                game.credits_y = 128;
            }
        } else if(game.state == GameState_LostFade) {
            if(game.fade_level < 128) game.fade_level += 8;
            else {
                game.state = GameState_LostAnim;
                game.lost_anim_ticks = 0;
                game.state_tick = furi_get_tick();
            }
        } else if(game.state == GameState_LostAnim) {
            if(game.lost_anim_ticks < 12) {
                if(furi_get_tick() - game.state_tick > 120) {
                    game.lost_anim_ticks++;
                    game.state_tick = furi_get_tick();
                }
            } else {
                game.state = GameState_LostMenu;
                game.menu_sel = 0;
            }
        } else if(game.state == GameState_FlipperPopup && !game.solved) {
            if(furi_get_tick() - game.popup_timer > 1050) {
                game.state = GameState_FlipperTurn;
                lofz_flipper_move(&game);
                if(lofz_is_solved(&game)) game.solved = true;
                else if(is_lost_nulls(&game)) {
                    game.state = GameState_LostFade;
                    game.fade_level = 0;
                    game.state_tick = furi_get_tick();
                } else {
                    game.state = GameState_PlayerTurn;
                }
            }
        }
        // Special phrase after credits
        if(game.state == GameState_PlayerTurn && game.secret_credits_count == 0 && game.secret_credits_last_tick) {
            static uint32_t after_credits_tick = 0;
            if(after_credits_tick == 0) after_credits_tick = furi_get_tick();
            if(furi_get_tick() - after_credits_tick < 3500) {
                canvas_clear(viewport->canvas);
                canvas_draw_str_aligned(viewport->canvas, 64, 65, AlignCenter, "That was your turn? Uffda much...");
                viewport_update(viewport);
                furi_delay_ms(30);
                continue;
            } else {
                after_credits_tick = 0;
                game.secret_credits_last_tick = 0;
            }
        }
        InputEvent event;
        if(furi_message_queue_get(event_queue, &event, 50) == FuriStatusOk) {
            viewport_input_callback_call(viewport, &event);
        }
        viewport_update(viewport);
        furi_delay_ms(25);
        if(game.solved) furi_delay_ms(2200);
    }

    input_set_callback(input, NULL, NULL);
    furi_record_close("input");
    gui_remove_view_port(gui, viewport);
    furi_record_close("gui");
    viewport_free(viewport);
    furi_message_queue_free(event_queue);
    return 0;
}

/*
LOFZ [Lights Out Flipper Zero]
Game, Code, and Star Wars Theme: DigiMancer3D & GitHub Copilot AI
https://github.com/github/copilot
*/

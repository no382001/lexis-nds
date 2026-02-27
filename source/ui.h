#pragma once

#include <nds.h>

#include "common.h"
#include "drawing.h"
#include "keyboard.h"
#include "notes.h"
#include "reader.h"
#include "text_render.h"


enum {
  MAX_PAGE_LINES = 40,
  MAX_WORD_LEN = 64,
  NUM_ZOOM_LEVELS = 5,
  NUM_FONT_FAMILIES = 3,
  MAX_BOOKS = 24, /* TODO: derive from compiled DB */

  NUM_PRESET_PALETTES = 3,
  NUM_CUSTOM_PALETTES = 2,
  NUM_PALETTES = NUM_PRESET_PALETTES + NUM_CUSTOM_PALETTES,

  MAX_RESULT_LINES = 60,
  MAX_RESULT_LEN = 128,

  BAR_TIMEOUT_FRAMES = 180, /* ~3 seconds at 60 fps */
  LINE_FETCH_EXTRA = 10,    /* extra lines to fetch beyond page */
};

typedef struct {
  uint16_t bg;
  uint16_t text;
  uint16_t num;
  uint16_t hl;
} palette_t;

typedef enum {
  ST_READ,
  ST_LOOKUP,
  ST_BAR,
  ST_SETTINGS,
  ST_PICKER,
  ST_KB_GOTO,
  ST_KB_LATIN,
  ST_DRAW,
} app_state_t;

typedef app_state_t (*key_handler_t)(app_state_t state);

typedef struct {
  u32 key;
  key_handler_t handler;
} keybind_t;

typedef struct {
  char text[MAX_RESULT_LEN];
  uint16_t color;
  int indent;
} result_line;

extern const int g_zoom_sizes[NUM_ZOOM_LEVELS];
extern const char *g_font_family_names[NUM_FONT_FAMILIES];
extern tr_font *g_all_fonts[NUM_FONT_FAMILIES][NUM_ZOOM_LEVELS];
extern tr_font *g_fonts[NUM_ZOOM_LEVELS];
extern tr_font *g_font;
extern int g_zoom_level;
extern int g_font_family;

extern reader_ctx *g_ctx;
extern int g_num_books;
extern int g_book;
extern int g_line_num;
extern int g_page_lines;
extern int g_fullscreen;
extern int g_fat_ok;

extern int g_palette_idx;
extern palette_t g_custom_palettes[NUM_CUSTOM_PALETTES];
extern int16_t g_book_lines[MAX_BOOKS];

extern const palette_t g_preset_palettes[NUM_PRESET_PALETTES];
extern const char *g_palette_names[NUM_PALETTES];

extern result_line g_result_buf[MAX_RESULT_LINES];
extern int g_result_count;
extern int g_result_scroll;
extern char g_result_title[MAX_WORD_LEN];

extern int g_set_book, g_set_line, g_set_cursor, g_set_tab;
extern int g_pick_custom, g_pick_field, g_pick_slider;
extern int g_bar_timer;
extern int g_settings_btn_x, g_settings_btn_y, g_settings_btn_w,
    g_settings_btn_h;


const palette_t *active_palette(void);
uint16_t pal_ui_bg(const palette_t *p);
uint16_t pal_btn_bg(const palette_t *p);

uint16_t pick_compose(int r, int g, int b);
void pick_decompose(uint16_t c, int *r, int *g, int *b);

void save_state(void);
int load_state(void);

int lines_per_screen(void);
void recompute_page_lines(void);
void show_text(void);


int touch_to_word(int tx, int ty, char *out_word, int out_len);


void result_push(const char *text, uint16_t color, int indent);
void build_lookup_result(const char *word, int dict_mode);
void draw_lookup_result(void);

app_state_t on_lookup_TOUCH(app_state_t s);
app_state_t on_lookup_B(app_state_t s);
app_state_t on_lookup_DOWN(app_state_t s);
app_state_t on_lookup_UP(app_state_t s);
app_state_t on_lookup_Y(app_state_t s);


void draw_settings(void);
void draw_bar(void);
void draw_picker(void);
void preview_top(void);

app_state_t bar_dismiss(void);
void settings_clamp(void);
app_state_t settings_go(void);

app_state_t on_bar_L(app_state_t s);
app_state_t on_bar_R(app_state_t s);
app_state_t on_bar_A(app_state_t s);
app_state_t on_bar_SELECT(app_state_t s);
app_state_t on_bar_any(app_state_t s);
app_state_t on_bar_TOUCH(app_state_t s);

app_state_t on_settings_TOUCH(app_state_t s);
app_state_t on_settings_A(app_state_t s);
app_state_t on_settings_X(app_state_t s);
app_state_t on_settings_dismiss(app_state_t s);
app_state_t on_settings_UP(app_state_t s);
app_state_t on_settings_DOWN(app_state_t s);
app_state_t on_settings_RIGHT(app_state_t s);
app_state_t on_settings_LEFT(app_state_t s);
app_state_t on_settings_R(app_state_t s);
app_state_t on_settings_L(app_state_t s);

app_state_t on_picker_TOUCH(app_state_t s);
app_state_t on_picker_A(app_state_t s);
app_state_t on_picker_B(app_state_t s);
app_state_t on_picker_UP(app_state_t s);
app_state_t on_picker_DOWN(app_state_t s);
app_state_t on_picker_RIGHT(app_state_t s);
app_state_t on_picker_LEFT(app_state_t s);
app_state_t on_picker_R(app_state_t s);
app_state_t on_picker_L(app_state_t s);


app_state_t on_kb_TOUCH(app_state_t s);
app_state_t on_kb_key(app_state_t s);
app_state_t on_kb_A(app_state_t s);

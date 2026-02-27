#include <fat.h>
#include <filesystem.h>
#include <nds.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"

static void vblank_handler(void) {}

const int g_zoom_sizes[NUM_ZOOM_LEVELS] = {8, 10, 12, 14, 16};
const char *g_font_family_names[NUM_FONT_FAMILIES] = {"Gentium", "DejaVu",
                                                      "Cardo"};

tr_font *g_all_fonts[NUM_FONT_FAMILIES][NUM_ZOOM_LEVELS];
tr_font *g_fonts[NUM_ZOOM_LEVELS];
tr_font *g_font;
int g_zoom_level = 1;
int g_font_family = 0;

reader_ctx *g_ctx;
int g_num_books;
int g_book = 1;
int g_line_num = 1;
int g_page_lines = 12;
int g_fullscreen = 1;
int g_fat_ok;

int g_palette_idx = 0;
palette_t g_custom_palettes[NUM_CUSTOM_PALETTES] = {
    {0x8000, 0xFFFF, 0xAD6B, 0xFBE0},
    {0xFFFF, 0x8000, 0x94A5, 0x801F},
};

int16_t g_book_lines[MAX_BOOKS];

result_line g_result_buf[MAX_RESULT_LINES];
int g_result_count;
int g_result_scroll;
char g_result_title[MAX_WORD_LEN];

int g_set_book, g_set_line;
int g_set_cursor;
int g_set_tab;
int g_pick_custom;
int g_pick_field;
int g_pick_slider;
int g_bar_timer;
int g_settings_btn_x, g_settings_btn_y, g_settings_btn_w, g_settings_btn_h;


static reader_line bot_lines[MAX_PAGE_LINES];
static int bot_line_count;
static int bot_header_h;
static int bot_line_y[MAX_PAGE_LINES];
static int bot_line_rows[MAX_PAGE_LINES];
static int bot_text_x[MAX_PAGE_LINES];


int lines_per_screen(void) {
  int line_h = g_font->glyph_h + 1;
  int header_h = line_h + 2;
  int n = (TR_SCREEN_H - header_h) / line_h;
  if (n > MAX_PAGE_LINES)
    n = MAX_PAGE_LINES;
  if (n < 1)
    n = 1;
  return n;
}

void recompute_page_lines(void) {
  g_page_lines = lines_per_screen();
  if (g_page_lines > MAX_PAGE_LINES)
    g_page_lines = MAX_PAGE_LINES;
}


static int render_lines(const reader_line *lines, int count, int first_line,
                        int total_max, int show_header) {
  int line_h = g_font->glyph_h + 1;
  int header_h = line_h + 2;
  int y = 0;
  int y_max = TR_SCREEN_H;

  if (show_header) {
    char hdr[80];
    snprintf(hdr, sizeof(hdr), "Book %d/%d  [%d-%d / %d]  %dpx%s", g_book,
             g_num_books, first_line, first_line + count - 1, total_max,
             g_zoom_sizes[g_zoom_level], g_fullscreen ? "  [FS]" : "");
    tr_draw_text(g_font, 2, 1, hdr, active_palette()->hl);
    tr_draw_hline(0, header_h - 1, TR_SCREEN_W, active_palette()->num);
    y = header_h;
  }

  int rendered = 0;
  for (int i = 0; i < count; i++) {
    if (y + line_h > y_max)
      break;

    char num[8];
    snprintf(num, sizeof(num), "%3d ", lines[i].line);
    int num_w = tr_text_width(g_font, num);
    int text_x = 2 + num_w;

    tr_draw_text(g_font, 2, y, num, active_palette()->num);
    int rows = tr_draw_text_wrap(g_font, text_x, text_x, y, TR_SCREEN_W - 2,
                                 lines[i].text, active_palette()->text);
    y += rows * line_h;
    rendered++;
  }

  if (rendered == 0) {
    tr_draw_text(g_font, 8, y + 4, "(no lines)", active_palette()->num);
  }
  return rendered;
}


static void show_bottom_info(void) {
  const palette_t *p = active_palette();
  const tr_font *big = g_fonts[NUM_ZOOM_LEVELS - 1];
  tr_select(TR_SCREEN_BOTTOM);
  tr_clear(p->bg);

  int line_h = big->glyph_h + 1;
  int y = 4;

  tr_draw_text(big, 4, y, "Reader v0.1", p->hl);
  y += line_h;
  tr_draw_text(big, 4, y, "Homer, Iliad", p->num);
  y += line_h * 2;

  tr_draw_hline(0, y - 2, TR_SCREEN_W, p->num);

  char line[80], tmp[16];
  int note_sz = notes_data_size();
  int draw_sz = draw_data_size();

  snprintf(line, sizeof(line), "Notes:    %d  (%s)", notes_get_count(),
           fmt_bytes(note_sz, tmp, sizeof(tmp)));
  tr_draw_text(big, 4, y, line, p->text);
  y += line_h;

  snprintf(line, sizeof(line), "Drawings: %d strokes  (%s)",
           g_draw_stroke_count, fmt_bytes(draw_sz, tmp, sizeof(tmp)));
  tr_draw_text(big, 4, y, line, p->text);
  y += line_h;

  snprintf(line, sizeof(line), "Points:   %d / %d", g_draw_pool_used,
           DRAW_POOL_SIZE);
  tr_draw_text(big, 4, y, line, p->text);
  y += line_h * 2;

  tr_draw_hline(0, y - 2, TR_SCREEN_W, p->num);

  tr_draw_text(big, 4, y, "Touch     Tap word to look up", p->hl);
  y += line_h;
  tr_draw_text(big, 4, y, "Y         Goto (book.line)", p->text);
  y += line_h;
  tr_draw_text(big, 4, y, "X         Draw mode", p->text);
  y += line_h;
  tr_draw_text(big, 4, y, "SELECT    Settings", p->text);
  y += line_h;
  tr_draw_text(big, 4, y, "START     Quit", p->text);

  tr_select(TR_SCREEN_TOP);
}

void show_text(void) {
  int maxl = reader_max_line(g_ctx, "iliad", g_book);

  static reader_line lines[MAX_PAGE_LINES];
  int fetch = g_page_lines + LINE_FETCH_EXTRA;
  if (fetch > MAX_PAGE_LINES)
    fetch = MAX_PAGE_LINES;
  int n = reader_get_lines(g_ctx, "iliad", g_book, g_line_num, fetch, lines);
  if (n > MAX_PAGE_LINES)
    n = MAX_PAGE_LINES;

  if (g_fullscreen) {
    tr_select(TR_SCREEN_BOTTOM);
    tr_clear(active_palette()->bg);

    int line_h = g_font->glyph_h + 1;
    int y = 0;
    int bot_rendered = 0;
    bot_header_h = 0;
    for (int i = 0; i < n; i++) {
      if (y + line_h > TR_SCREEN_H)
        break;
      char num[8];
      snprintf(num, sizeof(num), "%3d ", lines[i].line);
      int num_w = tr_text_width(g_font, num);
      int text_x = 2 + num_w;

      bot_lines[bot_rendered] = lines[i];
      bot_line_y[bot_rendered] = y;
      bot_text_x[bot_rendered] = text_x;

      tr_draw_text(g_font, 2, y, num, active_palette()->num);
      int rows = tr_draw_text_wrap(g_font, text_x, text_x, y, TR_SCREEN_W - 2,
                                   lines[i].text, active_palette()->text);
      bot_line_rows[bot_rendered] = rows;
      y += rows * line_h;
      bot_rendered++;
    }
    bot_line_count = bot_rendered;

    draw_line_map_t bot_map[MAX_PAGE_LINES];
    for (int i = 0; i < bot_rendered; i++) {
      bot_map[i].line = (int16_t)bot_lines[i].line;
      bot_map[i].y = (int16_t)bot_line_y[i];
    }

    g_draw_cur_map_count = bot_rendered;
    for (int i = 0; i < bot_rendered; i++)
      g_draw_cur_map[i] = bot_map[i];

    draw_render_overlay(g_book, g_zoom_level, bot_map, bot_rendered,
                        active_palette()->hl);

    tr_select(TR_SCREEN_TOP);
    tr_clear(active_palette()->bg);

    if (g_line_num > 1) {
      int per = lines_per_screen();
      int ctx_start = g_line_num - per * 2;
      if (ctx_start < 1)
        ctx_start = 1;
      int ctx_count = g_line_num - ctx_start;

      static reader_line ctx_lines[MAX_PAGE_LINES];
      int cn = reader_get_lines(g_ctx, "iliad", g_book, ctx_start, ctx_count,
                                ctx_lines);

      int ctx_line_h = g_font->glyph_h + 1;
      int row_counts[MAX_PAGE_LINES];
      for (int i = 0; i < cn; i++) {
        char num[8];
        snprintf(num, sizeof(num), "%3d ", ctx_lines[i].line);
        int num_w = tr_text_width(g_font, num);
        int text_x = 2 + num_w;
        row_counts[i] = tr_count_wrapped_lines(
            g_font, text_x, text_x, TR_SCREEN_W - 2, ctx_lines[i].text);
        if (row_counts[i] < 1)
          row_counts[i] = 1;
      }

      int first = cn;
      int rows_fit = 0;
      for (int i = cn - 1; i >= 0; i--) {
        if (rows_fit + row_counts[i] > TR_SCREEN_H / ctx_line_h)
          break;
        rows_fit += row_counts[i];
        first = i;
      }

      int ctx_y_start = TR_SCREEN_H - rows_fit * ctx_line_h;
      int ctx_y = ctx_y_start;
      for (int i = first; i < cn; i++) {
        char num[8];
        snprintf(num, sizeof(num), "%3d ", ctx_lines[i].line);
        int num_w = tr_text_width(g_font, num);
        int text_x = 2 + num_w;

        tr_draw_text(g_font, 2, ctx_y, num, active_palette()->num);
        tr_draw_text_wrap(g_font, text_x, text_x, ctx_y, TR_SCREEN_W - 2,
                          ctx_lines[i].text, active_palette()->text);
        ctx_y += row_counts[i] * ctx_line_h;
      }

      draw_line_map_t top_map[MAX_PAGE_LINES];
      int top_map_count = 0;
      {
        int ty = ctx_y_start;
        for (int i = first; i < cn; i++) {
          top_map[top_map_count].line = (int16_t)ctx_lines[i].line;
          top_map[top_map_count].y = (int16_t)ty;
          top_map_count++;
          ty += row_counts[i] * ctx_line_h;
        }
      }

      draw_render_overlay(g_book, g_zoom_level, top_map, top_map_count,
                          active_palette()->hl);
    }
  } else {
    tr_select(TR_SCREEN_TOP);
    tr_clear(active_palette()->bg);
    int top_rendered = render_lines(lines, n, g_line_num, maxl, 1);

    bot_line_count = top_rendered;
    bot_header_h = g_font->glyph_h + 3;
    for (int i = 0; i < top_rendered; i++)
      bot_lines[i] = lines[i];

    show_bottom_info();
  }

  tr_flip();
}
int touch_to_word(int tx, int ty, char *out_word, int out_len) {
  if (!g_fullscreen)
    return 0;
  if (bot_line_count == 0)
    return 0;

  int line_idx = -1;
  for (int i = 0; i < bot_line_count; i++) {
    int y_top = bot_line_y[i];
    int y_bot = y_top + bot_line_rows[i] * (g_font->glyph_h + 1);
    if (ty >= y_top && ty < y_bot) {
      line_idx = i;
      break;
    }
  }
  if (line_idx < 0)
    return 0;

  int found = tr_word_at_pos(
      g_font, bot_text_x[line_idx], bot_text_x[line_idx], bot_line_y[line_idx],
      TR_SCREEN_W - 2, bot_lines[line_idx].text, tx, ty, out_word, out_len);
  return found;
}


static app_state_t on_read_TOUCH(app_state_t s) {
  touchPosition touch;
  touchRead(&touch);
  char tapped_word[MAX_WORD_LEN];
  if (touch_to_word(touch.px, touch.py, tapped_word, MAX_WORD_LEN)) {
    build_lookup_result(tapped_word, 0);
    draw_lookup_result();
    return ST_LOOKUP;
  }
  return s;
}

static app_state_t on_read_SELECT(app_state_t s) {
  (void)s;
  g_bar_timer = BAR_TIMEOUT_FRAMES;
  draw_bar();
  return ST_BAR;
}

static app_state_t on_read_DOWN(app_state_t s) {
  int maxl = reader_max_line(g_ctx, "iliad", g_book);
  if (g_line_num < maxl) {
    g_line_num++;
    show_text();
  }
  return s;
}

static app_state_t on_read_UP(app_state_t s) {
  if (g_line_num > 1) {
    g_line_num--;
    show_text();
  }
  return s;
}

static app_state_t on_read_RIGHT(app_state_t s) {
  if (g_book < g_num_books) {
    if (g_book >= 1 && g_book <= MAX_BOOKS)
      g_book_lines[g_book - 1] = (int16_t)g_line_num;
    g_book++;
    g_line_num = (g_book <= MAX_BOOKS && g_book_lines[g_book - 1] > 0)
                     ? g_book_lines[g_book - 1]
                     : 1;
    show_text();
  }
  return s;
}

static app_state_t on_read_LEFT(app_state_t s) {
  if (g_book > 1) {
    if (g_book >= 1 && g_book <= MAX_BOOKS)
      g_book_lines[g_book - 1] = (int16_t)g_line_num;
    g_book--;
    g_line_num = (g_book >= 1 && g_book_lines[g_book - 1] > 0)
                     ? g_book_lines[g_book - 1]
                     : 1;
    show_text();
  }
  return s;
}

static app_state_t on_read_R(app_state_t s) {
  if (g_zoom_level < NUM_ZOOM_LEVELS - 1) {
    g_zoom_level++;
    g_font = g_fonts[g_zoom_level];
    recompute_page_lines();
    show_text();
  }
  return s;
}

static app_state_t on_read_L(app_state_t s) {
  if (g_zoom_level > 0) {
    g_zoom_level--;
    g_font = g_fonts[g_zoom_level];
    recompute_page_lines();
    show_text();
  }
  return s;
}

static app_state_t on_read_Y(app_state_t s) {
  (void)s;
  kb_open(KB_MODE_GOTO);
  kb_draw();
  return ST_KB_GOTO;
}

static app_state_t on_read_X(app_state_t s) {
  (void)s;
  draw_show_indicator();
  return ST_DRAW;
}


static app_state_t on_draw_exit(app_state_t s) {
  (void)s;
  draw_save();
  show_text();
  return ST_READ;
}

static app_state_t on_draw_clear(app_state_t s) {
  draw_clear_view(g_book, g_line_num, g_zoom_level);
  show_text();
  draw_show_indicator();
  return s;
}


static const keybind_t lookup_keys[] = {
    {KEY_TOUCH, on_lookup_TOUCH}, {KEY_B, on_lookup_B},
    {KEY_DOWN, on_lookup_DOWN},   {KEY_UP, on_lookup_UP},
    {KEY_Y, on_lookup_Y},
};

static const keybind_t read_keys[] = {
    {KEY_TOUCH, on_read_TOUCH}, {KEY_SELECT, on_read_SELECT},
    {KEY_DOWN, on_read_DOWN},   {KEY_UP, on_read_UP},
    {KEY_RIGHT, on_read_RIGHT}, {KEY_LEFT, on_read_LEFT},
    {KEY_R, on_read_R},         {KEY_L, on_read_L},
    {KEY_Y, on_read_Y},         {KEY_X, on_read_X},
};

static const keybind_t bar_keys[] = {
    {KEY_L, on_bar_L},       {KEY_R, on_bar_R},
    {KEY_A, on_bar_A},       {KEY_SELECT, on_bar_SELECT},
    {KEY_B, on_bar_any},     {KEY_UP, on_bar_any},
    {KEY_DOWN, on_bar_any},  {KEY_LEFT, on_bar_any},
    {KEY_RIGHT, on_bar_any}, {KEY_X, on_bar_any},
    {KEY_Y, on_bar_any},     {KEY_TOUCH, on_bar_TOUCH},
};

static const keybind_t settings_keys[] = {
    {KEY_TOUCH, on_settings_TOUCH}, {KEY_A, on_settings_A},
    {KEY_X, on_settings_X},         {KEY_B, on_settings_dismiss},
    {KEY_UP, on_settings_UP},       {KEY_DOWN, on_settings_DOWN},
    {KEY_RIGHT, on_settings_RIGHT}, {KEY_LEFT, on_settings_LEFT},
    {KEY_R, on_settings_R},         {KEY_L, on_settings_L},
};

static const keybind_t picker_keys[] = {
    {KEY_TOUCH, on_picker_TOUCH}, {KEY_A, on_picker_A},
    {KEY_B, on_picker_B},         {KEY_UP, on_picker_UP},
    {KEY_DOWN, on_picker_DOWN},   {KEY_RIGHT, on_picker_RIGHT},
    {KEY_LEFT, on_picker_LEFT},   {KEY_R, on_picker_R},
    {KEY_L, on_picker_L},
};

static const keybind_t kb_keys[] = {
    {KEY_TOUCH, on_kb_TOUCH},
    {KEY_B, on_kb_key},
    {KEY_A, on_kb_A},
};

static const keybind_t draw_keys[] = {
    {KEY_X, on_draw_exit},
    {KEY_B, on_draw_exit},
    {KEY_Y, on_draw_clear},
};

static const struct {
  const keybind_t *binds;
  int count;
} dispatch[] = {
    [ST_READ] = {read_keys, countof(read_keys)},
    [ST_LOOKUP] = {lookup_keys, countof(lookup_keys)},
    [ST_BAR] = {bar_keys, countof(bar_keys)},
    [ST_SETTINGS] = {settings_keys, countof(settings_keys)},
    [ST_PICKER] = {picker_keys, countof(picker_keys)},
    [ST_KB_GOTO] = {kb_keys, countof(kb_keys)},
    [ST_KB_LATIN] = {kb_keys, countof(kb_keys)},
    [ST_DRAW] = {draw_keys, countof(draw_keys)},
};

int main(void) {

  defaultExceptionHandler();

  irqSet(IRQ_VBLANK, vblank_handler);
  irqEnable(IRQ_VBLANK);

  lcdMainOnTop();

  videoSetModeSub(MODE_0_2D);
  vramSetBankC(VRAM_C_SUB_BG);
  PrintConsole bootConsole;
  consoleInit(&bootConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false,
              true);

  printf("Reader v0.1\n");
  printf("Booting...\n\n");

  printf("[1] NitroFS init...\n");
  if (!nitroFSInit(nil)) {
    printf("\x1b[31mNitroFS init failed!\x1b[0m\n");
    printf("errno=%d\n", errno);
    while (1)
      swiWaitForVBlank();
  }
  printf("[1] NitroFS OK\n");

  g_fat_ok = fatInitDefault();
  printf("[2] FAT %s\n", g_fat_ok ? "OK" : "unavailable (no save)");

  printf("[3] Opening DB...\n");
  g_ctx = reader_open("nitro:/lexis.dat");

  if (!g_ctx) {
    printf("\x1b[31mCannot open DB!\x1b[0m\n");
    printf("nitro:/lexis.dat not found.\n");
    while (1)
      swiWaitForVBlank();
  }

  g_num_books = reader_book_count(g_ctx, "iliad");
  printf("[4] Iliad: %d books\n", g_num_books);

  if (g_fat_ok)
    notes_load();

  if (g_fat_ok)
    draw_load();

  if (load_state())
    printf("    Save loaded (g_book %d, line %d)\n", g_book, g_line_num);
  else
    printf("    No save file\n");

  printf("[5] Loading fonts...\n");
  for (int fam = 0; fam < NUM_FONT_FAMILIES; fam++) {
    printf("  Family %d: %s\n", fam, g_font_family_names[fam]);
    for (int i = 0; i < NUM_ZOOM_LEVELS; i++) {
      char path[48];
      snprintf(path, sizeof(path), "nitro:/font_%d_%d.bin", fam,
               g_zoom_sizes[i]);
      g_all_fonts[fam][i] = tr_load_font(path);
      if (!g_all_fonts[fam][i]) {
        printf("\x1b[31mFailed: %s\x1b[0m\n", path);
        while (1)
          swiWaitForVBlank();
      }
      printf("    %dpx: %dx%d\n", g_zoom_sizes[i],
             g_all_fonts[fam][i]->glyph_w, g_all_fonts[fam][i]->glyph_h);
    }
  }
  memcpy(g_fonts, g_all_fonts[g_font_family], sizeof(g_fonts));
  g_font = g_fonts[g_zoom_level];
  recompute_page_lines();
  printf("    %d lines/page\n", g_page_lines);

  printf("[6] Setting up framebuffers...\n");
  swiWaitForVBlank();

  tr_init_fb();

  tr_init_fb_sub();

  show_text();

  scanKeys();

  app_state_t app_state = ST_READ;

  int frame_count = 0;

  while (1) {
    swiWaitForVBlank();
    tr_select(TR_SCREEN_TOP);
    tr_draw_heartbeat(frame_count++);

    scanKeys();
    u32 keys = keysDown();

    if (keys & KEY_START)
      break;

    if (g_book >= 1 && g_book <= MAX_BOOKS)
      g_book_lines[g_book - 1] = (int16_t)g_line_num;

    if (app_state == ST_BAR) {
      if (--g_bar_timer <= 0)
        app_state = bar_dismiss();
    }

    if (app_state == ST_DRAW)
      draw_update();

    const keybind_t *binds = dispatch[app_state].binds;
    int n = dispatch[app_state].count;
    for (int i = 0; i < n; i++) {
      if (keys & binds[i].key) {
        app_state = binds[i].handler(app_state);
        break;
      }
    }
  }

  save_state();
  notes_save();
  draw_save();

  reader_close(g_ctx);
  for (int fam = 0; fam < NUM_FONT_FAMILIES; fam++)
    for (int i = 0; i < NUM_ZOOM_LEVELS; i++)
      tr_free_font(g_all_fonts[fam][i]);
  return 0;
}

#include <stdio.h>
#include <string.h>

#include "ui.h"


void draw_bar(void) {
  tr_select(TR_SCREEN_BOTTOM);

  const tr_font *bf = g_fonts[1];
  int lh = bf->glyph_h + 1;
  int bar_h = lh * 3 + 6;
  int bar_y = TR_SCREEN_H - bar_h;

  const palette_t *p = active_palette();
  tr_fill_rect(0, bar_y, TR_SCREEN_W, bar_h, pal_ui_bg(p));
  tr_draw_hline(0, bar_y, TR_SCREEN_W, p->num);

  const char *btn_label = "SET";
  int btn_tw = tr_text_width(bf, btn_label);
  int btn_pad = 4;
  g_settings_btn_w = btn_tw + btn_pad * 2;
  g_settings_btn_h = bar_h - 2;
  g_settings_btn_x = TR_SCREEN_W - g_settings_btn_w;
  g_settings_btn_y = bar_y + 1;
  tr_fill_rect(g_settings_btn_x, g_settings_btn_y, g_settings_btn_w,
               g_settings_btn_h, pal_btn_bg(p));
  int btn_text_y = g_settings_btn_y + (g_settings_btn_h - bf->glyph_h) / 2;
  tr_draw_text(bf, g_settings_btn_x + btn_pad, btn_text_y, btn_label, p->text);

  int y = bar_y + 3;

  int maxl = reader_max_line(g_ctx, "iliad", g_book);
  char line1[80];
  snprintf(line1, sizeof(line1),
           "Book %d/%d  [%d/%d]  %dpx  [L\xe2\x88\x92/R+]", g_book, g_num_books,
           g_line_num, maxl, g_zoom_sizes[g_zoom_level]);
  tr_draw_text(bf, 6, y, line1, p->hl);
  y += lh;

  tr_select(TR_SCREEN_TOP);
}

app_state_t bar_dismiss(void) {
  show_text();
  return ST_READ;
}


app_state_t on_bar_L(app_state_t s) {
  (void)s;
  if (g_zoom_level > 0) {
    g_zoom_level--;
    g_font = g_fonts[g_zoom_level];
    recompute_page_lines();
    show_text();
  }
  g_bar_timer = BAR_TIMEOUT_FRAMES;
  draw_bar();
  return ST_BAR;
}

app_state_t on_bar_R(app_state_t s) {
  (void)s;
  if (g_zoom_level < NUM_ZOOM_LEVELS - 1) {
    g_zoom_level++;
    g_font = g_fonts[g_zoom_level];
    recompute_page_lines();
    show_text();
  }
  g_bar_timer = BAR_TIMEOUT_FRAMES;
  draw_bar();
  return ST_BAR;
}

app_state_t on_bar_A(app_state_t s) {
  (void)s;
  g_fullscreen = !g_fullscreen;
  recompute_page_lines();
  show_text();
  g_bar_timer = BAR_TIMEOUT_FRAMES;
  draw_bar();
  return ST_BAR;
}

app_state_t on_bar_SELECT(app_state_t s) {
  (void)s;
  return bar_dismiss();
}

app_state_t on_bar_any(app_state_t s) {
  (void)s;
  return bar_dismiss();
}

app_state_t on_bar_TOUCH(app_state_t s) {
  (void)s;
  touchPosition touch;
  touchRead(&touch);
  if (touch.px >= g_settings_btn_x &&
      touch.px < g_settings_btn_x + g_settings_btn_w &&
      touch.py >= g_settings_btn_y &&
      touch.py < g_settings_btn_y + g_settings_btn_h) {
    g_set_book = g_book;
    g_set_line = g_line_num;
    g_set_cursor = 0;
    g_set_tab = 0;
    draw_settings();
    return ST_SETTINGS;
  }
  return bar_dismiss();
}


enum {
  NUM_TABS = 3,
  SET_ROW_H = 20,
  SET_BTN_W = 24,
  SET_VAL_W = 60,
  SET_LEFT_X = 40,
  SET_GO_W = 50,
  SET_GO_H = 22,
  TAB_H = 16,
  TAB_Y = 2,
};

static void draw_tab_info(const tr_font *sf, int content_y) {
  const palette_t *p = active_palette();
  int lh = sf->glyph_h + 2;
  int y = content_y + 4;
  char line[80], tmp[16];

  tr_draw_text(sf, 8, y, "Reader v0.1", p->hl);
  y += lh;
  snprintf(line, sizeof(line), "Corpus: Homer, Iliad  (%d books)", g_num_books);
  tr_draw_text(sf, 8, y, line, p->text);
  y += lh;
  snprintf(line, sizeof(line), "Book %d  line %d  zoom %dpx", g_book,
           g_line_num, g_zoom_sizes[g_zoom_level]);
  tr_draw_text(sf, 8, y, line, p->num);
  y += lh + 4;

  tr_draw_hline(0, y, TR_SCREEN_W, p->num);
  y += 6;

  tr_draw_text(sf, 8, y, "User data", p->hl);
  y += lh;

  int note_sz = notes_data_size();
  int draw_sz = draw_data_size();

  snprintf(line, sizeof(line), "Notes:     %d  (%s)", notes_get_count(),
           fmt_bytes(note_sz, tmp, sizeof(tmp)));
  tr_draw_text(sf, 8, y, line, p->text);
  y += lh;

  snprintf(line, sizeof(line), "Drawings:  %d strokes  (%s)",
           g_draw_stroke_count, fmt_bytes(draw_sz, tmp, sizeof(tmp)));
  tr_draw_text(sf, 8, y, line, p->text);
  y += lh;

  snprintf(line, sizeof(line), "Points:    %d / %d", g_draw_pool_used,
           DRAW_POOL_SIZE);
  tr_draw_text(sf, 8, y, line, p->text);
  y += lh + 4;

  tr_draw_hline(0, y, TR_SCREEN_W, p->num);
  y += 6;

  tr_draw_text(sf, 8, y, "B back  L/R tabs", p->num);
}

enum {
  PAL_SWATCH_W = 24,
  PAL_SWATCH_H = 16,
  PAL_ROW_H = 22,
};

static void draw_tab_colors(const tr_font *sf, int content_y) {
  const palette_t *ap = active_palette();
  int y = content_y + 4;

  for (int i = 0; i < NUM_PALETTES; i++) {
    const palette_t *p = (i < NUM_PRESET_PALETTES)
                             ? &g_preset_palettes[i]
                             : &g_custom_palettes[i - NUM_PRESET_PALETTES];
    int row_y = y;
    uint16_t label_col = (i == g_palette_idx) ? ap->hl : ap->text;

    if (i == g_palette_idx)
      tr_draw_text(sf, 4, row_y + 3, "\xe2\x96\xb6", ap->hl);

    tr_draw_text(sf, 18, row_y + 3, g_palette_names[i], label_col);

    int sx = 120;
    tr_fill_rect(sx, row_y + 2, PAL_SWATCH_W, PAL_SWATCH_H, p->bg);
    tr_fill_rect(sx + 28, row_y + 2, PAL_SWATCH_W, PAL_SWATCH_H, p->text);
    tr_fill_rect(sx + 56, row_y + 2, PAL_SWATCH_W, PAL_SWATCH_H, p->num);
    tr_fill_rect(sx + 84, row_y + 2, PAL_SWATCH_W, PAL_SWATCH_H, p->hl);

    if (i == g_set_cursor) {
      tr_draw_hline(0, row_y, TR_SCREEN_W, ap->num);
      tr_draw_hline(0, row_y + PAL_ROW_H - 1, TR_SCREEN_W, ap->num);
    }

    y += PAL_ROW_H;
  }

  y += 8;
  tr_draw_hline(0, y, TR_SCREEN_W, ap->num);
  y += 4;
  tr_draw_text(sf, 8, y, "\xe2\x86\x95 select  A apply  X edit  B back",
               ap->num);
}

enum {
  FONT_ROW_H = 28,
};

static void draw_tab_font(const tr_font *sf, int content_y) {
  const palette_t *p = active_palette();
  int y = content_y + 4;

  tr_draw_text(sf, 8, y, "Typeface", p->hl);
  y += sf->glyph_h + 6;

  for (int i = 0; i < NUM_FONT_FAMILIES; i++) {
    int row_y = y;
    uint16_t label_col = (i == g_font_family) ? p->hl : p->text;

    if (i == g_font_family)
      tr_draw_text(sf, 4, row_y + 3, "\xe2\x96\xb6", p->hl);

    tr_draw_text(sf, 18, row_y + 3, g_font_family_names[i], label_col);

    const tr_font *preview = g_all_fonts[i][g_zoom_level];
    int sample_y = row_y + 2;
    tr_draw_text(preview, 100, sample_y, "\xce\xbc\xe1\xbf\x86\xce\xbd\xce\xb9\xce\xbd", p->text);

    if (i == g_set_cursor) {
      tr_draw_hline(0, row_y, TR_SCREEN_W, p->num);
      tr_draw_hline(0, row_y + FONT_ROW_H - 1, TR_SCREEN_W, p->num);
    }

    y += FONT_ROW_H;
  }

  y += 8;
  tr_draw_hline(0, y, TR_SCREEN_W, p->num);
  y += 4;
  tr_draw_text(sf, 8, y, "\xe2\x86\x95 select  A apply  B back", p->num);
}

void draw_settings(void) {
  const palette_t *p = active_palette();
  tr_select(TR_SCREEN_BOTTOM);
  tr_clear(p->bg);

  const tr_font *sf = g_fonts[1];

  static const char *tab_names[NUM_TABS] = {"Info", "Colors", "Font"};
  int tab_w = TR_SCREEN_W / NUM_TABS;
  for (int i = 0; i < NUM_TABS; i++) {
    int tx = i * tab_w;
    uint16_t bg = (i == g_set_tab) ? pal_ui_bg(p) : pal_btn_bg(p);
    uint16_t fg = (i == g_set_tab) ? p->hl : p->num;
    tr_fill_rect(tx, TAB_Y, tab_w - 1, TAB_H, bg);
    int tw = tr_text_width(sf, tab_names[i]);
    tr_draw_text(sf, tx + (tab_w - tw) / 2, TAB_Y + (TAB_H - sf->glyph_h) / 2,
                 tab_names[i], fg);
  }

  int content_y = TAB_Y + TAB_H + 4;

  if (g_set_tab == 0)
    draw_tab_info(sf, content_y);
  else if (g_set_tab == 1)
    draw_tab_colors(sf, content_y);
  else
    draw_tab_font(sf, content_y);

  tr_select(TR_SCREEN_TOP);
}


void preview_top(void) {
  const palette_t *p = active_palette();
  tr_select(TR_SCREEN_TOP);
  tr_clear(p->bg);

  int line_h = g_font->glyph_h + 1;
  int half = TR_SCREEN_H / 2;

  static reader_line pv[MAX_PAGE_LINES];
  int pn =
      reader_get_lines(g_ctx, "iliad", g_book, g_line_num, g_page_lines, pv);
  if (pn > MAX_PAGE_LINES)
    pn = MAX_PAGE_LINES;

  int maxl = reader_max_line(g_ctx, "iliad", g_book);
  char hdr[80];
  snprintf(hdr, sizeof(hdr), "Book %d/%d  [%d / %d]  %dpx", g_book, g_num_books,
           g_line_num, maxl, g_zoom_sizes[g_zoom_level]);
  tr_draw_text(g_font, 2, 1, hdr, p->hl);
  int header_h = line_h + 2;
  tr_draw_hline(0, header_h - 1, TR_SCREEN_W, p->num);

  int y = header_h;
  for (int i = 0; i < pn && y + line_h <= half; i++) {
    char num[8];
    snprintf(num, sizeof(num), "%3d ", pv[i].line);
    int num_w = tr_text_width(g_font, num);
    int text_x = 2 + num_w;
    tr_draw_text(g_font, 2, y, num, p->num);
    int rows = tr_draw_text_wrap(g_font, text_x, text_x, y, TR_SCREEN_W - 2,
                                 pv[i].text, p->text);
    y += rows * line_h;
  }

  tr_draw_hline(0, half, TR_SCREEN_W, p->num);

  char first_word[MAX_WORD_LEN];
  first_word[0] = '\0';
  if (pn > 0) {
    const char *s = pv[0].text;
    while (*s == ' ' || *s == '\t')
      s++;
    int wi = 0;
    while (*s && *s != ' ' && *s != '\t' && *s != '\n' &&
           wi < MAX_WORD_LEN - 1) {
      first_word[wi++] = *s++;
    }
    first_word[wi] = '\0';
  }

  y = half + 2;
  if (first_word[0]) {
    static reader_morph morphs[4];
    int nm = reader_morph_lookup(g_ctx, first_word, morphs, 4);

    tr_draw_text(g_font, 4, y, first_word, p->hl);
    y += line_h;

    if (nm > 0) {
      tr_draw_text(g_font, 12, y, morphs[0].parse_str, p->hl);
      y += line_h;

      char arrow[4 + sizeof(morphs[0].lemma)];
      snprintf(arrow, sizeof(arrow), "-> %s", morphs[0].lemma);
      tr_draw_text(g_font, 12, y, arrow, p->text);
      y += line_h;

      static reader_lex_entry lex[1];
      int nl = reader_lex_lookup(g_ctx, morphs[0].lemma, lex, 1);
      if (nl > 0 && lex[0].short_def[0]) {
        tr_draw_text_wrap(g_font, 12, 16, y, TR_SCREEN_W - 4, lex[0].short_def,
                          p->text);
      }
    } else {
      tr_draw_text(g_font, 12, y, "(no morphology data)", p->num);
    }
  }

  tr_flip();
}


void settings_clamp(void) {
  if (g_set_book < 1)
    g_set_book = 1;
  if (g_set_book > g_num_books)
    g_set_book = g_num_books;
  int maxl = reader_max_line(g_ctx, "iliad", g_set_book);
  if (g_set_line < 1)
    g_set_line = 1;
  if (g_set_line > maxl)
    g_set_line = maxl;
}

app_state_t settings_go(void) {
  settings_clamp();
  if (g_book >= 1 && g_book <= MAX_BOOKS)
    g_book_lines[g_book - 1] = (int16_t)g_line_num;
  g_book = g_set_book;
  g_line_num = g_set_line;
  if (g_book >= 1 && g_book <= MAX_BOOKS)
    g_book_lines[g_book - 1] = (int16_t)g_line_num;
  recompute_page_lines();
  show_text();
  return ST_READ;
}


app_state_t on_settings_dismiss(app_state_t s) {
  (void)s;
  save_state();
  show_text();
  return ST_READ;
}

static void apply_font_family(int fam) {
  if (fam < 0 || fam >= NUM_FONT_FAMILIES)
    fam = 0;
  g_font_family = fam;
  memcpy(g_fonts, g_all_fonts[g_font_family], sizeof(g_fonts));
  g_font = g_fonts[g_zoom_level];
  recompute_page_lines();
}

app_state_t on_settings_A(app_state_t s) {
  (void)s;
  if (g_set_tab == 0)
    return s;
  if (g_set_tab == 1) {
    g_palette_idx = g_set_cursor;
    if (g_palette_idx >= NUM_PALETTES)
      g_palette_idx = 0;
  } else if (g_set_tab == 2) {
    apply_font_family(g_set_cursor);
  }
  draw_settings();
  preview_top();
  return s;
}

app_state_t on_settings_X(app_state_t s) {
  (void)s;
  if (g_set_tab != 1)
    return s;
  if (g_set_cursor < NUM_PRESET_PALETTES)
    return s;
  g_pick_custom = g_set_cursor - NUM_PRESET_PALETTES;
  g_pick_field = 0;
  g_pick_slider = 0;
  g_palette_idx = g_set_cursor;
  draw_picker();
  return ST_PICKER;
}

app_state_t on_settings_UP(app_state_t s) {
  (void)s;
  if (g_set_cursor > 0)
    g_set_cursor--;
  draw_settings();
  return s;
}

app_state_t on_settings_DOWN(app_state_t s) {
  (void)s;
  int max_cursor = 0;
  if (g_set_tab == 1)
    max_cursor = NUM_PALETTES - 1;
  else if (g_set_tab == 2)
    max_cursor = NUM_FONT_FAMILIES - 1;
  if (g_set_cursor < max_cursor)
    g_set_cursor++;
  draw_settings();
  return s;
}

app_state_t on_settings_RIGHT(app_state_t s) {
  (void)s;
  draw_settings();
  return s;
}

app_state_t on_settings_LEFT(app_state_t s) {
  (void)s;
  draw_settings();
  return s;
}

static int cursor_for_tab(int tab) {
  if (tab == 1)
    return g_palette_idx;
  if (tab == 2)
    return g_font_family;
  return 0;
}

app_state_t on_settings_R(app_state_t s) {
  (void)s;
  if (g_set_tab < NUM_TABS - 1) {
    g_set_tab++;
    g_set_cursor = cursor_for_tab(g_set_tab);
    draw_settings();
  }
  return s;
}

app_state_t on_settings_L(app_state_t s) {
  (void)s;
  if (g_set_tab > 0) {
    g_set_tab--;
    g_set_cursor = cursor_for_tab(g_set_tab);
    draw_settings();
  }
  return s;
}

app_state_t on_settings_TOUCH(app_state_t s) {
  touchPosition touch;
  touchRead(&touch);
  int tx = touch.px, ty = touch.py;

  if (ty >= TAB_Y && ty < TAB_Y + TAB_H) {
    int new_tab = tx / (TR_SCREEN_W / NUM_TABS);
    if (new_tab >= NUM_TABS)
      new_tab = NUM_TABS - 1;
    if (new_tab != g_set_tab) {
      g_set_tab = new_tab;
      g_set_cursor = cursor_for_tab(g_set_tab);
      draw_settings();
    }
    return s;
  }

  int content_y = TAB_Y + TAB_H + 4;

  if (g_set_tab == 1) {
    int base_y = content_y + 4;
    for (int i = 0; i < NUM_PALETTES; i++) {
      int row_y = base_y + i * PAL_ROW_H;
      if (ty >= row_y && ty < row_y + PAL_ROW_H) {
        if (i >= NUM_PRESET_PALETTES) {
          int sx = 120;
          for (int f = 0; f < 4; f++) {
            int sw_x = sx + f * 28;
            if (tx >= sw_x && tx < sw_x + PAL_SWATCH_W && ty >= row_y + 2 &&
                ty < row_y + 2 + PAL_SWATCH_H) {
              g_pick_custom = i - NUM_PRESET_PALETTES;
              g_pick_field = f;
              g_pick_slider = 0;
              g_palette_idx = i;
              draw_picker();
              return ST_PICKER;
            }
          }
        }
        g_set_cursor = i;
        g_palette_idx = i;
        draw_settings();
        preview_top();
        return s;
      }
    }
  } else if (g_set_tab == 2) {
    int lh_title = g_fonts[1]->glyph_h + 6;
    int base_y = content_y + 4 + lh_title;
    for (int i = 0; i < NUM_FONT_FAMILIES; i++) {
      int row_y = base_y + i * FONT_ROW_H;
      if (ty >= row_y && ty < row_y + FONT_ROW_H) {
        g_set_cursor = i;
        apply_font_family(i);
        draw_settings();
        preview_top();
        return s;
      }
    }
  }

  return s;
}


enum {
  PICK_SLIDER_X = 40,
  PICK_SLIDER_W = 160,
  PICK_SLIDER_H = 14,
  PICK_BTN_W = 52,
  PICK_BTN_H = 18,
};

static const char *pick_field_names[4] = {"BG", "TXT", "NUM", "HL"};

static uint16_t *pick_color_ptr(void) {
  palette_t *p = &g_custom_palettes[g_pick_custom];
  switch (g_pick_field) {
  case 0:
    return &p->bg;
  case 1:
    return &p->text;
  case 2:
    return &p->num;
  default:
    return &p->hl;
  }
}

static void pick_layout(const tr_font *sf, int *out_field_y, int *out_preview_y,
                        int *out_slider_y0) {
  int lh = sf->glyph_h + 1;
  int y = 4;
  y += lh + 4;
  y += 4;
  *out_field_y = y;
  y += PICK_BTN_H + 8;
  *out_preview_y = y;
  y += 24;
  *out_slider_y0 = y;
}

void draw_picker(void) {
  const palette_t *ap = active_palette();
  const tr_font *sf = g_fonts[1];
  int lh = sf->glyph_h + 1;

  tr_select(TR_SCREEN_BOTTOM);
  tr_clear(ap->bg);

  int field_y, preview_y, slider_y0;
  pick_layout(sf, &field_y, &preview_y, &slider_y0);

  char title[40];
  snprintf(title, sizeof(title), "Edit: Custom %d", g_pick_custom + 1);
  tr_draw_text(sf, 8, 4, title, ap->hl);
  tr_draw_hline(0, 4 + lh + 4, TR_SCREEN_W, ap->num);

  for (int i = 0; i < 4; i++) {
    int bx = 8 + i * 60;
    uint16_t bg = (i == g_pick_field) ? pal_ui_bg(ap) : pal_btn_bg(ap);
    uint16_t fg = (i == g_pick_field) ? ap->hl : ap->text;
    tr_fill_rect(bx, field_y, PICK_BTN_W, PICK_BTN_H, bg);
    int tw = tr_text_width(sf, pick_field_names[i]);
    tr_draw_text(sf, bx + (PICK_BTN_W - tw) / 2,
                 field_y + (PICK_BTN_H - sf->glyph_h) / 2, pick_field_names[i],
                 fg);
  }

  uint16_t cur_color = *pick_color_ptr();
  tr_fill_rect(8, preview_y, TR_SCREEN_W - 16, 20, cur_color);
  tr_draw_hline(8, preview_y, TR_SCREEN_W - 16, ap->num);
  tr_draw_hline(8, preview_y + 19, TR_SCREEN_W - 16, ap->num);

  int rgb[3];
  pick_decompose(cur_color, &rgb[0], &rgb[1], &rgb[2]);
  static const char *slider_labels[3] = {"R", "G", "B"};
  static const uint16_t slider_colors[3] = {0x801F, 0x83E0, 0xFC00};

  for (int i = 0; i < 3; i++) {
    int sy = slider_y0 + i * (PICK_SLIDER_H + 6);
    uint16_t lbl_col = (i == g_pick_slider) ? ap->hl : ap->text;
    tr_draw_text(sf, 8, sy + 2, slider_labels[i], lbl_col);

    tr_fill_rect(PICK_SLIDER_X, sy, PICK_SLIDER_W, PICK_SLIDER_H,
                 pal_btn_bg(ap));
    int fill_w = (rgb[i] * PICK_SLIDER_W) / 31;
    if (fill_w > 0)
      tr_fill_rect(PICK_SLIDER_X, sy, fill_w, PICK_SLIDER_H, slider_colors[i]);

    char vbuf[12];
    snprintf(vbuf, sizeof(vbuf), "%d", rgb[i]);
    tr_draw_text(sf, PICK_SLIDER_X + PICK_SLIDER_W + 8, sy + 2, vbuf, lbl_col);

    if (i == g_pick_slider) {
      tr_draw_hline(PICK_SLIDER_X, sy, PICK_SLIDER_W, ap->hl);
      tr_draw_hline(PICK_SLIDER_X, sy + PICK_SLIDER_H - 1, PICK_SLIDER_W,
                    ap->hl);
    }
  }

  int hy = slider_y0 + 3 * (PICK_SLIDER_H + 6) + 8;
  tr_draw_hline(0, hy - 4, TR_SCREEN_W, ap->num);
  tr_draw_text(sf, 8, hy,
               "\xe2\x86\x95 slider  \xe2\x86\x94 \xc2\xb1"
               "1  L/R \xc2\xb1"
               "4  A field  B back",
               ap->num);

  int ry = hy + lh + 4;
  if (ry + lh <= TR_SCREEN_H) {
    char hexbuf[48];
    snprintf(hexbuf, sizeof(hexbuf), "%s: 0x%04X  R:%d G:%d B:%d",
             pick_field_names[g_pick_field], cur_color, rgb[0], rgb[1], rgb[2]);
    tr_draw_text(sf, 8, ry, hexbuf, ap->num);
  }

  preview_top();
}

app_state_t on_picker_B(app_state_t s) {
  (void)s;
  g_set_tab = 1;
  g_set_cursor = g_pick_custom + NUM_PRESET_PALETTES;
  g_palette_idx = g_set_cursor;
  draw_settings();
  return ST_SETTINGS;
}

app_state_t on_picker_A(app_state_t s) {
  g_pick_field = (g_pick_field + 1) % 4;
  draw_picker();
  return s;
}

app_state_t on_picker_UP(app_state_t s) {
  if (g_pick_slider > 0)
    g_pick_slider--;
  draw_picker();
  return s;
}

app_state_t on_picker_DOWN(app_state_t s) {
  if (g_pick_slider < 2)
    g_pick_slider++;
  draw_picker();
  return s;
}

app_state_t on_picker_RIGHT(app_state_t s) {
  uint16_t *cp = pick_color_ptr();
  int rgb[3];
  pick_decompose(*cp, &rgb[0], &rgb[1], &rgb[2]);
  if (rgb[g_pick_slider] < 31)
    rgb[g_pick_slider]++;
  *cp = pick_compose(rgb[0], rgb[1], rgb[2]);
  draw_picker();
  return s;
}

app_state_t on_picker_LEFT(app_state_t s) {
  uint16_t *cp = pick_color_ptr();
  int rgb[3];
  pick_decompose(*cp, &rgb[0], &rgb[1], &rgb[2]);
  if (rgb[g_pick_slider] > 0)
    rgb[g_pick_slider]--;
  *cp = pick_compose(rgb[0], rgb[1], rgb[2]);
  draw_picker();
  return s;
}

app_state_t on_picker_R(app_state_t s) {
  uint16_t *cp = pick_color_ptr();
  int rgb[3];
  pick_decompose(*cp, &rgb[0], &rgb[1], &rgb[2]);
  rgb[g_pick_slider] += 4;
  *cp = pick_compose(rgb[0], rgb[1], rgb[2]);
  draw_picker();
  return s;
}

app_state_t on_picker_L(app_state_t s) {
  uint16_t *cp = pick_color_ptr();
  int rgb[3];
  pick_decompose(*cp, &rgb[0], &rgb[1], &rgb[2]);
  rgb[g_pick_slider] -= 4;
  *cp = pick_compose(rgb[0], rgb[1], rgb[2]);
  draw_picker();
  return s;
}

app_state_t on_picker_TOUCH(app_state_t s) {
  touchPosition touch;
  touchRead(&touch);
  int tx = touch.px, ty = touch.py;

  const tr_font *sf = g_fonts[1];
  int field_y, preview_y, slider_y0;
  pick_layout(sf, &field_y, &preview_y, &slider_y0);

  for (int i = 0; i < 4; i++) {
    int bx = 8 + i * 60;
    if (tx >= bx && tx < bx + PICK_BTN_W && ty >= field_y &&
        ty < field_y + PICK_BTN_H) {
      g_pick_field = i;
      draw_picker();
      return s;
    }
  }

  for (int i = 0; i < 3; i++) {
    int sy = slider_y0 + i * (PICK_SLIDER_H + 6);
    if (ty >= sy && ty < sy + PICK_SLIDER_H && tx >= PICK_SLIDER_X &&
        tx < PICK_SLIDER_X + PICK_SLIDER_W) {
      g_pick_slider = i;
      int val = ((tx - PICK_SLIDER_X) * 31 + PICK_SLIDER_W / 2) / PICK_SLIDER_W;
      if (val < 0)
        val = 0;
      if (val > 31)
        val = 31;

      uint16_t *cp = pick_color_ptr();
      int rgb[3];
      pick_decompose(*cp, &rgb[0], &rgb[1], &rgb[2]);
      rgb[i] = val;
      *cp = pick_compose(rgb[0], rgb[1], rgb[2]);
      draw_picker();
      return s;
    }
  }

  return s;
}

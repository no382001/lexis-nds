#include <stdlib.h>
#include <string.h>

#include "keyboard.h"
#include "ui.h"

kb_state_t g_kb;

typedef struct {
  int x, y, w, h;
  const char *label;
  char ch;
  int action;
} kb_key_t;

enum {
  KB_ACT_NONE = 0,
  KB_ACT_BACKSPACE = 1,
  KB_ACT_ENTER = 2,
  KB_ACT_SHIFT = 3,
  KB_ACT_SPACE = 4,
  KB_ACT_CANCEL = 5,
  KB_ACT_DONE = 6,
  KB_ACT_NEWLINE = 7,
};


enum {
  GP = 4,
  GKW = 52,
  GKH = 26,
  GX0 = (256 - 3 * GKW - 2 * GP) / 2,
  GY0 = 40,
};

#define GROW(r) (GY0 + (r) * (GKH + GP))
#define GCOL(c) (GX0 + (c) * (GKW + GP))

static const kb_key_t goto_keys[] = {
    {GCOL(0), GROW(0), GKW, GKH, "1", '1', 0},
    {GCOL(1), GROW(0), GKW, GKH, "2", '2', 0},
    {GCOL(2), GROW(0), GKW, GKH, "3", '3', 0},
    {GCOL(0), GROW(1), GKW, GKH, "4", '4', 0},
    {GCOL(1), GROW(1), GKW, GKH, "5", '5', 0},
    {GCOL(2), GROW(1), GKW, GKH, "6", '6', 0},
    {GCOL(0), GROW(2), GKW, GKH, "7", '7', 0},
    {GCOL(1), GROW(2), GKW, GKH, "8", '8', 0},
    {GCOL(2), GROW(2), GKW, GKH, "9", '9', 0},
    {GCOL(0), GROW(3), GKW, GKH, ".", '.', 0},
    {GCOL(1), GROW(3), GKW, GKH, "0", '0', 0},
    {GCOL(2), GROW(3), GKW, GKH, "\xe2\x86\x90", 0, KB_ACT_BACKSPACE},
    {GCOL(0), GROW(4), GKW * 2 + GP, GKH, "Enter", 0, KB_ACT_ENTER},
    {GCOL(2), GROW(4), GKW, GKH, "Esc", 0, KB_ACT_CANCEL},
};

#define NUM_GOTO_KEYS ((int)countof(goto_keys))


enum {
  LKW = 22,
  LKH = 24,
  LGP = 2,
  LY0 = 64,
};

enum {
  LR0X = (256 - 10 * LKW - 9 * LGP) / 2,
  LR1X = (256 - 9 * LKW - 8 * LGP) / 2,
  LR2X = 4,
};

#define LROW(r) (LY0 + (r) * (LKH + LGP))
#define LCOL(r0, c) ((r0) + (c) * (LKW + LGP))

static const kb_key_t latin_lower[] = {
    {LCOL(LR0X, 0), LROW(0), LKW, LKH, "q", 'q', 0},
    {LCOL(LR0X, 1), LROW(0), LKW, LKH, "w", 'w', 0},
    {LCOL(LR0X, 2), LROW(0), LKW, LKH, "e", 'e', 0},
    {LCOL(LR0X, 3), LROW(0), LKW, LKH, "r", 'r', 0},
    {LCOL(LR0X, 4), LROW(0), LKW, LKH, "t", 't', 0},
    {LCOL(LR0X, 5), LROW(0), LKW, LKH, "y", 'y', 0},
    {LCOL(LR0X, 6), LROW(0), LKW, LKH, "u", 'u', 0},
    {LCOL(LR0X, 7), LROW(0), LKW, LKH, "i", 'i', 0},
    {LCOL(LR0X, 8), LROW(0), LKW, LKH, "o", 'o', 0},
    {LCOL(LR0X, 9), LROW(0), LKW, LKH, "p", 'p', 0},
    {LCOL(LR1X, 0), LROW(1), LKW, LKH, "a", 'a', 0},
    {LCOL(LR1X, 1), LROW(1), LKW, LKH, "s", 's', 0},
    {LCOL(LR1X, 2), LROW(1), LKW, LKH, "d", 'd', 0},
    {LCOL(LR1X, 3), LROW(1), LKW, LKH, "f", 'f', 0},
    {LCOL(LR1X, 4), LROW(1), LKW, LKH, "g", 'g', 0},
    {LCOL(LR1X, 5), LROW(1), LKW, LKH, "h", 'h', 0},
    {LCOL(LR1X, 6), LROW(1), LKW, LKH, "j", 'j', 0},
    {LCOL(LR1X, 7), LROW(1), LKW, LKH, "k", 'k', 0},
    {LCOL(LR1X, 8), LROW(1), LKW, LKH, "l", 'l', 0},
    {LR2X, LROW(2), LKW + 6, LKH, "\xe2\x87\xa7", 0, KB_ACT_SHIFT},
    {LR2X + LKW + 6 + LGP, LROW(2), LKW, LKH, "z", 'z', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 1), LROW(2), LKW, LKH, "x", 'x', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 2), LROW(2), LKW, LKH, "c", 'c', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 3), LROW(2), LKW, LKH, "v", 'v', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 4), LROW(2), LKW, LKH, "b", 'b', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 5), LROW(2), LKW, LKH, "n", 'n', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 6), LROW(2), LKW, LKH, "m", 'm', 0},
    {256 - LKW - 6 - LGP, LROW(2), LKW + 6, LKH, "\xe2\x86\x90", 0,
     KB_ACT_BACKSPACE},
    {4, LROW(3), 28, LKH, "X", 0, KB_ACT_CANCEL},
    {36, LROW(3), 120, LKH, " ", 0, KB_ACT_SPACE},
    {160, LROW(3), 36, LKH, "\xe2\x86\xb5", 0, KB_ACT_NEWLINE},
    {200, LROW(3), 52, LKH, "Done", 0, KB_ACT_DONE},
};

static const kb_key_t latin_upper[] = {
    {LCOL(LR0X, 0), LROW(0), LKW, LKH, "Q", 'Q', 0},
    {LCOL(LR0X, 1), LROW(0), LKW, LKH, "W", 'W', 0},
    {LCOL(LR0X, 2), LROW(0), LKW, LKH, "E", 'E', 0},
    {LCOL(LR0X, 3), LROW(0), LKW, LKH, "R", 'R', 0},
    {LCOL(LR0X, 4), LROW(0), LKW, LKH, "T", 'T', 0},
    {LCOL(LR0X, 5), LROW(0), LKW, LKH, "Y", 'Y', 0},
    {LCOL(LR0X, 6), LROW(0), LKW, LKH, "U", 'U', 0},
    {LCOL(LR0X, 7), LROW(0), LKW, LKH, "I", 'I', 0},
    {LCOL(LR0X, 8), LROW(0), LKW, LKH, "O", 'O', 0},
    {LCOL(LR0X, 9), LROW(0), LKW, LKH, "P", 'P', 0},
    {LCOL(LR1X, 0), LROW(1), LKW, LKH, "A", 'A', 0},
    {LCOL(LR1X, 1), LROW(1), LKW, LKH, "S", 'S', 0},
    {LCOL(LR1X, 2), LROW(1), LKW, LKH, "D", 'D', 0},
    {LCOL(LR1X, 3), LROW(1), LKW, LKH, "F", 'F', 0},
    {LCOL(LR1X, 4), LROW(1), LKW, LKH, "G", 'G', 0},
    {LCOL(LR1X, 5), LROW(1), LKW, LKH, "H", 'H', 0},
    {LCOL(LR1X, 6), LROW(1), LKW, LKH, "J", 'J', 0},
    {LCOL(LR1X, 7), LROW(1), LKW, LKH, "K", 'K', 0},
    {LCOL(LR1X, 8), LROW(1), LKW, LKH, "L", 'L', 0},
    {LR2X, LROW(2), LKW + 6, LKH, "\xe2\x87\xa7", 0, KB_ACT_SHIFT},
    {LR2X + LKW + 6 + LGP, LROW(2), LKW, LKH, "Z", 'Z', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 1), LROW(2), LKW, LKH, "X", 'X', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 2), LROW(2), LKW, LKH, "C", 'C', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 3), LROW(2), LKW, LKH, "V", 'V', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 4), LROW(2), LKW, LKH, "B", 'B', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 5), LROW(2), LKW, LKH, "N", 'N', 0},
    {LCOL(LR2X + LKW + 6 + LGP, 6), LROW(2), LKW, LKH, "M", 'M', 0},
    {256 - LKW - 6 - LGP, LROW(2), LKW + 6, LKH, "\xe2\x86\x90", 0,
     KB_ACT_BACKSPACE},
    {4, LROW(3), 28, LKH, "X", 0, KB_ACT_CANCEL},
    {36, LROW(3), 120, LKH, " ", 0, KB_ACT_SPACE},
    {160, LROW(3), 36, LKH, "\xe2\x86\xb5", 0, KB_ACT_NEWLINE},
    {200, LROW(3), 52, LKH, "Done", 0, KB_ACT_DONE},
};

#define NUM_LATIN_KEYS ((int)countof(latin_lower))

static void get_layout(const kb_key_t **out_keys, int *out_count) {
  switch (g_kb.mode) {
  case KB_MODE_GOTO:
    *out_keys = goto_keys;
    *out_count = NUM_GOTO_KEYS;
    break;
  case KB_MODE_LATIN:
    *out_keys = g_kb.shift ? latin_upper : latin_lower;
    *out_count = NUM_LATIN_KEYS;
    break;
  default:
    *out_keys = goto_keys;
    *out_count = NUM_GOTO_KEYS;
    break;
  }
}

static void buf_append_utf8(const char *s) {
  int slen = (int)strlen(s);
  if (g_kb.len + slen < KB_BUF_MAX - 1) {
    memcpy(g_kb.buf + g_kb.len, s, (size_t)slen);
    g_kb.len += slen;
    g_kb.buf[g_kb.len] = '\0';
  }
}

static void buf_append(char ch) {
  if (g_kb.len < KB_BUF_MAX - 1) {
    g_kb.buf[g_kb.len++] = ch;
    g_kb.buf[g_kb.len] = '\0';
  }
}

static void buf_backspace(void) {
  if (g_kb.len <= 0)
    return;
  int i = g_kb.len - 1;
  while (i > 0 && (g_kb.buf[i] & 0xC0) == 0x80)
    i--;
  g_kb.len = i;
  g_kb.buf[g_kb.len] = '\0';
}


void kb_open(kb_mode_t mode) {
  memset(&g_kb, 0, sizeof(g_kb));
  g_kb.mode = mode;
  g_kb.result = KB_RESULT_NONE;
}

void kb_draw(void) {
  const palette_t *p = active_palette();
  uint16_t bg = p->bg;
  uint16_t fg = p->text;
  uint16_t hl = p->hl;
  uint16_t dim = p->num;
  uint16_t kbg = pal_btn_bg(p);

  tr_select(TR_SCREEN_BOTTOM);
  tr_clear(bg);

  tr_font *tf = g_fonts[2];

  int field_y = 4;
  int field_rows = (g_kb.mode == KB_MODE_LATIN) ? 3 : 2;
  int field_h = tf->glyph_h * field_rows + 8;

  tr_fill_rect(4, field_y, 248, field_h, kbg);
  tr_draw_hline(4, field_y, 248, dim);
  tr_draw_hline(4, field_y + field_h - 1, 248, dim);

  const char *mode_label = "?:";
  switch (g_kb.mode) {
  case KB_MODE_GREEK:
    mode_label = "\xCE\xB1\xCE\xB2:";
    break;
  case KB_MODE_LATIN:
    mode_label = "ab:";
    break;
  case KB_MODE_GOTO:
    mode_label = "Hom. Il. ";
    break;
  }
  tr_draw_text(tf, 8, field_y + 4, mode_label, dim);
  int label_w = tr_text_width(tf, mode_label);

  char display[KB_BUF_MAX + 2];
  snprintf(display, sizeof(display), "%s_", g_kb.buf);
  int text_x = 8 + label_w + 4;
  tr_draw_text_wrap(tf, text_x, 8, field_y + 4, 252, display, fg);

  if (g_kb.error[0] != '\0') {
    int err_y = field_y + field_h + 4;
    tr_draw_text(tf, 8, err_y, g_kb.error, hl);
  }

  const kb_key_t *keys;
  int nkeys;
  get_layout(&keys, &nkeys);

  for (int i = 0; i < nkeys; i++) {
    const kb_key_t *k = &keys[i];

    uint16_t key_bg = kbg;
    uint16_t key_fg = fg;

    if (k->action == KB_ACT_SHIFT && g_kb.shift) {
      key_bg = hl;
      key_fg = bg;
    }
    if (k->action == KB_ACT_ENTER || k->action == KB_ACT_DONE) {
      key_bg = hl;
      key_fg = bg;
    }

    tr_fill_rect(k->x, k->y, k->w, k->h, key_bg);

    tr_draw_hline(k->x, k->y, k->w, dim);
    tr_draw_hline(k->x, k->y + k->h - 1, k->w, dim);
    for (int row = k->y; row < k->y + k->h; row++) {
      tr_fill_rect(k->x, row, 1, 1, dim);
      tr_fill_rect(k->x + k->w - 1, row, 1, 1, dim);
    }

    tr_font *lf = (g_kb.mode == KB_MODE_GOTO) ? g_fonts[3] : g_fonts[2];
    int tw = tr_text_width(lf, k->label);
    int tx = k->x + (k->w - tw) / 2;
    int ty = k->y + (k->h - lf->glyph_h) / 2;

    if (k->action == KB_ACT_SPACE) {
      tw = tr_text_width(tf, "space");
      tx = k->x + (k->w - tw) / 2;
      tr_draw_text(tf, tx, ty, "space", dim);
    } else {
      tr_draw_text(lf, tx, ty, k->label, key_fg);
    }
  }
}

kb_result_t kb_touch(int tx, int ty) {
  const kb_key_t *keys;
  int nkeys;
  get_layout(&keys, &nkeys);

  for (int i = 0; i < nkeys; i++) {
    const kb_key_t *k = &keys[i];
    if (tx >= k->x && tx < k->x + k->w && ty >= k->y && ty < k->y + k->h) {

      g_kb.error[0] = '\0';

      if (k->ch != 0) {
        buf_append(k->ch);
      } else if (k->action == KB_ACT_NONE && k->label[0] != '\0') {
        buf_append_utf8(k->label);
      } else {
        switch (k->action) {
        case KB_ACT_BACKSPACE:
          buf_backspace();
          break;
        case KB_ACT_ENTER:
          g_kb.result = KB_RESULT_CONFIRM;
          return g_kb.result;
        case KB_ACT_DONE:
          g_kb.result = KB_RESULT_CONFIRM;
          return g_kb.result;
        case KB_ACT_NEWLINE:
          buf_append('\n');
          break;
        case KB_ACT_CANCEL:
          g_kb.result = KB_RESULT_CANCEL;
          return g_kb.result;
        case KB_ACT_SHIFT:
          g_kb.shift = !g_kb.shift;
          break;
        case KB_ACT_SPACE:
          buf_append(' ');
          break;
        default:
          break;
        }
      }
      kb_draw();
      break;
    }
  }
  return KB_RESULT_NONE;
}

kb_result_t kb_key(uint32_t keys_down) {
  if (keys_down & KEY_B) {
    g_kb.result = KB_RESULT_CANCEL;
    return g_kb.result;
  }
  if (keys_down & KEY_A) {
    g_kb.result = KB_RESULT_CONFIRM;
    return g_kb.result;
  }
  return KB_RESULT_NONE;
}


static app_state_t kb_confirm_and_return(void) {
  if (g_kb.mode == KB_MODE_GOTO) {
    int new_book = 0, new_line = 0;
    const char *dot = strchr(g_kb.buf, '.');
    if (dot) {
      new_book = atoi(g_kb.buf);
      new_line = atoi(dot + 1);
    } else {
      new_line = atoi(g_kb.buf);
    }
    if (new_book >= 1) {
      if (new_book > g_num_books) {
        snprintf(g_kb.error, sizeof(g_kb.error), "Books: 1-%d", g_num_books);
        kb_draw();
        return ST_KB_GOTO;
      }
      if (g_book >= 1 && g_book <= MAX_BOOKS)
        g_book_lines[g_book - 1] = (int16_t)g_line_num;
      g_book = new_book;
    }
    if (new_line >= 1) {
      int maxl = reader_max_line(g_ctx, "iliad", g_book);
      if (new_line > maxl) {
        snprintf(g_kb.error, sizeof(g_kb.error), "Book %d: lines 1-%d", g_book,
                 maxl);
        kb_draw();
        return ST_KB_GOTO;
      }
      g_line_num = new_line;
    }
    show_text();
    return ST_READ;
  }

  if (g_kb.mode == KB_MODE_LATIN) {
    if (g_kb.buf[0] != '\0')
      notes_set(g_result_title, g_kb.buf);
    else
      notes_set(g_result_title, NULL);
    notes_save();

    build_lookup_result(g_result_title, 0);
    draw_lookup_result();
    return ST_LOOKUP;
  }

  show_text();
  return ST_READ;
}

static app_state_t kb_cancel_and_return(void) {
  if (g_kb.mode == KB_MODE_LATIN) {
    draw_lookup_result();
    return ST_LOOKUP;
  }
  show_text();
  return ST_READ;
}

app_state_t on_kb_TOUCH(app_state_t s) {
  touchPosition touch;
  touchRead(&touch);
  kb_result_t r = kb_touch(touch.px, touch.py);
  if (r == KB_RESULT_CONFIRM)
    return kb_confirm_and_return();
  if (r == KB_RESULT_CANCEL)
    return kb_cancel_and_return();
  return s;
}

app_state_t on_kb_key(app_state_t s) {
  (void)s;
  return kb_cancel_and_return();
}

app_state_t on_kb_A(app_state_t s) {
  (void)s;
  return kb_confirm_and_return();
}

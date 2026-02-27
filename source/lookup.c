#include <stdio.h>
#include <string.h>

#include "ui.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
void result_push(const char *text, uint16_t color, int indent) {
  if (g_result_count >= MAX_RESULT_LINES)
    return;
  result_line *r = &g_result_buf[g_result_count++];
  snprintf(r->text, MAX_RESULT_LEN, "%s", text);
  r->color = color;
  r->indent = indent;
}
#pragma GCC diagnostic pop

void build_lookup_result(const char *word, int dict_mode) {
  g_result_count = 0;
  g_result_scroll = 0;
  strncpy(g_result_title, word, MAX_WORD_LEN - 1);
  g_result_title[MAX_WORD_LEN - 1] = '\0';

  if (dict_mode) {
    static reader_lex_entry entries[4];
    int n = reader_lex_lookup(g_ctx, word, entries, 4);

    if (n > 0) {
      for (int i = 0; i < n; i++) {
        result_push(entries[i].lemma, active_palette()->hl, 4);
        if (entries[i].short_def[0])
          result_push(entries[i].short_def, active_palette()->text, 12);
        result_push("", active_palette()->bg, 0);
      }
    } else {
      static reader_morph morphs[8];
      int nm = reader_morph_lookup(g_ctx, word, morphs, 8);
      if (nm > 0) {
        for (int i = 0; i < nm; i++) {
          n = reader_lex_lookup(g_ctx, morphs[i].lemma, entries, 1);
          if (n > 0) {
            char buf[MAX_RESULT_LEN];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(buf, sizeof(buf), "%s (%s)", entries[0].lemma,
                     morphs[i].parse_str);
#pragma GCC diagnostic pop
            result_push(buf, active_palette()->hl, 4);
            if (entries[0].short_def[0])
              result_push(entries[0].short_def, active_palette()->text, 12);
            result_push("", active_palette()->bg, 0);
          }
        }
      } else {
        char buf[MAX_RESULT_LEN];
        snprintf(buf, sizeof(buf), "Not found: %s", word);
        result_push(buf, active_palette()->num, 4);
      }
    }
  } else {
    static reader_morph morphs[8];
    int nm = reader_morph_lookup(g_ctx, word, morphs, 8);

    if (nm == 0) {
      char buf[MAX_RESULT_LEN];
      snprintf(buf, sizeof(buf), "No data for: %s", word);
      result_push(buf, active_palette()->num, 4);
    } else {
      for (int i = 0; i < nm; i++) {
        char buf[MAX_RESULT_LEN];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(buf, sizeof(buf), "%s -> %s", morphs[i].form, morphs[i].lemma);
#pragma GCC diagnostic pop
        result_push(buf, active_palette()->hl, 4);
        result_push(morphs[i].parse_str, active_palette()->hl, 12);

        static reader_lex_entry lex[1];
        int nl = reader_lex_lookup(g_ctx, morphs[i].lemma, lex, 1);
        if (nl > 0 && lex[0].short_def[0])
          result_push(lex[0].short_def, active_palette()->text, 12);
        result_push("", active_palette()->bg, 0);
      }
    }
  }

  const char *note = notes_find(word);
  if (note) {
    result_push("--- Note ---", active_palette()->num, 4);
    result_push(note, active_palette()->hl, 8);
  }
}

void draw_lookup_result(void) {
  const palette_t *p = active_palette();
  tr_select(TR_SCREEN_TOP);
  tr_clear(p->bg);

  int line_h = g_font->glyph_h + 1;
  int header_h = line_h + 2;
  int footer_h = line_h + 4;
  int y_max = TR_SCREEN_H - footer_h;

  if (g_result_scroll >= g_result_count)
    g_result_scroll = g_result_count - 1;
  if (g_result_scroll < 0)
    g_result_scroll = 0;

  tr_draw_hline(0, header_h - 1, TR_SCREEN_W, p->num);

  int y = header_h;
  for (int i = g_result_scroll; i < g_result_count; i++) {
    if (y + line_h > y_max)
      break;
    const result_line *r = &g_result_buf[i];
    int rows = tr_draw_text_wrap(g_font, r->indent, r->indent + 8, y,
                                 TR_SCREEN_W - 2, r->text, r->color);
    y += rows * line_h;
  }

  int footer_y = TR_SCREEN_H - g_font->glyph_h - 2;
  tr_draw_hline(0, footer_y - 2, TR_SCREEN_W, p->num);
  tr_draw_text(g_font, 4, footer_y, "[B] back  [Y] note  Up/Dn scroll", p->hl);

  tr_flip();
}


app_state_t on_lookup_TOUCH(app_state_t s) {
  touchPosition touch;
  touchRead(&touch);
  char tapped_word[MAX_WORD_LEN];
  if (touch_to_word(touch.px, touch.py, tapped_word, MAX_WORD_LEN)) {
    build_lookup_result(tapped_word, 0);
    draw_lookup_result();
  }
  return s;
}

app_state_t on_lookup_B(app_state_t s) {
  (void)s;
  show_text();
  return ST_READ;
}

app_state_t on_lookup_DOWN(app_state_t s) {
  g_result_scroll++;
  draw_lookup_result();
  return s;
}

app_state_t on_lookup_UP(app_state_t s) {
  if (g_result_scroll > 0)
    g_result_scroll--;
  draw_lookup_result();
  return s;
}

app_state_t on_lookup_Y(app_state_t s) {
  (void)s;
  kb_open(KB_MODE_LATIN);
  const char *existing = notes_find(g_result_title);
  if (existing) {
    strncpy(g_kb.buf, existing, KB_BUF_MAX - 1);
    g_kb.buf[KB_BUF_MAX - 1] = '\0';
    g_kb.len = (int)strlen(g_kb.buf);
  }
  kb_draw();
  return ST_KB_LATIN;
}

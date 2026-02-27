#pragma once

#include <stddef.h>
#include <stdint.h>

enum {
  TR_SCREEN_W = 256,
  TR_SCREEN_H = 192,
};

enum {
  TR_SCREEN_TOP = 0,
  TR_SCREEN_BOTTOM = 1,
};

enum {
  TR_ALPHA = 0x8000,
  TR_WHITE = 0xFFFF,
  TR_BLACK = 0x8000,
  TR_YELLOW = 0x83FF,
  TR_CYAN = 0xFBE0,
  TR_RED = 0x801F,
  TR_GREY = 0xAD6B,
  TR_DKGREY = 0x94A5,
  TR_DKNAVY = 0x9C00,
};

typedef struct {
  uint32_t codepoint;
  uint8_t advance;
  int8_t reserved;
  uint16_t atlas_idx;
} tr_glyph_entry;

typedef struct {
  uint8_t glyph_w;
  uint8_t glyph_h;
  uint16_t num_glyphs;
  uint8_t baseline;
  uint8_t row_bytes;

  tr_glyph_entry *glyphs;
  uint8_t *bitmaps;
} tr_font;

tr_font *tr_load_font(const char *path);
void tr_free_font(tr_font *f);

void tr_init_fb(void);
void tr_init_fb_sub(void);
void tr_select(int screen);
void tr_flip(void);
void tr_clear(uint16_t color);
void tr_fill_rect(int x, int y, int w, int h, uint16_t color);

int tr_draw_text(const tr_font *f, int x, int y, const char *utf8,
                 uint16_t color);
int tr_draw_text_wrap(const tr_font *f, int x_start, int x_indent, int y,
                      int max_x, const char *utf8, uint16_t color);
int tr_count_wrapped_lines(const tr_font *f, int x_start, int x_indent,
                           int max_x, const char *utf8);
int tr_word_at_pos(const tr_font *f, int x_start, int x_indent, int y,
                   int max_x, const char *utf8, int px, int py, char *out,
                   int out_len);

void tr_draw_pixel(int x, int y, uint16_t color);
void tr_draw_line(int x0, int y0, int x1, int y1, uint16_t color);
void tr_draw_hline(int x, int y, int w, uint16_t color);
int tr_text_width(const tr_font *f, const char *utf8);
void tr_draw_heartbeat(int frame);

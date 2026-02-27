#include "text_render.h"
#include "common.h"

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  TR_MAX_CHARS = 512,
  TR_FALLBACK_ADV = 4,
  PFNT_HEADER_SIZE = 16,
  HEARTBEAT_SIZE = 4,
};

static int s_top_bg;
static uint16_t *s_top_buf[2];
static int s_top_back = 0;

static uint16_t *fb_bot = nil;

static uint16_t *fb = nil;

static int s_selected = TR_SCREEN_TOP;

void tr_init_fb(void) {
  videoSetMode(MODE_5_2D);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_BG);

  s_top_bg = bgInit(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

  bgSetRotateScale(s_top_bg, 0, 1 << 8, 1 << 8);
  bgUpdate();

  s_top_buf[0] = bgGetGfxPtr(s_top_bg);
  s_top_buf[1] = s_top_buf[0] + 256 * 256;

  s_top_back = 1;
  fb = s_top_buf[s_top_back];
}

void tr_init_fb_sub(void) {
  videoSetModeSub(MODE_5_2D);
  vramSetBankC(VRAM_C_SUB_BG);

  int bg = bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
  fb_bot = bgGetGfxPtr(bg);

  bgSetRotateScale(bg, 0, 1 << 8, 1 << 8);
  bgUpdate();
}

void tr_select(int screen) {
  s_selected = screen;
  fb = (screen == TR_SCREEN_BOTTOM) ? fb_bot : s_top_buf[s_top_back];
}

void tr_flip(void) {
  bgSetMapBase(s_top_bg, s_top_back * 8);
  bgUpdate();
  s_top_back ^= 1;
  if (s_selected == TR_SCREEN_TOP)
    fb = s_top_buf[s_top_back];
}

void tr_clear(uint16_t color) {
  if (!fb)
    return;
  uint32_t fill = (uint32_t)color | ((uint32_t)color << 16);
  uint32_t *p = (uint32_t *)fb;
  for (int i = 0; i < TR_SCREEN_W * TR_SCREEN_H / 2; i++)
    p[i] = fill;
}

void tr_draw_pixel(int x, int y, uint16_t color) {
  if (!fb || x < 0 || y < 0 || x >= TR_SCREEN_W || y >= TR_SCREEN_H)
    return;
  fb[y * TR_SCREEN_W + x] = color;
}

// bresenham
void tr_draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = x1 - x0;
  int dy = y1 - y0;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  while (1) {
    tr_draw_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void tr_draw_hline(int x, int y, int w, uint16_t color) {
  if (!fb || y < 0 || y >= TR_SCREEN_H)
    return;
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (x + w > TR_SCREEN_W)
    w = TR_SCREEN_W - x;
  uint16_t *row = fb + y * TR_SCREEN_W + x;
  for (int i = 0; i < w; i++)
    row[i] = color;
}

void tr_fill_rect(int x, int y, int w, int h, uint16_t color) {
  for (int row = 0; row < h; row++)
    tr_draw_hline(x, y + row, w, color);
}


static uint32_t utf8_decode(const char **p) {
  const uint8_t *s = (const uint8_t *)*p;
  uint32_t cp;
  int extra;

  if (s[0] < 0x80) {
    cp = s[0];
    extra = 0;
  } else if ((s[0] & 0xE0) == 0xC0) {
    cp = s[0] & 0x1F;
    extra = 1;
  } else if ((s[0] & 0xF0) == 0xE0) {
    cp = s[0] & 0x0F;
    extra = 2;
  } else if ((s[0] & 0xF8) == 0xF0) {
    cp = s[0] & 0x07;
    extra = 3;
  } else {
    *p = (const char *)(s + 1);
    return 0xFFFD;
  }

  for (int i = 1; i <= extra; i++) {
    if ((s[i] & 0xC0) != 0x80) {
      *p = (const char *)(s + i);
      return 0xFFFD;
    }
    cp = (cp << 6) | (s[i] & 0x3F);
  }

  *p = (const char *)(s + 1 + extra);
  return cp;
}


tr_font *tr_load_font(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return nil;

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (sz < 16) {
    fclose(f);
    return nil;
  }

  char magic[4];
  fread(magic, 1, 4, f);
  if (memcmp(magic, "PFNT", 4) != 0) {
    fclose(f);
    return nil;
  }

  uint8_t glyph_w, glyph_h, baseline;
  uint16_t num_glyphs;
  fread(&glyph_w, 1, 1, f);
  fread(&glyph_h, 1, 1, f);
  fread(&num_glyphs, 2, 1, f);
  fread(&baseline, 1, 1, f);
  fseek(f, PFNT_HEADER_SIZE, SEEK_SET); /* skip padding */

  uint8_t row_bytes = (glyph_w + 7) / 8;

  tr_font *font = (tr_font *)calloc(1, sizeof(tr_font));
  if (!font) {
    fclose(f);
    return nil;
  }

  font->glyph_w = glyph_w;
  font->glyph_h = glyph_h;
  font->num_glyphs = num_glyphs;
  font->baseline = baseline;
  font->row_bytes = row_bytes;

  font->glyphs = (tr_glyph_entry *)malloc(num_glyphs * sizeof(tr_glyph_entry));
  if (!font->glyphs) {
    free(font);
    fclose(f);
    return nil;
  }
  fread(font->glyphs, sizeof(tr_glyph_entry), num_glyphs, f);

  size_t bmp_size = (size_t)num_glyphs * glyph_h * row_bytes;
  font->bitmaps = (uint8_t *)malloc(bmp_size);
  if (!font->bitmaps) {
    free(font->glyphs);
    free(font);
    fclose(f);
    return nil;
  }
  fread(font->bitmaps, 1, bmp_size, f);

  fclose(f);
  return font;
}

void tr_free_font(tr_font *f) {
  if (!f)
    return;
  free(f->bitmaps);
  free(f->glyphs);
  free(f);
}


static const tr_glyph_entry *find_glyph(const tr_font *f, uint32_t cp) {
  int lo = 0, hi = (int)f->num_glyphs - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (f->glyphs[mid].codepoint < cp)
      lo = mid + 1;
    else if (f->glyphs[mid].codepoint > cp)
      hi = mid - 1;
    else
      return &f->glyphs[mid];
  }
  return nil;
}

static void blit_glyph(const tr_font *f, const tr_glyph_entry *g, int x, int y,
                       uint16_t color) {
  if (!fb)
    return;

  size_t glyph_offset = (size_t)g->atlas_idx * f->glyph_h * f->row_bytes;
  const uint8_t *bitmap = f->bitmaps + glyph_offset;

  for (int row = 0; row < f->glyph_h; row++) {
    int py = y + row;
    if (py < 0) {
      bitmap += f->row_bytes;
      continue;
    }
    if (py >= TR_SCREEN_H)
      break;

    const uint8_t *rowdata = bitmap;
    bitmap += f->row_bytes;

    for (int col = 0; col < f->glyph_w; col++) {
      int px = x + col;
      if (px < 0)
        continue;
      if (px >= TR_SCREEN_W)
        break;

      if (rowdata[col >> 3] & (0x80 >> (col & 7))) {
        fb[py * TR_SCREEN_W + px] = color;
      }
    }
  }
}

int tr_draw_text(const tr_font *f, int x, int y, const char *utf8,
                 uint16_t color) {
  if (!f || !utf8 || !fb)
    return x;

  int chars = 0;
  while (*utf8 && chars < TR_MAX_CHARS) {
    uint32_t cp = utf8_decode(&utf8);
    chars++;

    const tr_glyph_entry *g = find_glyph(f, cp);
    if (g) {
      blit_glyph(f, g, x, y, color);
      x += g->advance;
    } else {
      x += TR_FALLBACK_ADV;
    }

    if (x >= TR_SCREEN_W)
      break;
  }

  return x;
}

int tr_text_width(const tr_font *f, const char *utf8) {
  if (!f || !utf8)
    return 0;
  int w = 0;
  while (*utf8) {
    uint32_t cp = utf8_decode(&utf8);
    const tr_glyph_entry *g = find_glyph(f, cp);
    w += g ? g->advance : TR_FALLBACK_ADV;
  }
  return w;
}


static int measure_word(const tr_font *f, const char *start, const char **end) {
  int w = 0;
  const char *p = start;
  while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
    uint32_t cp = utf8_decode(&p);
    const tr_glyph_entry *g = find_glyph(f, cp);
    w += g ? g->advance : TR_FALLBACK_ADV;
  }
  *end = p;
  return w;
}

int tr_draw_text_wrap(const tr_font *f, int x_start, int x_indent, int y,
                      int max_x, const char *utf8, uint16_t color) {
  if (!f || !utf8)
    return 0;
  int line_h = f->glyph_h + 1;
  int x = x_start;
  int rows = 1;
  const char *p = utf8;

  while (*p) {
    /* skip spaces, measure them */
    while (*p == ' ' || *p == '\t') {
      uint32_t cp = utf8_decode(&p);
      const tr_glyph_entry *g = find_glyph(f, cp);
      int adv = g ? g->advance : 4;
      x += adv;
    }
    if (!*p)
      break;
    if (*p == '\n') {
      p++;
      x = x_indent;
      y += line_h;
      rows++;
      continue;
    }

    const char *word_end;
    int word_w = measure_word(f, p, &word_end);

    if (x + word_w > max_x && x > x_indent) {
      x = x_indent;
      y += line_h;
      rows++;
    }

    while (p < word_end) {
      uint32_t cp = utf8_decode(&p);
      const tr_glyph_entry *g = find_glyph(f, cp);
      if (g && fb) {
        blit_glyph(f, g, x, y, color);
        x += g->advance;
      } else {
        x += 4;
      }
    }
  }
  return rows;
}

int tr_count_wrapped_lines(const tr_font *f, int x_start, int x_indent,
                           int max_x, const char *utf8) {
  if (!f || !utf8)
    return 1;
  int x = x_start;
  int rows = 1;
  const char *p = utf8;

  while (*p) {
    while (*p == ' ' || *p == '\t') {
      uint32_t cp = utf8_decode(&p);
      const tr_glyph_entry *g = find_glyph(f, cp);
      x += g ? g->advance : TR_FALLBACK_ADV;
    }
    if (!*p)
      break;
    if (*p == '\n') {
      p++;
      x = x_indent;
      rows++;
      continue;
    }

    const char *word_end;
    int word_w = measure_word(f, p, &word_end);

    if (x + word_w > max_x && x > x_indent) {
      x = x_indent;
      rows++;
    }

    while (p < word_end) {
      uint32_t cp = utf8_decode(&p);
      const tr_glyph_entry *g = find_glyph(f, cp);
      x += g ? g->advance : TR_FALLBACK_ADV;
    }
  }
  return rows;
}

int tr_word_at_pos(const tr_font *f, int x_start, int x_indent, int y,
                   int max_x, const char *utf8, int px, int py, char *out,
                   int out_len) {
  if (!f || !utf8 || !out || out_len < 2)
    return 0;
  int line_h = f->glyph_h + 1;
  int x = x_start;
  const char *p = utf8;

  while (*p) {
    while (*p == ' ' || *p == '\t') {
      uint32_t cp = utf8_decode(&p);
      const tr_glyph_entry *g = find_glyph(f, cp);
      x += g ? g->advance : TR_FALLBACK_ADV;
    }
    if (!*p)
      break;
    if (*p == '\n') {
      p++;
      x = x_indent;
      y += line_h;
      continue;
    }

    const char *word_end;
    int word_w = measure_word(f, p, &word_end);

    if (x + word_w > max_x && x > x_indent) {
      x = x_indent;
      y += line_h;
    }

    if (py >= y && py < y + line_h && px >= x && px < x + word_w) {
      int len = (int)(word_end - p);
      if (len >= out_len)
        len = out_len - 1;
      memcpy(out, p, len);
      out[len] = '\0';
      return 1;
    }

    while (p < word_end) {
      uint32_t cp = utf8_decode(&p);
      const tr_glyph_entry *g = find_glyph(f, cp);
      x += g ? g->advance : TR_FALLBACK_ADV;
    }
  }
  return 0;
}

void tr_draw_heartbeat(int frame) {
  uint16_t *front = s_top_buf[s_top_back ^ 1];
  uint16_t c = (frame & 1) ? TR_WHITE : (TR_ALPHA | 0x001F); /* red */
  for (int dy = 0; dy < HEARTBEAT_SIZE; dy++)
    for (int dx = 0; dx < HEARTBEAT_SIZE; dx++)
      front[(dy)*TR_SCREEN_W + (TR_SCREEN_W - HEARTBEAT_SIZE - 1 + dx)] = c;
}

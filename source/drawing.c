#include "drawing.h"
#include "ui.h"

#include <nds.h>
#include <stdio.h>
#include <string.h>


draw_point_t g_draw_pool[DRAW_POOL_SIZE];
int g_draw_pool_used;
draw_stroke_t g_draw_strokes[DRAW_MAX_STROKES];
int g_draw_stroke_count;

/* current bottom-screen line map */
draw_line_map_t g_draw_cur_map[DRAW_MAX_MAP];
int g_draw_cur_map_count;

static int draw_dirty;


static int cur_stroke_idx = -1;
static int last_sx, last_sy;


static void draw_compact_pool(void) {
  int new_used = 0;
  for (int i = 0; i < g_draw_stroke_count; i++) {
    draw_stroke_t *s = &g_draw_strokes[i];
    if ((int)s->start != new_used) {
      memmove(&g_draw_pool[new_used], &g_draw_pool[s->start],
              s->count * sizeof(draw_point_t));
      s->start = (uint16_t)new_used;
    }
    new_used += s->count;
  }
  g_draw_pool_used = new_used;
}

static int screen_to_line(int sy, int16_t *out_line, int16_t *out_yoff) {
  for (int m = 0; m < g_draw_cur_map_count; m++) {
    int top = g_draw_cur_map[m].y;
    int bot =
        (m + 1 < g_draw_cur_map_count) ? g_draw_cur_map[m + 1].y : TR_SCREEN_H;
    if (sy >= top && sy < bot) {
      *out_line = g_draw_cur_map[m].line;
      *out_yoff = (int16_t)(sy - top);
      return 1;
    }
  }
  if (g_draw_cur_map_count > 0) {
    int last = g_draw_cur_map_count - 1;
    *out_line = g_draw_cur_map[last].line;
    *out_yoff = (int16_t)(sy - g_draw_cur_map[last].y);
    return 1;
  }
  return 0;
}

static int line_to_screen(const draw_line_map_t *map, int map_count,
                          int16_t text_line, int16_t y_off) {
  for (int m = 0; m < map_count; m++) {
    if (map[m].line == text_line)
      return map[m].y + y_off;
  }
  return DRAW_OFF_SCREEN;
}

void draw_init(void) {
  _Static_assert(sizeof(draw_point_t) == 6, "draw_point_t packing");
  _Static_assert(sizeof(draw_stroke_t) == 8, "draw_stroke_t packing");

  g_draw_pool_used = 0;
  g_draw_stroke_count = 0;
  draw_dirty = 0;
  cur_stroke_idx = -1;
  g_draw_cur_map_count = 0;
}

void draw_update(void) {
  u32 down = keysDown();
  u32 held = keysHeld();
  u32 up = keysUp();

  if (down & KEY_TOUCH) {
    if (g_draw_stroke_count >= DRAW_MAX_STROKES ||
        g_draw_pool_used >= DRAW_POOL_SIZE)
      goto end_check;

    touchPosition touch;
    touchRead(&touch);

    int16_t pt_line, pt_yoff;
    if (!screen_to_line(touch.py, &pt_line, &pt_yoff))
      goto end_check;

    cur_stroke_idx = g_draw_stroke_count;
    draw_stroke_t *s = &g_draw_strokes[cur_stroke_idx];
    s->start = (uint16_t)g_draw_pool_used;
    s->count = 1;
    s->book = (int16_t)g_book;
    s->zoom = (int8_t)g_zoom_level;
    s->_pad = 0;

    g_draw_pool[g_draw_pool_used].x = (int16_t)touch.px;
    g_draw_pool[g_draw_pool_used].y_off = pt_yoff;
    g_draw_pool[g_draw_pool_used].line = pt_line;
    g_draw_pool_used++;
    g_draw_stroke_count++;

    last_sx = touch.px;
    last_sy = touch.py;

    tr_select(TR_SCREEN_BOTTOM);
    tr_draw_pixel(touch.px, touch.py, active_palette()->hl);

    draw_dirty = 1;

  } else if ((held & KEY_TOUCH) && cur_stroke_idx >= 0) {
    if (g_draw_pool_used >= DRAW_POOL_SIZE)
      goto end_check;

    touchPosition touch;
    touchRead(&touch);

    int dx = touch.px - last_sx;
    int dy = touch.py - last_sy;
    if (dx * dx + dy * dy < DRAW_MIN_DIST_SQ)
      goto end_check;

    int16_t pt_line, pt_yoff;
    if (!screen_to_line(touch.py, &pt_line, &pt_yoff))
      goto end_check;

    draw_stroke_t *s = &g_draw_strokes[cur_stroke_idx];
    g_draw_pool[g_draw_pool_used].x = (int16_t)touch.px;
    g_draw_pool[g_draw_pool_used].y_off = pt_yoff;
    g_draw_pool[g_draw_pool_used].line = pt_line;
    g_draw_pool_used++;
    s->count++;

    tr_select(TR_SCREEN_BOTTOM);
    tr_draw_line(last_sx, last_sy, touch.px, touch.py, active_palette()->hl);

    last_sx = touch.px;
    last_sy = touch.py;
  }

end_check:
  if (up & KEY_TOUCH) {
    cur_stroke_idx = -1;
  }
}

void draw_render_overlay(int cur_book, int cur_zoom, const draw_line_map_t *map,
                         int map_count, uint16_t color) {
  if (map_count == 0)
    return;

  for (int i = 0; i < g_draw_stroke_count; i++) {
    draw_stroke_t *s = &g_draw_strokes[i];
    if (s->book != cur_book || s->zoom != cur_zoom)
      continue;
    if (s->count == 0)
      continue;

    if (s->count == 1) {
      draw_point_t *p = &g_draw_pool[s->start];
      int sy = line_to_screen(map, map_count, p->line, p->y_off);
      if (sy >= 0 && sy < TR_SCREEN_H)
        tr_draw_pixel(p->x, sy, color);
      continue;
    }

    draw_point_t *prev = &g_draw_pool[s->start];
    int prev_sy = line_to_screen(map, map_count, prev->line, prev->y_off);
    int prev_vis = (prev_sy > DRAW_OFF_SCREEN + 1000);
    for (int j = 1; j < (int)s->count; j++) {
      draw_point_t *cur = &g_draw_pool[s->start + j];
      int cur_sy = line_to_screen(map, map_count, cur->line, cur->y_off);
      int cur_vis = (cur_sy > DRAW_OFF_SCREEN + 1000);
      if (prev_vis && cur_vis)
        tr_draw_line(prev->x, prev_sy, cur->x, cur_sy, color);
      prev = cur;
      prev_sy = cur_sy;
      prev_vis = cur_vis;
    }
  }
}

void draw_show_indicator(void) {
  tr_select(TR_SCREEN_BOTTOM);
  const palette_t *p = active_palette();
  int h = g_fonts[0]->glyph_h + 2;
  int y = TR_SCREEN_H - h;
  tr_fill_rect(0, y, TR_SCREEN_W, h, p->hl);
  tr_draw_text(g_fonts[0], 2, y + 1, "[X] done  [Y] clear", p->bg);
}

void draw_clear_view(int cur_book, int cur_line, int cur_zoom) {
  (void)cur_line;
  int dst = 0;
  for (int src = 0; src < g_draw_stroke_count; src++) {
    draw_stroke_t *s = &g_draw_strokes[src];
    if (s->book == cur_book && s->zoom == cur_zoom)
      continue;
    if (dst != src)
      g_draw_strokes[dst] = *s;
    dst++;
  }
  g_draw_stroke_count = dst;
  draw_compact_pool();
  draw_dirty = 1;
}


static const char DRAW_PATH[] = "fat:/data/reader/drawings.dat";

enum {
  DRAW_MAGIC = 0x57415244,
  DRAW_VERSION = 2,
};

void draw_load(void) {
  FILE *f = fopen(DRAW_PATH, "rb");
  if (!f)
    return;

  uint32_t magic, version, pool_n, stroke_n;
  if (fread(&magic, 4, 1, f) != 1 || magic != DRAW_MAGIC)
    goto fail;
  if (fread(&version, 4, 1, f) != 1 || version != DRAW_VERSION)
    goto fail;
  if (fread(&pool_n, 4, 1, f) != 1)
    goto fail;
  if (fread(&stroke_n, 4, 1, f) != 1)
    goto fail;

  if (pool_n > DRAW_POOL_SIZE || stroke_n > DRAW_MAX_STROKES)
    goto fail;

  if (fread(g_draw_pool, sizeof(draw_point_t), pool_n, f) != pool_n)
    goto fail;
  if (fread(g_draw_strokes, sizeof(draw_stroke_t), stroke_n, f) != stroke_n)
    goto fail;

  g_draw_pool_used = (int)pool_n;
  g_draw_stroke_count = (int)stroke_n;
  draw_dirty = 0;
  fclose(f);
  return;

fail:
  draw_init();
  fclose(f);
}

void draw_save(void) {
  if (!draw_dirty)
    return;

  FILE *f = fopen(DRAW_PATH, "wb");
  if (!f)
    return;

  uint32_t magic = DRAW_MAGIC;
  uint32_t version = DRAW_VERSION;
  uint32_t pool_n = (uint32_t)g_draw_pool_used;
  uint32_t stroke_n = (uint32_t)g_draw_stroke_count;

  fwrite(&magic, 4, 1, f);
  fwrite(&version, 4, 1, f);
  fwrite(&pool_n, 4, 1, f);
  fwrite(&stroke_n, 4, 1, f);
  fwrite(g_draw_pool, sizeof(draw_point_t), pool_n, f);
  fwrite(g_draw_strokes, sizeof(draw_stroke_t), stroke_n, f);

  fclose(f);
  draw_dirty = 0;
}

int draw_data_size(void) {
  if (g_draw_stroke_count == 0)
    return 0;
  return 16 + g_draw_pool_used * (int)sizeof(draw_point_t) +
         g_draw_stroke_count * (int)sizeof(draw_stroke_t);
}

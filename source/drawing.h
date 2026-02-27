#pragma once

#include <stdint.h>

enum {
  DRAW_POOL_SIZE = 8192,
  DRAW_MAX_STROKES = 256,
  DRAW_MAX_MAP = 64,
  DRAW_OFF_SCREEN = -9999,
  DRAW_MIN_DIST_SQ = 4,
};

typedef struct {
  int16_t x;
  int16_t y_off;
  int16_t line;
} draw_point_t;

typedef struct {
  uint16_t start;
  uint16_t count;
  int16_t book;
  int8_t zoom;
  uint8_t _pad;
} draw_stroke_t;

typedef struct {
  int16_t line;
  int16_t y;
} draw_line_map_t;

extern draw_line_map_t g_draw_cur_map[DRAW_MAX_MAP];
extern int g_draw_cur_map_count;

extern draw_point_t g_draw_pool[DRAW_POOL_SIZE];
extern int g_draw_pool_used;
extern draw_stroke_t g_draw_strokes[DRAW_MAX_STROKES];
extern int g_draw_stroke_count;

void draw_init(void);
void draw_update(void);
void draw_render_overlay(int cur_book, int cur_zoom, const draw_line_map_t *map,
                         int map_count, uint16_t color);
void draw_show_indicator(void);
void draw_clear_view(int cur_book, int cur_line, int cur_zoom);
void draw_load(void);
void draw_save(void);
int draw_data_size(void);

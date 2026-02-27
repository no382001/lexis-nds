#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "ui.h"


enum {
  SAVE_MAGIC = 0x56525350,
  SAVE_VERSION = 2,
};

static const char SAVE_PATH[] = "fat:/data/reader/reader.sav";

typedef struct {
  uint32_t magic;
  uint8_t version;
  uint8_t g_zoom_level;
  uint8_t g_fullscreen;
  uint8_t g_palette_idx;
  int16_t cur_book;
  uint8_t g_font_family;
  uint8_t reserved;
  int16_t g_book_lines[MAX_BOOKS];
  palette_t custom[NUM_CUSTOM_PALETTES];
} __attribute__((packed)) reader_save_t;


static void ensure_dir(const char *path) {
  char tmp[128];
  strncpy(tmp, path, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  /* skip scheme prefix like "fat:/" */
  char *start = strstr(tmp, ":/");
  start = start ? start + 2 : tmp + 1;
  for (char *p = start; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
}


void save_state(void) {
  _Static_assert(sizeof(reader_save_t) <= 128, "save struct size");

  if (!g_fat_ok)
    return;
  ensure_dir(SAVE_PATH);
  FILE *f = fopen(SAVE_PATH, "wb");
  if (!f)
    return;
  reader_save_t sv;
  memset(&sv, 0, sizeof(sv));
  sv.magic = SAVE_MAGIC;
  sv.version = SAVE_VERSION;
  sv.g_zoom_level = (uint8_t)g_zoom_level;
  sv.g_fullscreen = (uint8_t)g_fullscreen;
  sv.g_palette_idx = (uint8_t)g_palette_idx;
  sv.cur_book = (int16_t)g_book;
  sv.g_font_family = (uint8_t)g_font_family;
  if (g_book >= 1 && g_book <= MAX_BOOKS)
    g_book_lines[g_book - 1] = (int16_t)g_line_num;
  memcpy(sv.g_book_lines, g_book_lines, sizeof(g_book_lines));
  memcpy(sv.custom, g_custom_palettes, sizeof(g_custom_palettes));
  fwrite(&sv, sizeof(sv), 1, f);
  fclose(f);
}

int load_state(void) {
  if (!g_fat_ok)
    return 0;
  FILE *f = fopen(SAVE_PATH, "rb");
  if (!f)
    return 0;
  reader_save_t sv;
  int ok = (fread(&sv, sizeof(sv), 1, f) == 1);
  fclose(f);
  if (!ok || sv.magic != SAVE_MAGIC || sv.version != SAVE_VERSION)
    return 0;
  g_zoom_level = sv.g_zoom_level;
  if (g_zoom_level >= NUM_ZOOM_LEVELS)
    g_zoom_level = 1;
  g_fullscreen = sv.g_fullscreen ? 1 : 0;
  g_palette_idx = sv.g_palette_idx;
  if (g_palette_idx >= NUM_PALETTES)
    g_palette_idx = 0;
  g_font_family = sv.g_font_family;
  if (g_font_family >= NUM_FONT_FAMILIES)
    g_font_family = 0;
  g_book = sv.cur_book;
  memcpy(g_book_lines, sv.g_book_lines, sizeof(g_book_lines));
  memcpy(g_custom_palettes, sv.custom, sizeof(g_custom_palettes));
  if (g_book >= 1 && g_book <= MAX_BOOKS)
    g_line_num = g_book_lines[g_book - 1];
  if (g_line_num < 1)
    g_line_num = 1;
  return 1;
}

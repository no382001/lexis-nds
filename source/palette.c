#include "ui.h"

const palette_t g_preset_palettes[NUM_PRESET_PALETTES] = {
    {0x8000, 0xFFFF, 0xAD6B, 0xFBE0},
    {0xFFFF, 0x8000, 0x94A5, 0x801F},
    {0x9EF7, 0x8842, 0x9109, 0x819F},
};

const char *g_palette_names[NUM_PALETTES] = {"Dark", "Light", "Sepia",
                                             "Custom 1", "Custom 2"};


const palette_t *active_palette(void) {
  if (g_palette_idx < NUM_PRESET_PALETTES)
    return &g_preset_palettes[g_palette_idx];
  return &g_custom_palettes[g_palette_idx - NUM_PRESET_PALETTES];
}

uint16_t pal_ui_bg(const palette_t *p) {
  int r = ((p->bg & 0x1F) * 3 + (p->num & 0x1F)) >> 2;
  int g = (((p->bg >> 5) & 0x1F) * 3 + ((p->num >> 5) & 0x1F)) >> 2;
  int b = (((p->bg >> 10) & 0x1F) * 3 + ((p->num >> 10) & 0x1F)) >> 2;
  return 0x8000 | (b << 10) | (g << 5) | r;
}

uint16_t pal_btn_bg(const palette_t *p) {
  int r = ((p->bg & 0x1F) + (p->num & 0x1F)) >> 1;
  int g = (((p->bg >> 5) & 0x1F) + ((p->num >> 5) & 0x1F)) >> 1;
  int b = (((p->bg >> 10) & 0x1F) + ((p->num >> 10) & 0x1F)) >> 1;
  return 0x8000 | (b << 10) | (g << 5) | r;
}


void pick_decompose(uint16_t c, int *r, int *g, int *b) {
  *r = c & 0x1F;
  *g = (c >> 5) & 0x1F;
  *b = (c >> 10) & 0x1F;
}

uint16_t pick_compose(int r, int g, int b) {
  if (r < 0)
    r = 0;
  if (r > 31)
    r = 31;
  if (g < 0)
    g = 0;
  if (g > 31)
    g = 31;
  if (b < 0)
    b = 0;
  if (b > 31)
    b = 31;
  return 0x8000 | (b << 10) | (g << 5) | r;
}

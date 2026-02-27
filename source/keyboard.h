#pragma once

#include <stdint.h>

enum { KB_BUF_MAX = 256 };

typedef enum {
  KB_MODE_GREEK,
  KB_MODE_LATIN,
  KB_MODE_GOTO,
} kb_mode_t;

typedef enum {
  KB_RESULT_NONE,
  KB_RESULT_CONFIRM,
  KB_RESULT_CANCEL,
} kb_result_t;


typedef struct {
  kb_mode_t mode;
  char buf[KB_BUF_MAX];
  int len;
  int shift;
  kb_result_t result;
  char error[64];
} kb_state_t;

extern kb_state_t g_kb;

void kb_open(kb_mode_t mode);
void kb_draw(void);
kb_result_t kb_touch(int tx, int ty);
kb_result_t kb_key(uint32_t keys_down);

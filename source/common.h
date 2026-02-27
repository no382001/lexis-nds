#pragma once

#include <stdio.h>

#define nil ((void *)0)
#define countof(a) (sizeof(a) / sizeof((a)[0]))

static inline const char *fmt_bytes(int bytes, char *buf, int bufsz) {
  if (bytes == 0) {
    snprintf(buf, bufsz, "0 B");
  } else if (bytes < 1024) {
    snprintf(buf, bufsz, "%d B", bytes);
  } else {
    snprintf(buf, bufsz, "%d.%d KB", bytes / 1024, (bytes % 1024) * 10 / 1024);
  }
  return buf;
}

#pragma once

#include "common.h"
#include <stddef.h>

typedef struct reader_ctx reader_ctx;

typedef struct {
  int book;
  int line;
  char text[2048];
} reader_line;

typedef struct {
  char form[256];
  char lemma[256];
  char postag[16];
  char parse_str[256];
} reader_morph;

typedef struct {
  char lemma[256];
  char short_def[1024];
  char definition[8192];
} reader_lex_entry;

reader_ctx *reader_open(const char *db_path);
void reader_close(reader_ctx *ctx);

int reader_get_lines(reader_ctx *ctx, const char *work, int book,
                     int start_line, int count, reader_line *out);
int reader_book_count(reader_ctx *ctx, const char *work);
int reader_max_line(reader_ctx *ctx, const char *work, int book);

int reader_morph_lookup(reader_ctx *ctx, const char *form, reader_morph *out,
                        int max_results);

int reader_lex_lookup(reader_ctx *ctx, const char *lemma, reader_lex_entry *out,
                      int max_results);

void reader_format_postag(const char *postag, char *out, size_t out_size);

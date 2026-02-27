#include "reader.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PRDB_MAX_BOOKS = 30 };

typedef struct {
  char magic[4];
  uint32_t version;
  uint32_t num_texts;
  uint32_t num_morphs;
  uint32_t num_lex;
  uint32_t num_books;
  uint32_t text_idx_off;
  uint32_t morph_idx_off;
  uint32_t lex_idx_off;
  uint32_t strings_off;
  uint32_t book_max[PRDB_MAX_BOOKS];
} prdb_header;

typedef struct {
  uint16_t book;
  uint16_t line;
  uint32_t text_off;
} prdb_text;

typedef struct {
  uint32_t form_off;
  uint32_t lemma_off;
  uint32_t postag_off;
} prdb_morph;

typedef struct {
  uint32_t lemma_off;
  uint32_t short_def_off;
  uint32_t def_off;
} prdb_lex;

struct reader_ctx {
  uint8_t *data;
  size_t data_size;
  prdb_header *hdr;
  prdb_text *texts;
  prdb_morph *morphs;
  prdb_lex *lexicon;
  const char *strings;
};


static void safe_copy(char *dst, size_t sz, const char *src) {
  snprintf(dst, sz, "%s", src ? src : "");
}

static inline const char *pool(const reader_ctx *ctx, uint32_t off) {
  return ctx->strings + off;
}


static const char *pt_pos[] = {
    ['n'] = "noun", ['v'] = "verb",     ['a'] = "adj",    ['d'] = "adv",
    ['l'] = "art",  ['g'] = "particle", ['c'] = "conj",   ['r'] = "prep",
    ['p'] = "pron", ['m'] = "num",      ['i'] = "interj", ['x'] = "irreg",
};

static const char *pt_person[] = {
    ['1'] = "1st",
    ['2'] = "2nd",
    ['3'] = "3rd",
};

static const char *pt_number[] = {
    ['s'] = "sg",
    ['p'] = "pl",
    ['d'] = "dual",
};

static const char *pt_tense[] = {
    ['p'] = "pres", ['i'] = "imperf", ['f'] = "fut",     ['a'] = "aor",
    ['r'] = "perf", ['l'] = "plup",   ['t'] = "futperf",
};

static const char *pt_mood[] = {
    ['i'] = "ind", ['s'] = "subj",  ['o'] = "opt",
    ['n'] = "inf", ['m'] = "imper", ['p'] = "ptcp",
};

static const char *pt_voice[] = {
    ['a'] = "act",
    ['p'] = "pass",
    ['m'] = "mid",
    ['e'] = "mp",
};

static const char *pt_gender[] = {
    ['m'] = "masc",
    ['f'] = "fem",
    ['n'] = "neut",
};

static const char *pt_case[] = {
    ['n'] = "nom", ['g'] = "gen", ['d'] = "dat", ['a'] = "acc", ['v'] = "voc",
};

static const char *pt_degree[] = {
    ['c'] = "comp",
    ['s'] = "super",
};

static const struct {
  const char **tbl;
  size_t len;
} postag_tables[] = {
    {pt_pos, countof(pt_pos)},       {pt_person, countof(pt_person)},
    {pt_number, countof(pt_number)}, {pt_tense, countof(pt_tense)},
    {pt_mood, countof(pt_mood)},     {pt_voice, countof(pt_voice)},
    {pt_gender, countof(pt_gender)}, {pt_case, countof(pt_case)},
    {pt_degree, countof(pt_degree)},
};

void reader_format_postag(const char *postag, char *out, size_t out_size) {
  if (!postag || strlen(postag) < countof(postag_tables)) {
    snprintf(out, out_size, "?");
    return;
  }

  char *p = out;
  char *end = out + out_size - 1;
  int first = 1;

  for (size_t i = 0; i < countof(postag_tables) && p < end; i++) {
    unsigned char ch = (unsigned char)postag[i];
    const char *label =
        (ch < postag_tables[i].len) ? postag_tables[i].tbl[ch] : nil;
    if (label) {
      if (!first && p < end)
        *p++ = ' ';
      int n = snprintf(p, (size_t)(end - p), "%s", label);
      if (n > 0)
        p += n;
      first = 0;
    }
  }
  *p = '\0';
}


reader_ctx *reader_open(const char *db_path) {
  _Static_assert(sizeof(prdb_text) == 8, "prdb_text packing");
  _Static_assert(sizeof(prdb_morph) == 12, "prdb_morph packing");
  _Static_assert(sizeof(prdb_lex) == 12, "prdb_lex packing");

  if (!db_path)
    db_path = "nitro:/lexis.dat";

  printf("  opening %s\n", db_path);

  FILE *f = fopen(db_path, "rb");
  if (!f) {
    printf("  fopen failed\n");
    return nil;
  }

  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);

  printf("  file size: %ld bytes\n", sz);

  if (sz < (long)sizeof(prdb_header)) {
    fclose(f);
    return nil;
  }

  uint8_t *data = (uint8_t *)malloc((size_t)sz);
  if (!data) {
    fclose(f);
    printf("  malloc(%ld) failed!\n", sz);
    return nil;
  }

  size_t rd = fread(data, 1, (size_t)sz, f);
  fclose(f);

  if ((long)rd != sz) {
    free(data);
    printf("  fread short: %zu / %ld\n", rd, sz);
    return nil;
  }

  printf("  loaded %ld bytes OK\n", sz);

  prdb_header *hdr = (prdb_header *)data;
  if (memcmp(hdr->magic, "PRDB", 4) != 0 || hdr->version != 1) {
    free(data);
    printf("  bad magic/version\n");
    return nil;
  }

  reader_ctx *ctx = (reader_ctx *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    free(data);
    return nil;
  }

  ctx->data = data;
  ctx->data_size = (size_t)sz;
  ctx->hdr = hdr;
  ctx->texts = (prdb_text *)(data + hdr->text_idx_off);
  ctx->morphs = (prdb_morph *)(data + hdr->morph_idx_off);
  ctx->lexicon = (prdb_lex *)(data + hdr->lex_idx_off);
  ctx->strings = (const char *)(data + hdr->strings_off);

  printf("  %lu texts, %lu morphs, %lu lex, %lu books\n",
         (unsigned long)hdr->num_texts, (unsigned long)hdr->num_morphs,
         (unsigned long)hdr->num_lex, (unsigned long)hdr->num_books);

  return ctx;
}

void reader_close(reader_ctx *ctx) {
  if (!ctx)
    return;
  free(ctx->data);
  free(ctx);
}


int reader_get_lines(reader_ctx *ctx, const char *work, int book,
                     int start_line, int count, reader_line *out) {
  (void)work;

  uint32_t num = ctx->hdr->num_texts;

  int lo = 0, hi = (int)num - 1;
  int pos = (int)num;

  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    int b = ctx->texts[mid].book;
    int l = ctx->texts[mid].line;

    if (b < book || (b == book && l < start_line)) {
      lo = mid + 1;
    } else {
      pos = mid;
      hi = mid - 1;
    }
  }

  int n = 0;
  for (int i = pos; i < (int)num && n < count; i++) {
    if (ctx->texts[i].book != (uint16_t)book)
      break;
    out[n].book = ctx->texts[i].book;
    out[n].line = ctx->texts[i].line;
    safe_copy(out[n].text, sizeof(out[n].text),
              pool(ctx, ctx->texts[i].text_off));
    n++;
  }
  return n;
}

int reader_book_count(reader_ctx *ctx, const char *work) {
  (void)work;
  return (int)ctx->hdr->num_books;
}

int reader_max_line(reader_ctx *ctx, const char *work, int book) {
  (void)work;
  if (book >= 1 && book < PRDB_MAX_BOOKS)
    return (int)ctx->hdr->book_max[book];
  return 0;
}


int reader_morph_lookup(reader_ctx *ctx, const char *form, reader_morph *out,
                        int max_results) {
  uint32_t num = ctx->hdr->num_morphs;
  int lo = 0, hi = (int)num - 1;
  int first = -1;

  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    int cmp = strcmp(form, pool(ctx, ctx->morphs[mid].form_off));
    if (cmp < 0)
      hi = mid - 1;
    else if (cmp > 0)
      lo = mid + 1;
    else {
      first = mid;
      hi = mid - 1;
    }
  }

  if (first < 0)
    return 0;

  int n = 0;
  for (int i = first; i < (int)num && n < max_results; i++) {
    if (strcmp(form, pool(ctx, ctx->morphs[i].form_off)) != 0)
      break;
    safe_copy(out[n].form, sizeof(out[n].form),
              pool(ctx, ctx->morphs[i].form_off));
    safe_copy(out[n].lemma, sizeof(out[n].lemma),
              pool(ctx, ctx->morphs[i].lemma_off));
    safe_copy(out[n].postag, sizeof(out[n].postag),
              pool(ctx, ctx->morphs[i].postag_off));
    reader_format_postag(out[n].postag, out[n].parse_str,
                         sizeof(out[n].parse_str));
    n++;
  }
  return n;
}


static int lex_bsearch(reader_ctx *ctx, const char *lemma,
                       reader_lex_entry *out, int max_results) {
  uint32_t num = ctx->hdr->num_lex;
  int lo = 0, hi = (int)num - 1;
  int first = -1;

  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    int cmp = strcmp(lemma, pool(ctx, ctx->lexicon[mid].lemma_off));
    if (cmp < 0)
      hi = mid - 1;
    else if (cmp > 0)
      lo = mid + 1;
    else {
      first = mid;
      hi = mid - 1;
    }
  }

  if (first < 0)
    return 0;

  int n = 0;
  for (int i = first; i < (int)num && n < max_results; i++) {
    if (strcmp(lemma, pool(ctx, ctx->lexicon[i].lemma_off)) != 0)
      break;
    safe_copy(out[n].lemma, sizeof(out[n].lemma),
              pool(ctx, ctx->lexicon[i].lemma_off));
    safe_copy(out[n].short_def, sizeof(out[n].short_def),
              pool(ctx, ctx->lexicon[i].short_def_off));
    safe_copy(out[n].definition, sizeof(out[n].definition),
              pool(ctx, ctx->lexicon[i].def_off));
    n++;
  }
  return n;
}

int reader_lex_lookup(reader_ctx *ctx, const char *lemma, reader_lex_entry *out,
                      int max_results) {
  int n = lex_bsearch(ctx, lemma, out, max_results);
  if (n > 0)
    return n;

  size_t len = strlen(lemma);
  if (len > 1 && lemma[len - 1] >= '0' && lemma[len - 1] <= '9') {
    char stripped[256];
    if (len < sizeof(stripped)) {
      memcpy(stripped, lemma, len);
      stripped[len] = '\0';
      size_t end = len;
      while (end > 0 && stripped[end - 1] >= '0' && stripped[end - 1] <= '9')
        end--;
      if (end > 0 && end < len) {
        stripped[end] = '\0';
        n = lex_bsearch(ctx, stripped, out, max_results);
      }
    }
  }
  return n;
}

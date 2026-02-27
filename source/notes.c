#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "notes.h"


enum {
  NOTES_MAGIC = 0x4E4F5445u,
};

static const char NOTES_PATH[] = "fat:/data/reader/notes.dat";

static note_entry_t notes[NOTES_MAX];
static int notes_count;
static int notes_dirty;


static void ensure_dir(const char *path) {
  char tmp[128];
  strncpy(tmp, path, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
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

static int find_index(const char *key) {
  for (int i = 0; i < notes_count; i++) {
    if (strcmp(notes[i].key, key) == 0)
      return i;
  }
  return -1;
}


void notes_load(void) {
  notes_count = 0;
  notes_dirty = 0;

  FILE *f = fopen(NOTES_PATH, "rb");
  if (!f)
    return;

  uint32_t magic = 0;
  uint32_t count = 0;
  if (fread(&magic, 4, 1, f) != 1 || magic != NOTES_MAGIC) {
    fclose(f);
    return;
  }
  if (fread(&count, 4, 1, f) != 1) {
    fclose(f);
    return;
  }
  if (count > NOTES_MAX)
    count = NOTES_MAX;

  int n = (int)fread(notes, sizeof(note_entry_t), count, f);
  notes_count = n;
  fclose(f);
}

void notes_save(void) {
  if (!notes_dirty)
    return;

  ensure_dir(NOTES_PATH);
  FILE *f = fopen(NOTES_PATH, "wb");
  if (!f)
    return;

  uint32_t magic = NOTES_MAGIC;
  uint32_t count = (uint32_t)notes_count;
  fwrite(&magic, 4, 1, f);
  fwrite(&count, 4, 1, f);
  fwrite(notes, sizeof(note_entry_t), (size_t)notes_count, f);
  fclose(f);

  notes_dirty = 0;
}

const char *notes_find(const char *key) {
  int idx = find_index(key);
  if (idx < 0)
    return NULL;
  return notes[idx].text;
}

int notes_get_count(void) { return notes_count; }

int notes_data_size(void) {
  if (notes_count == 0)
    return 0;
  return 8 + notes_count * (int)sizeof(note_entry_t);
}

void notes_set(const char *key, const char *text) {
  if (!text || text[0] == '\0') {
    int idx = find_index(key);
    if (idx >= 0) {
      notes_count--;
      if (idx < notes_count)
        notes[idx] = notes[notes_count];
      notes_dirty = 1;
    }
    return;
  }

  int idx = find_index(key);
  if (idx >= 0) {
    strncpy(notes[idx].text, text, NOTE_TEXT_MAX - 1);
    notes[idx].text[NOTE_TEXT_MAX - 1] = '\0';
    notes_dirty = 1;
    return;
  }

  if (notes_count >= NOTES_MAX)
    return;
  strncpy(notes[notes_count].key, key, NOTE_KEY_MAX - 1);
  notes[notes_count].key[NOTE_KEY_MAX - 1] = '\0';
  strncpy(notes[notes_count].text, text, NOTE_TEXT_MAX - 1);
  notes[notes_count].text[NOTE_TEXT_MAX - 1] = '\0';
  notes_count++;
  notes_dirty = 1;
}

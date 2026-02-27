#pragma once

enum {
  NOTE_KEY_MAX = 64,
  NOTE_TEXT_MAX = 256,
  NOTES_MAX = 256, /* TODO: derive from word coverage + margin */
};

typedef struct {
  char key[NOTE_KEY_MAX];
  char text[NOTE_TEXT_MAX];
} note_entry_t;

void notes_load(void);
void notes_save(void);
const char *notes_find(const char *key);
int notes_get_count(void);
int notes_data_size(void);
void notes_set(const char *key, const char *text);

#!/usr/bin/env python3
"""
output binary format (all integers little-endian):

  HEADER  (160 bytes)
    magic[4]        "PRDB"
    version         u32  = 1
    num_texts       u32
    num_morphs      u32
    num_lex         u32
    num_books       u32
    text_idx_off    u32  — offset to text index
    morph_idx_off   u32  — offset to morph index
    lex_idx_off     u32  — offset to lex index
    strings_off     u32  — offset to string pool
    book_max[30]    u32 × 30  — max line per book (1-indexed)

  TEXT INDEX  (num_texts × 8 bytes, sorted by book, line)
    book            u16
    line            u16
    text_off        u32  — offset into string pool

  MORPH INDEX  (num_morphs × 12 bytes, sorted by form bytes)
    form_off        u32
    lemma_off       u32
    postag_off      u32

  LEX INDEX  (num_lex × 12 bytes, sorted by lemma bytes)
    lemma_off       u32
    short_def_off   u32
    def_off         u32

  STRING POOL
    null-terminated UTF-8 strings, concatenated.
    offset 0 is always the empty string "\\0".
"""

import sqlite3
import struct
import sys
import os


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.db> <output.dat> [--skip-defs]")
        sys.exit(1)

    db_path  = sys.argv[1]
    out_path = sys.argv[2]
    skip_defs = "--skip-defs" in sys.argv

    db = sqlite3.connect(db_path)

    # ── string pool with deduplication ──────────────────────
    pool = bytearray(b"\x00")   # offset 0 = empty string
    seen = {"": 0}

    def intern(s):
        """return the offset of `s` in the string pool, adding it if new."""
        s = s or ""
        if s in seen:
            return seen[s]
        off = len(pool)
        seen[s] = off
        pool.extend(s.encode("utf-8"))
        pool.append(0)
        return off

    rows = db.execute(
        "SELECT book, line, greek FROM texts ORDER BY book, line"
    ).fetchall()

    text_entries = []
    book_max = [0] * 30
    max_book = 0
    for book, line, greek in rows:
        text_entries.append((book, line, intern(greek)))
        if 1 <= book < 30:
            book_max[book] = max(book_max[book], line)
        max_book = max(max_book, book)
    num_books = max_book

    print(f"  texts:   {len(text_entries)} lines, {num_books} books")

    rows = db.execute(
        "SELECT form, lemma, postag FROM morphology"
    ).fetchall()

    rows.sort(key=lambda r: (r[0] or "").encode("utf-8"))

    morph_entries = []
    for form, lemma, postag in rows:
        morph_entries.append((
            intern(form  or ""),
            intern(lemma or ""),
            intern(postag or ""),
        ))

    print(f"  morphs:  {len(morph_entries)} entries")

    rows = db.execute(
        "SELECT lemma, short_def, definition FROM lexicon"
    ).fetchall()

    rows.sort(key=lambda r: (r[0] or "").encode("utf-8"))

    lex_entries = []
    for lemma, short_def, definition in rows:
        lex_entries.append((
            intern(lemma     or ""),
            intern(short_def or ""),
            0 if skip_defs else intern(definition or ""),
        ))

    print(f"  lexicon: {len(lex_entries)} entries")

    # section offsets
    HEADER_SIZE   = 4 + 9 * 4 + 30 * 4   # 160 bytes
    text_idx_off  = HEADER_SIZE
    morph_idx_off = text_idx_off  + len(text_entries)  * 8
    lex_idx_off   = morph_idx_off + len(morph_entries)  * 12
    strings_off   = lex_idx_off   + len(lex_entries)    * 12

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)

    with open(out_path, "wb") as f:
        # header
        f.write(b"PRDB")
        f.write(struct.pack("<I", 1))                   # version
        f.write(struct.pack("<I", len(text_entries)))   # num_texts
        f.write(struct.pack("<I", len(morph_entries)))  # num_morphs
        f.write(struct.pack("<I", len(lex_entries)))    # num_lex
        f.write(struct.pack("<I", num_books))           # num_books
        f.write(struct.pack("<I", text_idx_off))
        f.write(struct.pack("<I", morph_idx_off))
        f.write(struct.pack("<I", lex_idx_off))
        f.write(struct.pack("<I", strings_off))
        for i in range(30):
            f.write(struct.pack("<I", book_max[i]))
        assert f.tell() == HEADER_SIZE

        # text index
        for book, line, toff in text_entries:
            f.write(struct.pack("<HHI", book, line, toff))
        assert f.tell() == morph_idx_off

        # morph. index
        for foff, loff, poff in morph_entries:
            f.write(struct.pack("<III", foff, loff, poff))
        assert f.tell() == lex_idx_off

        # lex index
        for loff, soff, doff in lex_entries:
            f.write(struct.pack("<III", loff, soff, doff))
        assert f.tell() == strings_off

        # string pool
        f.write(bytes(pool))

    total = strings_off + len(pool)
    print()
    print(f"Generated {out_path}")
    print(f"  Texts:    {len(text_entries):>6}")
    print(f"  Morphs:   {len(morph_entries):>6}")
    print(f"  Lexicon:  {len(lex_entries):>6}")
    print(f"  Strings:  {len(pool):>6} bytes  ({len(seen)} unique)")
    print(f"  Total:    {total:>6} bytes  ({total / 1024 / 1024:.2f} MB)")
    if skip_defs:
        print("  (full definitions skipped)")

    # coverage
    lex_lemmas = set()
    for lemma, _, _ in db.execute("SELECT lemma, short_def, definition FROM lexicon"):
        if lemma:
            lex_lemmas.add(lemma)

    morph_lemmas = set()
    for _, lemma, _ in db.execute("SELECT form, lemma, postag FROM morphology"):
        if lemma:
            # strip trailing digits (treebank convention, e.g. μῆνις1)
            stripped = lemma.rstrip("0123456789")
            morph_lemmas.add(stripped or lemma)

    missing = morph_lemmas - lex_lemmas
    covered = morph_lemmas & lex_lemmas
    pct = 100 * len(covered) / len(morph_lemmas) if morph_lemmas else 0

    print()
    print(f"  Coverage: {len(covered)}/{len(morph_lemmas)} lemmas have definitions ({pct:.1f}%)")
    print(f"  Missing:  {len(missing)} lemmas without definitions")
    if missing:
        sample = sorted(missing)[:20]
        print(f"  Sample:   {', '.join(sample)}")
        if len(missing) > 20:
            print(f"            ... and {len(missing) - 20} more")


if __name__ == "__main__":
    main()

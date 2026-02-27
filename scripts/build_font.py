#!/usr/bin/env python3
"""
binary format (little-endian):
  HEADER (16 bytes)
    magic[4]        "PFNT"
    glyph_w         u8   — max glyph width (pixels)
    glyph_h         u8   — glyph height (pixels)
    num_glyphs      u16  — number of glyphs in atlas
    baseline        u8   — baseline offset from top
    padding[7]      reserved (0)

  GLYPH TABLE (num_glyphs × 8 bytes, sorted by codepoint)
    codepoint       u32  — Unicode codepoint
    advance         u8   — horizontal advance (pixels)
    bearing_x       i8   — left bearing (signed)
    atlas_row       u16  — row index into bitmap data

  BITMAP DATA (num_glyphs × glyph_h × ceil(glyph_w/8) bytes)
    each row is packed 1-bit, MSB-first, padded to byte boundary.
    one glyph = glyph_h rows × row_bytes bytes.
"""

import sqlite3
import struct
import sys
import os
import math

from PIL import Image, ImageDraw, ImageFont


def get_codepoints(db_path):
    """collect all unique codepoints from the DB."""
    db = sqlite3.connect(db_path)
    chars = set()

    for (text,) in db.execute("SELECT greek FROM texts"):
        chars.update(text)
    for (form, lemma) in db.execute("SELECT form, lemma FROM morphology"):
        if form:
            chars.update(form)
        if lemma:
            chars.update(lemma)
    for (sd,) in db.execute(
        "SELECT short_def FROM lexicon WHERE short_def IS NOT NULL"
    ):
        chars.update(sd)
    db.close()

    # always include printable ASCII
    for i in range(0x20, 0x7F):
        chars.add(chr(i))

    # remove control chars, keep only printable
    chars = {c for c in chars if ord(c) >= 0x20}
    return sorted(chars, key=ord)


def render_glyph(font, char, glyph_h, max_w):
    """render a single character and return (advance, bitmap_rows).

    Pillow draws text so that the baseline sits at y = ascent
    (from font.getmetrics()).  we render into a (max_w × glyph_h)
    image at position (0, 0) — Pillow handles vertical placement.
    """
    row_bytes = math.ceil(max_w / 8)

    bbox = font.getbbox(char)
    if bbox is None:
        return 0, [bytes(row_bytes)] * glyph_h

    # width++
    try:
        advance = int(round(font.getlength(char)))
    except Exception:
        advance = bbox[2] - bbox[0]

    # render
    img = Image.new("1", (max_w, glyph_h), 0)
    draw = ImageDraw.Draw(img)
    draw.text((0, 0), char, fill=1, font=font)

    # convert to packed rows (already 1-bit)
    bitmap_rows = []
    pixels = img.load()
    for y in range(glyph_h):
        row = bytearray(row_bytes)
        for x in range(max_w):
            if pixels[x, y]:
                row[x >> 3] |= 0x80 >> (x & 7)
        bitmap_rows.append(bytes(row))

    return advance, bitmap_rows


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Build NDS bitmap font")
    parser.add_argument("--size", type=int, default=10, help="Font size in px")
    parser.add_argument(
        "--font",
        default="/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        help="TTF font path",
    )
    parser.add_argument("--db", default="data/perseus.db", help="Perseus DB")
    parser.add_argument("--out", default=None, help="Output path (default: nds/data/font_{size}.bin)")
    args = parser.parse_args()

    if args.out is None:
        args.out = f"nds/data/font_{args.size}.bin"

    print(f"Font: {args.font}")
    print(f"Size: {args.size}px")

    font = ImageFont.truetype(args.font, args.size)

    # get all needed codepoints
    codepoints = get_codepoints(args.db)
    chars = [chr(cp) if isinstance(cp, int) else cp for cp in codepoints]
    print(f"Codepoints: {len(chars)}")

    # determine glyph cell from font metrics
    ascent, descent = font.getmetrics()
    glyph_h = ascent + descent

    # find maximum glyph width across all characters
    max_w = 0
    for ch in chars:
        bbox = font.getbbox(ch)
        if bbox:
            max_w = max(max_w, bbox[2])  # right edge
    max_w = min(max_w + 1, 16)  # +1 padding, cap at 16

    baseline = ascent

    print(f"Glyph cell: {max_w}x{glyph_h}, baseline at {baseline}")

    row_bytes = math.ceil(max_w / 8)

    # render all glyphs
    glyphs = []
    for ch in chars:
        cp = ord(ch)
        advance, bitmap = render_glyph(font, ch, glyph_h, max_w)
        glyphs.append((cp, advance, bitmap))

    # sort by codepoint (should already be sorted)
    glyphs.sort(key=lambda g: g[0])

    # write binary
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)

    with open(args.out, "wb") as f:
        # Header: 16 bytes
        f.write(b"PFNT")
        f.write(struct.pack("<B", max_w))       # glyph_w
        f.write(struct.pack("<B", glyph_h))     # glyph_h
        f.write(struct.pack("<H", len(glyphs))) # num_glyphs
        f.write(struct.pack("<B", baseline))    # baseline
        f.write(b"\x00" * 7)                    # padding
        assert f.tell() == 16

        # glyph table
        for i, (cp, advance, _) in enumerate(glyphs):
            f.write(struct.pack("<I", cp))
            f.write(struct.pack("<B", min(advance, 255)))
            f.write(struct.pack("<b", 0))        # reserved
            f.write(struct.pack("<H", i))        # atlas_row = glyph index
        assert f.tell() == 16 + len(glyphs) * 8

        # bitmap data
        for _, _, bitmap in glyphs:
            for row in bitmap:
                f.write(row[:row_bytes])

    total = 16 + len(glyphs) * 8 + len(glyphs) * glyph_h * row_bytes
    print(f"\nGenerated {args.out}")
    print(f"  {len(glyphs)} glyphs, {max_w}x{glyph_h} cell")
    print(f"  {total} bytes ({total/1024:.1f} KB)")


if __name__ == "__main__":
    main()

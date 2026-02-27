# lexis-nds

a nintendo dsi greek text reader with an inline lexicon for word lookup. designed to work with any annotated greek corpus — the current build targets Homer's Iliad as the MVP.

## features

- reader with paginated greek text
- tap a word to look it up in the lexicon
- notes and hand-drawn annotations
- configurable font family and size (Gentium Plus, Cardo, DejaVu Sans)
- configurable color palette
- saves progress and notes to the sd card

## building

### prerequisites

`docker just`

### data pipeline

download and process the Perseus corpus, then build the font binaries:

```sh
just data
```

this runs:
1. `build_db.py` — downloads Perseus XML, builds a SQLite database
2. `build_flatdb.py` — converts the DB to a compact flat binary (`romfs/lexis.dat`)
3. `build_font.py` — rasterises TTF fonts into NDS-friendly bitmaps

### rom

```sh
just
```

### upload to hardware

```sh
just upload
```

pushes the ROM to an HTTP server on the local network (edit the IP in `justfile` to match your setup).
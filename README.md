# lexis-nds

a nintendo dsi greek text reader with an inline lexicon for word lookup.

## features

- reader with paginated greek text
- tap a word to look it up in the lexicon
- notes and hand-drawn annotations
- configurable font family and size (Gentium Plus, Cardo, DejaVu Sans)
- configurable color palette
- saves progress and notes to the sd card

## releases

prebuilt ROMs are available on the [releases page](../../releases). current corpora:

- **lexis-iliad.nds** — Homer, Iliad
- **lexis-anabasis.nds** — Xenophon, Anabasis

## building

### prerequisites

`docker just python3`

### iliad

```sh
just iliad-all
```

### anabasis

```sh
just anabasis-all
```

each pipeline runs:
1. `build_db.py` — downloads Perseus XML and treebank data, builds a SQLite database
2. `build_flatdb.py` — converts the DB to a compact flat binary (`romfs/lexis.dat`)
3. `build_font.py` — rasterises TTF fonts into NDS-friendly bitmaps
4. `docker run ... make` — compiles the ROM inside the BlocksDS container

### upload to hardware

```sh
just upload          # iliad (default)
just upload anabasis
```

pushes the ROM to an HTTP server on the local network (edit the IP in `justfile` to match your setup).

to pull and launch the ROM directly on your DSi over WiFi, use [ndsfetch](https://github.com/no382001/ndsfetch).

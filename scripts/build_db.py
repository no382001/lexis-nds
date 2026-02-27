#!/usr/bin/env python3

import os
import re
import sys
import sqlite3
import subprocess
import unicodedata
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT    = Path(__file__).resolve().parent.parent
DATA    = ROOT / "data"
DB_PATH = DATA / "perseus.db"

TEI     = "{http://www.tei-c.org/ns/1.0}"

_BETA_LOWER = {
    'a': 'α', 'b': 'β', 'g': 'γ', 'd': 'δ', 'e': 'ε',
    'z': 'ζ', 'h': 'η', 'q': 'θ', 'i': 'ι', 'k': 'κ',
    'l': 'λ', 'm': 'μ', 'n': 'ν', 'c': 'ξ', 'o': 'ο',
    'p': 'π', 'r': 'ρ', 's': 'σ', 'w': 'ω', 't': 'τ',
    'u': 'υ', 'f': 'φ', 'x': 'χ', 'y': 'ψ', 'v': 'ϝ',
    'j': 'ς',
}

_BETA_DIACRITICS = {
    ')': '\u0313',  # smooth breathing (psili)
    '(': '\u0314',  # rough breathing (dasia)
    '/': '\u0301',  # acute accent (oxia)
    '\\': '\u0300', # grave accent (varia)
    '=': '\u0342',  # circumflex (perispomeni)
    '+': '\u0308',  # diaeresis
    '|': '\u0345',  # iota subscript (ypogegrammeni)
}

def beta_to_unicode(beta):
    """convert Perseus beta-code string to Unicode Greek.

    handles lowercase/uppercase, breathing, accents, iota subscript,
    final sigma. returns original string if it already contains Greek.
    """
    if not beta:
        return beta

    # if already unicode, return as-is
    if any('\u0370' <= c <= '\u03FF' or '\u1F00' <= c <= '\u1FFF' for c in beta):
        return beta

    result = []
    i = 0
    s = beta
    length = len(s)
    capitalize_next = False

    while i < length:
        ch = s[i]

        # capital marker
        if ch == '*':
            capitalize_next = True
            i += 1
            # diacritics can appear between * and the letter
            diac = []
            while i < length and s[i] in _BETA_DIACRITICS:
                diac.append(_BETA_DIACRITICS[s[i]])
                i += 1
            if i < length and s[i].lower() in _BETA_LOWER:
                base = _BETA_LOWER[s[i].lower()].upper()
                # collect any trailing diacritics too
                i += 1
                while i < length and s[i] in _BETA_DIACRITICS:
                    diac.append(_BETA_DIACRITICS[s[i]])
                    i += 1
                result.append(base + ''.join(diac))
            else:
                result.extend(diac)
            capitalize_next = False
            continue

        # diacritics after a letter
        if ch in _BETA_DIACRITICS:
            result.append(_BETA_DIACRITICS[ch])
            i += 1
            continue

        # regular letter
        lower = ch.lower()
        if lower in _BETA_LOWER:
            base = _BETA_LOWER[lower]
            if capitalize_next or ch.isupper():
                base = base.upper()
                capitalize_next = False
            i += 1

            # collect following diacritics
            diac = []
            while i < length and s[i] in _BETA_DIACRITICS:
                diac.append(_BETA_DIACRITICS[s[i]])
                i += 1

            result.append(base + ''.join(diac))
            continue

        # sigma might be final sigma at end of word
        # (already handled above as 's' → 'σ')
        # punctuation / other chars pass through
        result.append(ch)
        i += 1

    # fix final sigma
    text = ''.join(result)
    text = re.sub(r'σ(?=\s|$|[,.:;!?\-\]\)}>"\'])', 'ς', text)
    text = re.sub(r'Σ(?=\s|$|[,.:;!?\-\]\)}>"\'])', 'Σ', text)  # keep capital

    # normalize to NFC for consistent matching
    return unicodedata.normalize('NFC', text)

REPOS = {
    "texts":    "https://github.com/PerseusDL/canonical-greekLit.git",
    "treebank": "https://github.com/PerseusDL/treebank_data.git",
    "shortdefs": "https://github.com/helmadik/shortdefs.git",
    "LSJLogeion": "https://github.com/helmadik/LSJLogeion.git",
}

def download_repos(skip=False):
    """shallow-clone Perseus repos if not already present."""
    if skip:
        print("  (skipping download, using existing data/)")
        return
    DATA.mkdir(exist_ok=True)
    for name, url in REPOS.items():
        target = DATA / name
        if target.exists():
            print(f"  {name}: already downloaded")
            continue
        print(f"  {name}: cloning (shallow)...")
        subprocess.run(
            ["git", "clone", "--depth", "1", url, str(target)],
            check=True,
        )
    print()

def create_schema(conn):
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS texts (
            id      INTEGER PRIMARY KEY AUTOINCREMENT,
            work    TEXT NOT NULL,
            book    INTEGER NOT NULL,
            line    INTEGER NOT NULL,
            greek   TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS morphology (
            id      INTEGER PRIMARY KEY AUTOINCREMENT,
            form    TEXT NOT NULL,
            lemma   TEXT NOT NULL,
            postag  TEXT
        );

        CREATE TABLE IF NOT EXISTS lexicon (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            lemma       TEXT NOT NULL,
            short_def   TEXT,
            definition  TEXT
        );
    """)

def create_indexes(conn):
    """create indexes after bulk insert for speed."""
    print("  Creating indexes...")
    conn.executescript("""
        CREATE INDEX IF NOT EXISTS idx_texts_loc   ON texts(work, book, line);
        CREATE INDEX IF NOT EXISTS idx_morph_form  ON morphology(form);
        CREATE INDEX IF NOT EXISTS idx_morph_lemma ON morphology(lemma);
        CREATE INDEX IF NOT EXISTS idx_lex_lemma   ON lexicon(lemma);
    """)

def normalize_lemma(s):
    """canonical form for lemma matching: NFC, strip leading combining/modifier
    chars (e.g. coronis ʼ U+02BC), strip trailing digits, lowercase."""
    s = unicodedata.normalize("NFC", s)
    # strip leading combining marks and modifier letters (U+02B0–U+02FF)
    while s and (unicodedata.category(s[0]).startswith("M")
                  or 0x02B0 <= ord(s[0]) <= 0x02FF):
        s = s[1:]
    s = re.sub(r"\d+$", "", s)
    return s.lower()

def tag_local(elem):
    """strip namespace from an element tag."""
    t = elem.tag
    return t.split("}")[-1] if "}" in t else t

def get_text(elem):
    """all text content from an element tree, concatenated."""
    return "".join(elem.itertext()).strip()

def find_iliad_xml():
    """locate the Iliad TEI XML in canonical-greekLit."""
    texts = DATA / "texts"

    for candidate in [
        "data/tlg0012/tlg001/tlg0012.tlg001.perseus-grc2.xml",
        "data/tlg0012/tlg001/tlg0012.tlg001.perseus-grc1.xml",
    ]:
        p = texts / candidate
        if p.exists():
            return p
    
    # fallback: search
    for f in sorted(texts.rglob("tlg0012.tlg001.*grc*.xml")):
        return f
    return None

def import_iliad(conn):
    """parse Iliad TEI XML -> texts table."""
    print("\n  Importing Iliad text...")

    xml_path = find_iliad_xml()
    if not xml_path:
        print("    ERROR: Iliad XML not found!")
        return 0

    print(f"    Source: {xml_path.name}")

    tree = ET.parse(str(xml_path))
    root = tree.getroot()

    cur = conn.cursor()
    count = 0
    last_book = 0

    # walk the tree looking for book-level divs
    for div in root.iter():
        if tag_local(div) != "div":
            continue

        subtype = (div.get("subtype") or div.get("type") or "").lower()
        if subtype not in ("book",):
            continue

        book_str = div.get("n", "")
        try:
            book_num = int(book_str)
        except ValueError:
            continue

        # collect all <l> (verse line) elements within this book
        for elem in div.iter():
            if tag_local(elem) != "l":
                continue
            line_str = elem.get("n", "")
            try:
                line_num = int(line_str)
            except ValueError:
                continue

            text = get_text(elem)
            if not text:
                continue

            cur.execute(
                "INSERT INTO texts (work, book, line, greek) VALUES (?,?,?,?)",
                ("iliad", book_num, line_num, text),
            )
            count += 1
            last_book = max(last_book, book_num)

    conn.commit()
    print(f"    {count} lines across {last_book} books")
    return count

def find_treebank_files():
    """locate Iliad treebank XML files."""
    tb = DATA / "treebank"
    files = []

    # tlg0012.tlg001 = Homer, Iliad
    for f in sorted(tb.rglob("*.xml")):
        if "tlg0012" in f.name and "tlg001" in f.name:
            files.append(f)

    if not files:
        # broader search: any .tb.xml
        for f in sorted(tb.rglob("*.tb.xml")):
            # check inside for Homer references
            files.append(f)

    return files

def import_morphology(conn):
    """parse treebank XML → morphology table (form, lemma, postag)."""
    print("\n  Importing morphology...")

    files = find_treebank_files()
    if not files:
        print("    WARNING: No treebank files found for the Iliad")
        return 0

    print(f"    Found {len(files)} treebank file(s)")

    cur = conn.cursor()
    seen = set()
    count = 0

    for fpath in files:
        try:
            tree = ET.parse(str(fpath))
        except ET.ParseError as e:
            print(f"    Warning: skip {fpath.name}: {e}")
            continue

        root = tree.getroot()

        for elem in root.iter():
            if tag_local(elem) != "word":
                continue

            form   = unicodedata.normalize('NFC', (elem.get("form") or "").strip())
            lemma  = normalize_lemma((elem.get("lemma") or "").strip())
            postag = (elem.get("postag") or "").strip()

            if not form or not lemma:
                continue
            # skip punctuation
            if postag and postag[0] == "u":
                continue

            key = (form, lemma, postag)
            if key in seen:
                continue
            seen.add(key)

            cur.execute(
                "INSERT INTO morphology (form, lemma, postag) VALUES (?,?,?)",
                (form, lemma, postag or None),
            )
            count += 1

    conn.commit()
    print(f"    {count} unique analyses")
    return count


def import_lexicon(conn):
    """import lexicon from Logeion shortdefs + LSJLogeion XML.

    primary source: helmadik/shortdefs
    secondary source: helmadik/LSJLogeion
    """
    print("\n  Importing Logeion lexicon...")

    shortdefs_path = DATA / "shortdefs" / "shortdefsGreekEnglishLogeion"
    if not shortdefs_path.exists():
        print("    WARNING: shortdefs file not found")
        return 0

    shortdefs = {}
    with open(shortdefs_path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            parts = line.split("\t", 1)
            if len(parts) == 2:
                lemma = normalize_lemma(parts[0].strip())
                defn  = parts[1].strip()
                if lemma and defn:
                    shortdefs[lemma] = defn

    print(f"    Loaded {len(shortdefs)} short definitions")

    lsj_dir = DATA / "LSJLogeion"
    full_defs = {}
    if lsj_dir.is_dir():
        xml_files = sorted(lsj_dir.glob("greatscott*.xml"))
        print(f"    Found {len(xml_files)} LSJLogeion XML file(s)")

        for i, fpath in enumerate(xml_files):
            if (i + 1) % 20 == 0 or i == len(xml_files) - 1:
                print(f"    Processing {i+1}/{len(xml_files)}: {fpath.name}")
            try:
                tree = ET.parse(str(fpath))
            except ET.ParseError as e:
                print(f"    Warning: skip {fpath.name}: {e}")
                continue

            for elem in tree.getroot().iter():
                if elem.tag != "div2":
                    continue

                headword = ""
                for child in elem.iter():
                    if child.tag == "orth":
                        headword = get_text(child)
                        break
                if not headword:
                    headword = elem.get("n", "")
                if not headword:
                    continue

                headword = normalize_lemma(headword.strip())
                full_text = get_text(elem)
                if len(full_text) > 8000:
                    full_text = full_text[:8000] + "..."
                if full_text:
                    full_defs[headword] = full_text

        print(f"    Loaded {len(full_defs)} full definitions from LSJLogeion")
    else:
        print("    LSJLogeion not found, using shortdefs only")

    cur = conn.cursor()
    count = 0

    # insert all shortdefs entries, with full def if available
    all_lemmas = set(shortdefs.keys()) | set(full_defs.keys())
    for lemma in all_lemmas:
        short = shortdefs.get(lemma)
        full  = full_defs.get(lemma)
        # only insert if we have at least a short def
        if not short and not full:
            continue
        cur.execute(
            "INSERT INTO lexicon (lemma, short_def, definition) VALUES (?,?,?)",
            (lemma, short, full),
        )
        count += 1

    conn.commit()
    print(f"    {count} lexicon entries (merged)")
    return count

def main():
    skip = "--skip-download" in sys.argv

    print("=" * 50)
    print("  Perseus Database Builder")
    print("=" * 50)
    print()

    download_repos(skip=skip)

    if DB_PATH.exists():
        DB_PATH.unlink()
    DATA.mkdir(exist_ok=True)

    conn = sqlite3.connect(str(DB_PATH))
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=OFF")

    create_schema(conn)

    texts  = import_iliad(conn)
    morphs = import_morphology(conn)
    lexent = import_lexicon(conn)

    # prune lexicon to only entries referenced by the morphology data.
    # this keeps the DB lean
    print("\n  Pruning lexicon to MVP vocabulary...")
    cur = conn.cursor()

    # build set of normalized lemma forms we need
    needed_norm = set()
    for (lemma,) in cur.execute("SELECT DISTINCT lemma FROM morphology"):
        needed_norm.add(normalize_lemma(lemma))

    # delete lexicon entries whose normalized lemma isn't needed
    before = cur.execute("SELECT COUNT(*) FROM lexicon").fetchone()[0]
    cur.execute("SELECT rowid, lemma FROM lexicon")
    to_delete = [rowid for rowid, lemma in cur.fetchall()
                 if normalize_lemma(lemma) not in needed_norm]
    for rowid in to_delete:
        cur.execute("DELETE FROM lexicon WHERE rowid=?", (rowid,))
    conn.commit()
    after = cur.execute("SELECT COUNT(*) FROM lexicon").fetchone()[0]
    lexent = after
    print(f"    Kept {after} of {before} entries (dropped {before - after})")

    create_indexes(conn)

    print("\n  Compacting database...")
    conn.execute("PRAGMA journal_mode=DELETE")
    conn.execute("VACUUM")
    conn.close()

    size_mb = DB_PATH.stat().st_size / (1024 * 1024)

    print()
    print("=" * 50)
    print(f"  Database: {DB_PATH}")
    print(f"  Size:     {size_mb:.1f} MB")
    print(f"  Texts:    {texts} lines")
    print(f"  Morph:    {morphs} analyses")
    print(f"  Lexicon:  {lexent} entries")
    print("=" * 50)
    print("  Done!")
    print()

if __name__ == "__main__":
    main()

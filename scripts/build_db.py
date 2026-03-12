#!/usr/bin/env python3

import os
import re
import sys
import sqlite3
import subprocess
import unicodedata
import xml.etree.ElementTree as ET
from pathlib import Path

import yaml

ROOT    = Path(__file__).resolve().parent.parent
DATA    = ROOT / "data"

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
    "gorman":   "https://github.com/perseids-publications/gorman-trees.git",
    "vgorman":  "https://github.com/vgorman1/Greek-Dependency-Trees.git",
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


def find_text_xml(author, work):
    """locate TEI XML for the given TLG author/work IDs."""
    texts = DATA / "texts"
    for suffix in ["2", "1", ""]:
        p = texts / f"data/{author}/{work}/{author}.{work}.perseus-grc{suffix}.xml"
        if p.exists():
            return p
    for f in sorted(texts.rglob(f"{author}.{work}.*grc*.xml")):
        return f
    return None

def find_treebank_files(author, work, repo="treebank", pattern=None):
    """locate treebank XML files for the given TLG author/work IDs.

    repo:    key in REPOS / subdirectory under data/ to search.
    pattern: optional filename glob substring for repos without TLG IDs in names.

    Three strategies tried in order:
    1. explicit pattern match (when corpora.yml supplies one)
    2. filename match: TLG IDs in filename (Perseus treebank_data style)
    3. document_id scan: peek at first sentence in each file (Gorman perseids style)
    """
    tb = DATA / repo

    if pattern:
        pat = pattern.lower()
        return [f for f in sorted(tb.rglob("*.xml")) if pat in f.name.lower()]

    by_name = [f for f in sorted(tb.rglob("*.xml"))
               if author in f.name and work in f.name]
    if by_name:
        return by_name

    # scan document_id in first sentence of each file
    matches = []
    for path in sorted(tb.rglob("*.xml")):
        try:
            for _, elem in ET.iterparse(str(path), events=("start",)):
                local = elem.tag.split("}")[-1] if "}" in elem.tag else elem.tag
                if local == "sentence":
                    doc_id = elem.get("document_id", "")
                    if author in doc_id and work in doc_id:
                        matches.append(path)
                    break
        except ET.ParseError:
            pass
    return sorted(matches)


def import_verse(conn, work_name, xml_path):
    """parse verse TEI XML (book + <l> lines) -> texts table."""
    print(f"    Source: {xml_path.name}")
    tree = ET.parse(str(xml_path))
    root = tree.getroot()

    cur = conn.cursor()
    count = 0
    last_book = 0

    for div in root.iter():
        if tag_local(div) != "div":
            continue
        subtype = (div.get("subtype") or div.get("type") or "").lower()
        if subtype != "book":
            continue
        try:
            book_num = int(div.get("n", ""))
        except ValueError:
            continue

        for elem in div.iter():
            if tag_local(elem) != "l":
                continue
            try:
                line_num = int(elem.get("n", ""))
            except ValueError:
                continue
            text = get_text(elem)
            if not text:
                continue
            cur.execute(
                "INSERT INTO texts (work, book, line, greek) VALUES (?,?,?,?)",
                (work_name, book_num, line_num, text),
            )
            count += 1
            last_book = max(last_book, book_num)

    conn.commit()
    print(f"    {count} lines across {last_book} books")
    return count

def import_prose(conn, work_name, xml_path):
    """parse prose TEI XML (book/chapter/section divs) -> texts table.

    Lines are numbered sequentially (1, 2, 3 ...) within each book.
    The chapter.section reference is prepended to the text as "[ch.sec] ...".
    """
    print(f"    Source: {xml_path.name}")
    tree = ET.parse(str(xml_path))
    root = tree.getroot()

    cur = conn.cursor()
    count = 0
    last_book = 0

    for book_div in root.iter():
        if tag_local(book_div) != "div":
            continue
        if (book_div.get("subtype") or book_div.get("type") or "").lower() != "book":
            continue
        try:
            book_num = int(book_div.get("n", ""))
        except ValueError:
            continue

        seq = 0
        for ch_div in book_div:
            if tag_local(ch_div) != "div":
                continue
            if (ch_div.get("subtype") or ch_div.get("type") or "").lower() != "chapter":
                continue
            try:
                ch_num = int(ch_div.get("n", ""))
            except ValueError:
                continue

            for sec_div in ch_div:
                if tag_local(sec_div) != "div":
                    continue
                if (sec_div.get("subtype") or sec_div.get("type") or "").lower() != "section":
                    continue
                try:
                    sec_num = int(sec_div.get("n", ""))
                except ValueError:
                    continue
                text = get_text(sec_div)
                if not text:
                    continue
                seq += 1
                cur.execute(
                    "INSERT INTO texts (work, book, line, greek) VALUES (?,?,?,?)",
                    (work_name, book_num, seq, f"[{ch_num}.{sec_num}] {text}"),
                )
                count += 1
                last_book = max(last_book, book_num)

    conn.commit()
    print(f"    {count} sections across {last_book} books")
    return count

def import_texts(conn, work_name, structure, xml_path):
    importers = {
        "verse": import_verse,
        "prose": import_prose,
    }
    if structure not in importers:
        raise ValueError(f"Unknown structure '{structure}' — expected: {', '.join(importers)}")
    return importers[structure](conn, work_name, xml_path)


def import_morphology(conn, treebank_files):
    """parse treebank XML → morphology table (form, lemma, postag)."""
    print("\n  Importing morphology...")

    if not treebank_files:
        print("    WARNING: No treebank files found")
        return 0

    print(f"    Found {len(treebank_files)} treebank file(s)")

    cur = conn.cursor()
    seen = set()
    count = 0

    for fpath in treebank_files:
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

    work = "iliad"
    for arg in sys.argv[1:]:
        if arg.startswith("--work="):
            work = arg.split("=", 1)[1]

    corpora = yaml.safe_load((ROOT / "corpora.yml").read_text())

    if work not in corpora:
        print(f"Unknown work '{work}'. Choices: {', '.join(corpora)}")
        sys.exit(1)

    cfg      = corpora[work]
    label    = cfg["label"]
    db_path  = ROOT / cfg["db"]
    struct   = cfg["structure"]
    tb_cfg   = cfg["treebank"]
    tx_auth  = cfg["text"]["author"]
    tx_work  = cfg["text"]["work"]

    print("=" * 50)
    print(f"  Perseus Database Builder — {label}")
    print("=" * 50)
    print()

    download_repos(skip=skip)

    if db_path.exists():
        db_path.unlink()
    DATA.mkdir(exist_ok=True)

    conn = sqlite3.connect(str(db_path))
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=OFF")

    create_schema(conn)

    print(f"\n  Importing {label} text...")
    xml_path = find_text_xml(tx_auth, tx_work)
    if not xml_path:
        print(f"    ERROR: XML not found for {tx_auth}/{tx_work}")
        sys.exit(1)
    texts = import_texts(conn, work, struct, xml_path)

    treebank_files = find_treebank_files(
        tb_cfg["author"], tb_cfg["work"],
        tb_cfg.get("repo", "treebank"),
        tb_cfg.get("pattern"),
    )
    morphs = import_morphology(conn, treebank_files)
    lexent = import_lexicon(conn)

    print("\n  Pruning lexicon to MVP vocabulary...")
    cur = conn.cursor()
    needed_norm = set()
    for (lemma,) in cur.execute("SELECT DISTINCT lemma FROM morphology"):
        needed_norm.add(normalize_lemma(lemma))

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

    size_mb = db_path.stat().st_size / (1024 * 1024)

    print()
    print("=" * 50)
    print(f"  Database: {db_path}")
    print(f"  Size:     {size_mb:.1f} MB")
    print(f"  Texts:    {texts} lines")
    print(f"  Morph:    {morphs} analyses")
    print(f"  Lexicon:  {lexent} entries")
    print("=" * 50)

    # corpus.mk — Makefile picks up NAME and GAME_SUBTITLE
    mk_path = ROOT / "corpus.mk"
    with open(mk_path, "w") as f:
        f.write(f"CORPUS_WORK  := {work}\n")
        f.write(f"CORPUS_LABEL := {label}\n")

    # corpus_auto.h — C code includes this; avoids shell quoting issues with
    # spaces/commas when the label is passed as a compiler -D flag
    h_path = ROOT / "source" / "corpus_auto.h"
    with open(h_path, "w") as f:
        f.write("/* auto-generated by build_db.py — do not edit */\n")
        f.write(f'#define CORPUS_WORK  "{work}"\n')
        f.write(f'#define CORPUS_LABEL "{label}"\n')

    print(f"\n  Generated: {mk_path.name}, {h_path.name}")
    print("  Done!")
    print()

if __name__ == "__main__":
    main()

#!/usr/bin/env python3

import os
import sys
import json
import zipfile
import urllib.request
from pathlib import Path

OUT_DIR = Path(__file__).resolve().parent.parent / "data" / "fonts"

FONTS = {
    "GentiumPlus-Regular.ttf": {
        "type": "github_release",
        "repo": "silnrsi/font-gentium",
        "zip_member_suffix": "Gentium-Regular.ttf",  # v7+ dropped "Plus" branding
    },
    "Cardo-Regular.ttf": {
        "type": "raw",
        "url": "https://github.com/google/fonts/raw/main/ofl/cardo/Cardo-Regular.ttf",
    },
}


def download(url, dest):
    print(f"    Downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "lexis-nds-build/1.0"})
    with urllib.request.urlopen(req) as resp, open(dest, "wb") as f:
        f.write(resp.read())


def get_github_release_asset_url(repo, suffix):
    api = f"https://api.github.com/repos/{repo}/releases/latest"
    req = urllib.request.Request(api, headers={"User-Agent": "lexis-nds-build/1.0"})
    with urllib.request.urlopen(req) as resp:
        data = json.loads(resp.read())
    for asset in data.get("assets", []):
        if asset["name"].endswith(".zip"):
            return asset["browser_download_url"], asset["name"]
    raise RuntimeError(f"No zip asset found in latest release of {repo}")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    for filename, spec in FONTS.items():
        dest = OUT_DIR / filename
        if dest.exists():
            print(f"  {filename}: already present, skipping")
            continue

        print(f"  {filename}:")

        if spec["type"] == "raw":
            download(spec["url"], dest)

        elif spec["type"] == "github_release":
            zip_url, zip_name = get_github_release_asset_url(spec["repo"], spec["zip_member_suffix"])
            zip_tmp = OUT_DIR / zip_name
            download(zip_url, zip_tmp)
            suffix = spec["zip_member_suffix"]
            with zipfile.ZipFile(zip_tmp) as zf:
                members = [m for m in zf.namelist() if m.endswith(suffix)]
                if not members:
                    raise RuntimeError(f"{suffix} not found in {zip_name}")
                member = members[0]
                print(f"    Extracting {member}")
                data = zf.read(member)
                dest.write_bytes(data)
            zip_tmp.unlink()

        print(f"    -> {dest}")

    print("  Done.")


if __name__ == "__main__":
    main()

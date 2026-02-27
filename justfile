default: lexis-nds

setup:
    python3 scripts/build_db.py

db:
    python3 scripts/build_db.py --skip-download

flatdb: db
    python3 scripts/build_flatdb.py data/perseus.db romfs/lexis.dat --skip-defs

get-fonts:
    python3 scripts/get_fonts.py

fonts: db get-fonts
    @echo "Building Gentium Plus (family 0)..."
    python3 scripts/build_font.py --size 8  --font data/fonts/GentiumPlus-Regular.ttf --out romfs/font_0_8.bin
    python3 scripts/build_font.py --size 10 --font data/fonts/GentiumPlus-Regular.ttf --out romfs/font_0_10.bin
    python3 scripts/build_font.py --size 12 --font data/fonts/GentiumPlus-Regular.ttf --out romfs/font_0_12.bin
    python3 scripts/build_font.py --size 14 --font data/fonts/GentiumPlus-Regular.ttf --out romfs/font_0_14.bin
    python3 scripts/build_font.py --size 16 --font data/fonts/GentiumPlus-Regular.ttf --out romfs/font_0_16.bin
    @echo "Building DejaVu Sans (family 1)..."
    python3 scripts/build_font.py --size 8  --out romfs/font_1_8.bin
    python3 scripts/build_font.py --size 10 --out romfs/font_1_10.bin
    python3 scripts/build_font.py --size 12 --out romfs/font_1_12.bin
    python3 scripts/build_font.py --size 14 --out romfs/font_1_14.bin
    python3 scripts/build_font.py --size 16 --out romfs/font_1_16.bin
    @echo "Building Cardo (family 2)..."
    python3 scripts/build_font.py --size 8  --font data/fonts/Cardo-Regular.ttf --out romfs/font_2_8.bin
    python3 scripts/build_font.py --size 10 --font data/fonts/Cardo-Regular.ttf --out romfs/font_2_10.bin
    python3 scripts/build_font.py --size 12 --font data/fonts/Cardo-Regular.ttf --out romfs/font_2_12.bin
    python3 scripts/build_font.py --size 14 --font data/fonts/Cardo-Regular.ttf --out romfs/font_2_14.bin
    python3 scripts/build_font.py --size 16 --font data/fonts/Cardo-Regular.ttf --out romfs/font_2_16.bin

data: setup flatdb fonts

lexis-nds:
    docker compose run --rm blocksds make

ndsfetch:
    cd ndsfetch && just build

upload: lexis-nds
    curl -X DELETE http://192.168.0.11:3923/nds/lexis-nds.nds 2>/dev/null || true
    curl -T lexis-nds.nds http://192.168.0.11:3923/nds/lexis-nds.nds

upload-ndsfetch: ndsfetch
    curl -X DELETE http://192.168.0.11:3923/nds/ndsfetch.nds 2>/dev/null || true
    curl -T ndsfetch/ndsfetch.nds http://192.168.0.11:3923/nds/ndsfetch.nds

serve port="8880":
    python3 -m http.server {{port}}

fmt:
    clang-format -i source/*.c source/*.h ndsfetch/source/*.c

run: lexis-nds
    data/melonDS-x86_64.AppImage lexis-nds.nds &

get-emulator:
    mkdir -p data
    curl -L -o data/melonDS-appimage.zip https://github.com/melonDS-emu/melonDS/releases/download/1.1/melonDS-1.1-appimage-x86_64.zip
    unzip -o data/melonDS-appimage.zip -d data/
    chmod +x data/melonDS-x86_64.AppImage
    rm data/melonDS-appimage.zip

clean:
    rm -rf build/ lexis-nds.nds ndsfetch/build/ scripts/__pycache__/

distclean: clean
    rm -rf data/ romfs/

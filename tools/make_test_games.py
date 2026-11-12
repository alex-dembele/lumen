#!/usr/bin/env python3
"""Crée de faux dossiers de jeux (param.sfo, icon0.png, pic1.png, eboot.bin vide)
pour tester l'interface sans vrai jeu. Usage : make_test_games.py <dossier_sortie>"""

import math
import struct
import sys
import zlib
from pathlib import Path

GAMES = [
    ("Neon Drift", "TEST00001", "01.00", (255, 80, 160), (40, 0, 90)),
    ("Sky Garden", "TEST00002", "01.02", (90, 220, 140), (0, 60, 90)),
    ("Iron Tide", "TEST00003", "02.00", (255, 170, 60), (90, 20, 0)),
    ("Polar Echo", "TEST00004", "01.00", (140, 200, 255), (10, 30, 80)),
    ("Ember Quest", "TEST00005", "01.05", (255, 90, 60), (50, 0, 20)),
]


def write_png(path, width, height, pixel):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixel(x / width, y / height))

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 6))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * max(0.0, min(1.0, t))) for i in range(3))


def write_sfo(path, entries):
    keys = b""
    data = b""
    index = b""
    for key, value in entries:
        if isinstance(value, int):
            raw, fmt, max_len = struct.pack("<I", value), 0x0404, 4
        else:
            raw = value.encode() + b"\0"
            fmt, max_len = 0x0204, (len(raw) + 3) & ~3
        index += struct.pack("<HHIII", len(keys), fmt, len(raw), max_len, len(data))
        keys += key.encode() + b"\0"
        data += raw.ljust(max_len, b"\0")
    keys = keys.ljust((len(keys) + 3) & ~3, b"\0")
    key_start = 20 + len(index)
    data_start = key_start + len(keys)
    header = struct.pack("<IIIII", 0x46535000, 0x101, key_start, data_start, len(entries))
    path.write_bytes(header + index + keys + data)


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "test-games")
    for title, title_id, version, accent, dark in GAMES:
        root = out / title_id
        (root / "sce_sys").mkdir(parents=True, exist_ok=True)
        (root / "eboot.bin").write_bytes(b"")
        write_sfo(root / "sce_sys" / "param.sfo", [
            ("APP_VER", version), ("CATEGORY", "gd"), ("TITLE", title), ("TITLE_ID", title_id),
        ])

        def icon(u, v, accent=accent, dark=dark):
            d = math.hypot(u - 0.5, v - 0.42)
            ring = 1.0 if 0.18 < d < 0.24 else 0.0
            col = mix(accent, dark, v * 1.1)
            return mix(col, (255, 255, 255), ring * 0.85)

        def background(u, v, accent=accent, dark=dark):
            wave = 0.5 + 0.5 * math.sin(u * 9 + v * 4)
            return mix(dark, accent, (1 - v) * 0.55 * wave + 0.1)

        write_png(root / "sce_sys" / "icon0.png", 256, 256, icon)
        write_png(root / "sce_sys" / "pic1.png", 1920, 1080, background)
        print("créé", root)


if __name__ == "__main__":
    main()

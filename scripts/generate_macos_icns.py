#!/usr/bin/env python3
"""Genera icons/AppIcon.icns desde icons/logo.ico (solo macOS: sips + iconutil)."""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ICO_PATH = ROOT / "icons" / "logo.ico"
OUT_ICNS = ROOT / "icons" / "AppIcon.icns"

# Nombres exigidos por iconutil
ICONSET_SIZES = [
    ("icon_16x16.png", 16, 16),
    ("icon_16x16@2x.png", 32, 32),
    ("icon_32x32.png", 32, 32),
    ("icon_32x32@2x.png", 64, 64),
    ("icon_128x128.png", 128, 128),
    ("icon_128x128@2x.png", 256, 256),
    ("icon_256x256.png", 256, 256),
    ("icon_256x256@2x.png", 512, 512),
    ("icon_512x512.png", 512, 512),
    ("icon_512x512@2x.png", 1024, 1024),
]


def main() -> int:
    if sys.platform != "darwin":
        print("generate_macos_icns.py: omitido (requiere macOS).", file=sys.stderr)
        return 0

    if not ICO_PATH.is_file():
        print(f"No existe {ICO_PATH}", file=sys.stderr)
        return 1

    try:
        from PIL import Image
    except ImportError:
        print("Instala Pillow: pip install Pillow", file=sys.stderr)
        return 1

    im = Image.open(ICO_PATH)
    frames: list = []
    i = 0
    while True:
        try:
            im.seek(i)
            frames.append(im.copy())
            i += 1
        except EOFError:
            break
    if not frames:
        frames = [im]
    base = max(frames, key=lambda f: f.size[0] * f.size[1]).convert("RGBA")

    tmp_png = Path(tempfile.mkdtemp()) / "base.png"
    # Fuente cuadrada para sips
    w, h = base.size
    side = max(w, h)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(base, ((side - w) // 2, (side - h) // 2))
    canvas.resize((1024, 1024), Image.Resampling.LANCZOS).save(tmp_png, "PNG")

    iconset = Path(tempfile.mkdtemp()) / "AppIcon.iconset"
    iconset.mkdir(parents=True)

    for name, rw, rh in ICONSET_SIZES:
        out = iconset / name
        r = subprocess.run(
            ["sips", "-z", str(rh), str(rw), str(tmp_png), "--out", str(out)],
            capture_output=True,
            text=True,
        )
        if r.returncode != 0:
            print(r.stderr or r.stdout, file=sys.stderr)
            return r.returncode

    OUT_ICNS.parent.mkdir(parents=True, exist_ok=True)
    if OUT_ICNS.is_file():
        OUT_ICNS.unlink()

    r = subprocess.run(
        ["iconutil", "-c", "icns", str(iconset), "-o", str(OUT_ICNS)],
        capture_output=True,
        text=True,
    )
    shutil.rmtree(tmp_png.parent, ignore_errors=True)
    shutil.rmtree(iconset.parent, ignore_errors=True)

    if r.returncode != 0:
        print(r.stderr or r.stdout, file=sys.stderr)
        return r.returncode

    print(f"OK: {OUT_ICNS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

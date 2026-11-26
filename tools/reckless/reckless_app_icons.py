#!/usr/bin/env python3
# This file is part of KiCad, a free EDA CAD application.
#
# Copyright (C) 2026 Reckless Technology.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""Make the Reckless application icons from the Reckless logo.

The source is tools/reckless/app_icon/RecklessAppIcon.png, the 1024 px icon the Reckless toolkit's
assets/logos/make_app_icon.py makes (the logo at 90% fill on a transparent square). Every size here is that picture
scaled down with a Lanczos filter, the way the toolkit's bundle recipes make its own icons, so a taskbar, a Dock and a
Start menu all show the same mark.

    reckless_app_icons.py --write resources/reckless/app-icon     # write every file
    reckless_app_icons.py --check resources/reckless/app-icon     # fail if the files are stale or incomplete
    reckless_app_icons.py --sheet contact_sheet.png               # draw every size on a contact sheet

What it writes, under the directory given:

    hicolor/<n>x<n>/apps/<program>.png and hicolor/scalable/apps/<program>.svg   Linux launchers (n = 16 to 128)
    macos/<name>.icns                                                          macOS bundles
    windows/<name>.ico                                                         Windows executables
    png/reckless_app_icon_<n>.png                                              the programs' own window icons

--check compares pictures, not bytes, so a different Pillow or zlib that writes other bytes for the same picture does
not fail it. It allows each channel to differ by MAX_CHANNEL_DIFFERENCE for a different resampler.
"""

import argparse
import base64
import io
import re
import struct
import sys
import tempfile
from pathlib import Path

from PIL import Image

SOURCE = Path(__file__).resolve().parent / "app_icon" / "RecklessAppIcon.png"

# The Linux desktop entries' icon names and the sizes the hicolor theme carries them in.
LINUX_PROGRAMS = ["kicad", "eeschema", "pcbnew", "gerbview", "pcbcalculator"]
LINUX_SIZES = [16, 24, 32, 48, 64, 128]

# The macOS bundles' application icons (the *_doc.icns files are document icons and stay KiCad's own).
MACOS_NAMES = [
    "kicad",
    "eeschema",
    "libedit",
    "pcbnew",
    "fpedit",
    "gerbview",
    "pagelayout_editor",
    "pcb_calculator",
]

# The Windows executables' icons, under the names resources/msw/*.rc use.
WINDOWS_NAMES = [
    "icon_kicad",
    "icon_eeschema",
    "icon_pcbnew",
    "icon_gerbview",
    "icon_pagelayout_editor",
    "icon_pcbcalculator",
]
WINDOWS_SIZES = [16, 24, 32, 48, 64, 128, 256]

# The sizes the programs ask for when they set a window's icon (16 and 32 are the small ones, 48 and up the large).
WINDOW_SIZES = [16, 32, 48, 64, 128, 256]

# macOS icon element types and the pixel size of the PNG each holds.
ICNS_ELEMENTS = [
    (b"icp4", 16),
    (b"icp5", 32),
    (b"icp6", 64),
    (b"ic07", 128),
    (b"ic08", 256),
    (b"ic09", 512),
    (b"ic10", 1024),
]

MAX_CHANNEL_DIFFERENCE = 2


def scaled(source: Image.Image, size: int) -> Image.Image:
    """The icon at size x size, from the full-size source."""
    if size == source.width:
        return source.copy()
    return source.resize((size, size), Image.LANCZOS)


def png_bytes(image: Image.Image) -> bytes:
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def icns_bytes(source: Image.Image) -> bytes:
    body = b""
    for element_type, size in ICNS_ELEMENTS:
        payload = png_bytes(scaled(source, size))
        body += element_type + struct.pack(">I", 8 + len(payload)) + payload
    return b"icns" + struct.pack(">I", 8 + len(body)) + body


def ico_bytes(source: Image.Image) -> bytes:
    buffer = io.BytesIO()
    source.save(buffer, format="ICO", sizes=[(size, size) for size in WINDOWS_SIZES])
    return buffer.getvalue()


def svg_text(source: Image.Image) -> str:
    """A scalable icon: the 256 px picture inside an SVG, which is all a raster logo can be."""
    encoded = base64.b64encode(png_bytes(scaled(source, 256))).decode("ascii")
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"'
        ' width="256" height="256" viewBox="0 0 256 256">\n'
        '  <image width="256" height="256" xlink:href="data:image/png;base64,' + encoded + '"/>\n'
        "</svg>\n"
    )


def every_file(source: Image.Image) -> dict:
    """Relative path -> bytes, for every file this script makes."""
    files = {}
    for program in LINUX_PROGRAMS:
        for size in LINUX_SIZES:
            files[f"hicolor/{size}x{size}/apps/{program}.png"] = png_bytes(scaled(source, size))
        files[f"hicolor/scalable/apps/{program}.svg"] = svg_text(source).encode("utf-8")
    icns = icns_bytes(source)
    for name in MACOS_NAMES:
        files[f"macos/{name}.icns"] = icns
    ico = ico_bytes(source)
    for name in WINDOWS_NAMES:
        files[f"windows/{name}.ico"] = ico
    for size in WINDOW_SIZES:
        files[f"png/reckless_app_icon_{size}.png"] = png_bytes(scaled(source, size))
    return files


def load_source() -> Image.Image:
    image = Image.open(SOURCE).convert("RGBA")
    if image.size != (1024, 1024):
        sys.exit(f"{SOURCE} is {image.size}, not 1024 x 1024")
    return image


def pictures(path: str, data: bytes) -> list:
    """Every picture in a file as (size, RGBA bytes), for comparison."""
    if path.endswith(".png"):
        image = Image.open(io.BytesIO(data)).convert("RGBA")
        return [(image.size, image.tobytes())]
    if path.endswith(".svg"):
        match = re.search(rb"base64,([A-Za-z0-9+/=]+)", data)
        if not match:
            return []
        return pictures("x.png", base64.b64decode(match.group(1))) + [(None, re.sub(rb"base64,[A-Za-z0-9+/=]+", b"", data))]
    if path.endswith(".icns"):
        found = []
        offset = 8
        while offset < len(data):
            length = struct.unpack(">I", data[offset + 4 : offset + 8])[0]
            found += pictures("x.png", data[offset + 8 : offset + length])
            offset += length
        return found
    if path.endswith(".ico"):
        icon = Image.open(io.BytesIO(data))
        found = []
        for size in sorted(icon.ico.sizes()):
            frame = icon.ico.getimage(size).convert("RGBA")
            found.append((frame.size, frame.tobytes()))
        return found
    return [(None, data)]


def same_pictures(first: list, second: list) -> bool:
    if len(first) != len(second):
        return False
    for (size_a, bytes_a), (size_b, bytes_b) in zip(first, second):
        if size_a != size_b or len(bytes_a) != len(bytes_b):
            return False
        if size_a is None:
            if bytes_a != bytes_b:
                return False
        elif max(abs(a - b) for a, b in zip(bytes_a, bytes_b)) > MAX_CHANNEL_DIFFERENCE:
            return False
    return True


def write(target: Path) -> None:
    for relative, data in every_file(load_source()).items():
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    print(f"wrote {len(every_file(load_source()))} files under {target}")


def check(target: Path) -> int:
    expected = every_file(load_source())
    problems = []
    for relative, data in expected.items():
        committed = target / relative
        if not committed.is_file():
            problems.append(f"missing: {relative}")
            continue
        try:
            is_current = same_pictures(pictures(relative, committed.read_bytes()), pictures(relative, data))
        except Exception:  # a truncated or damaged file is a stale one
            is_current = False
        if not is_current:
            problems.append(f"stale: {relative}")
    on_disk = {str(p.relative_to(target)) for p in target.rglob("*") if p.is_file()}
    for relative in sorted(on_disk - set(expected)):
        problems.append(f"not made by the script: {relative}")
    for problem in problems:
        print(problem)
    if problems:
        print(f"{len(problems)} problem(s): run tools/reckless/reckless_app_icons.py --write {target}")
        return 1
    print(f"{len(expected)} files are current")
    return 0


def sheet(path: Path) -> None:
    """Every size on a dark and a light ground, so the edge and the small sizes can be judged by eye."""
    source = load_source()
    sizes = [16, 24, 32, 48, 64, 128, 256]
    width = sum(size + 16 for size in sizes) + 16
    grounds = [(0x1B, 0x1A, 0x18), (0xF2, 0xF2, 0xF2)]
    out = Image.new("RGBA", (width, 2 * 288 + 16), (0, 0, 0, 255))
    for row, ground in enumerate(grounds):
        band = Image.new("RGBA", (width, 288), ground + (255,))
        x = 16
        for size in sizes:
            icon = scaled(source, size)
            band.alpha_composite(icon, (x, 16))
            x += size + 16
        out.paste(band, (0, row * 296))
    # The same sizes at 4x nearest-neighbor, to see the pixels.
    zoom = Image.new("RGBA", (width, 4 * 64 + 32), (0x1B, 0x1A, 0x18, 255))
    x = 16
    for size in [16, 24, 32, 48]:
        icon = scaled(source, size).resize((size * 4, size * 4), Image.NEAREST)
        zoom.alpha_composite(icon, (x, 16))
        x += size * 4 + 16
    final = Image.new("RGBA", (width, out.height + zoom.height), (0, 0, 0, 255))
    final.paste(out, (0, 0))
    final.paste(zoom, (0, out.height))
    final.save(path)
    print(f"wrote {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--write", metavar="DIR", help="write every icon under DIR")
    group.add_argument("--check", metavar="DIR", help="fail if DIR does not hold exactly what --write makes")
    group.add_argument("--sheet", metavar="PNG", help="draw a contact sheet of the sizes")
    arguments = parser.parse_args()
    if arguments.write:
        write(Path(arguments.write))
        return 0
    if arguments.check:
        return check(Path(arguments.check))
    sheet(Path(arguments.sheet))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
#
# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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

"""
Build the house bitmap font: the GAL's built-in MSDF atlas drawn from Monaspace Xenon Regular.

The house build (KICAD_HOUSE_FONTS) compiles bitmap_font_xenon_img.c and bitmap_font_xenon_desc.c in place of the
Ubuntu Mono atlas, which stays as it is for every other build. This script writes those two files.

It keeps the upstream atlas's codepoint set exactly. Every codepoint Monaspace Xenon has is rendered from Xenon by
msdf-atlas-gen; every one it lacks keeps the upstream atlas's glyph, copied texel for texel, so nothing turns into a
missing-glyph box. Both kinds use the upstream encoding, so the shaders read the new atlas unchanged:

  - 48 atlas texels per em, which are also the font units of bitmap_font_*_desc.c;
  - a multichannel signed distance field with a 3-texel range (edge at 0.5, inside above it);
  - a 1-texel border round every cell (font_information.smooth_pixels), outside the quad a glyph is drawn on;
  - cells stored upside down, since the GAL maps the top of a glyph's quad to the bottom row of its cell.

Xenon's glyphs advance 0.62 em (29.76 units). A kept upstream glyph, drawn for a 24-unit advance, is centered in the
wider cell, so the font stays monospaced and the painters' character-count sizing still holds.

Usage:
    make_house_atlas.py --font MonaspaceXenon-Regular.otf --msdf-atlas-gen /path/to/msdf-atlas-gen

The README beside this script names the tool and font versions the committed atlas was made with.

Requires: Python 3, numpy and Pillow, and msdf-atlas-gen 1.4 (github.com/Chlumsky/msdf-atlas-gen, MIT).
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
GL_DIR = os.path.normpath(os.path.join(HERE, "../../common/gal/opengl"))

EM_TEXELS = 48     # atlas texels per em, as in the upstream atlas
PX_RANGE = 3       # distance range in texels, as in the upstream atlas
BORDER = 1         # texels round each quad in its cell (font_information.smooth_pixels)
ATLAS_WIDTH = 1024


def load_upstream():
    """The upstream atlas: its size, pixels, vertical extent, and each codepoint's glyph record."""
    img_src = open(os.path.join(GL_DIR, "bitmap_font_img.c"), encoding="utf-8").read()
    head = re.search(r"font_image\s*=\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*\{", img_src)
    width, height, border, _ = map(int, head.groups())

    if border != BORDER:
        sys.exit(f"upstream atlas border is {border}, expected {BORDER}")

    body = img_src[head.end():img_src.rfind("}", 0, img_src.rfind("}"))]
    pixels = np.array(re.findall(r"\d+", body), dtype=np.uint8)[:width * height * 3].reshape(height, width, 3)

    desc_src = open(os.path.join(GL_DIR, "bitmap_font_desc.c"), encoding="utf-8").read()
    info = re.search(r"font_information\s*=\s*\{\s*(\d+),\s*([-\d.]+)f,\s*([-\d.]+)f", desc_src).groups()
    spans_src = re.search(r"font_codepoint_spans\[\]\s*=\s*\{(.*?)\};", desc_src, re.S).group(1)
    spans = [tuple(map(int, s)) for s in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", spans_src)]
    infos_src = re.search(r"font_codepoint_infos\[\]\s*=\s*\{(.*)\};", desc_src, re.S).group(1)
    records = [[float(v) for v in re.findall(r"-?[\d.]+", r)] for r in re.findall(r"\{([^}]*)\}", infos_src)]

    glyphs = {}

    for start, end, cumulative in spans:
        for cp in range(start, end):
            glyphs[cp] = records[cumulative + cp - start]

    return dict(pixels=pixels, min_y=float(info[1]), glyphs=glyphs)


def render_font(font, tool, codepoints):
    """Render the codepoints the font has with msdf-atlas-gen. Returns its atlas pixels and glyph layout."""
    with tempfile.TemporaryDirectory() as work:
        charset = os.path.join(work, "charset.txt")
        image = os.path.join(work, "atlas.png")
        layout = os.path.join(work, "atlas.json")

        with open(charset, "w") as f:
            f.write(", ".join(str(cp) for cp in codepoints) + "\n")

        # A pixel-aligned origin puts each glyph box on the texel grid. The top origin matches the image's row
        # order. The outer padding keeps neighbors apart in this intermediate atlas only.
        command = [tool, "-font", font, "-charset", charset, "-type", "msdf", "-format", "png",
                   "-imageout", image, "-json", layout, "-size", str(EM_TEXELS), "-pxrange", str(PX_RANGE),
                   "-pxalign", "on", "-yorigin", "top", "-outerpxpadding", str(BORDER), "-potr",
                   "-threads", "1"]
        subprocess.run(command, check=True)

        pixels = np.asarray(Image.open(image).convert("RGB"))
        glyphs = {g["unicode"]: g for g in json.load(open(layout))["glyphs"]}
        metrics = json.load(open(layout))["metrics"]

    return pixels, glyphs, metrics


def font_cell(pixels, glyph):
    """A rendered glyph's cell, border included, and its record in font units (record fields after atlas x, y)."""
    box = glyph["atlasBounds"]
    plane = glyph["planeBounds"]

    # The bounds run between texel centers, half a texel inside the glyph's box, which holds the ink and half the
    # distance range round it. The quad keeps one texel of that range beyond the ink and the next texel out is the
    # cell's border, so the quad stays close to the ink as the upstream atlas's do: a minified glyph's antialiasing
    # spreads over its whole quad.
    left, top = int(round(box["left"] - 0.5)), int(round(box["top"] - 0.5))
    right, bottom = int(round(box["right"] + 0.5)), int(round(box["bottom"] + 0.5))
    inset = 1 - BORDER
    cell = pixels[top + inset:bottom - inset, left + inset:right - inset]

    # Cells are stored upside down, as in the upstream atlas: the GAL maps a quad's top to its cell's bottom row
    cell = np.flipud(cell).copy()

    record = [cell.shape[1], cell.shape[0],
              plane["left"] * EM_TEXELS + 0.5, plane["right"] * EM_TEXELS - 0.5,
              -plane["bottom"] * EM_TEXELS + 0.5, -plane["top"] * EM_TEXELS - 0.5,
              glyph["advance"] * EM_TEXELS]
    return cell, record


def kept_cell(upstream, record, advance):
    """An upstream glyph's cell and record, centered in the wider advance."""
    x, y, w, h = (int(v) for v in record[:4])
    cell = upstream["pixels"][y:y + h, x:x + w].copy()
    shift = max( 0.0, ( advance - record[8] ) / 2.0 )
    return cell, [w, h, record[4] + shift, record[5] + shift, record[6], record[7], max(advance, record[8])]


def pack(cells):
    """Shelf-pack the cells into an atlas ATLAS_WIDTH wide. Returns each cell's corner and the atlas height."""
    order = sorted(cells, key=lambda cp: (-cells[cp].shape[0], cp))
    place = {}
    x, y, shelf = 0, 0, 0

    for cp in order:
        h, w = cells[cp].shape[:2]

        if x + w > ATLAS_WIDTH:
            x, y, shelf = 0, y + shelf, 0

        place[cp] = (x, y)
        x += w
        shelf = max(shelf, h)

    return place, y + shelf


def write_img(path, width, height, atlas):
    flat = atlas.reshape(-1)
    lines = [",".join(map(str, flat[i:i + 3 * width])) for i in range(0, flat.size, 3 * width)]

    with open(path, "w", encoding="utf-8") as f:
        f.write("// Generated by tools/gal_bitmap_font/make_house_atlas.py, do not modify.\n")
        f.write("// Monaspace Xenon Regular, SIL Open Font License 1.1: see monaspace-xenon-ofl.txt\n")
        f.write(f"FONT_IMAGE_TYPE font_image = {{ {width}, {height}, {BORDER}, 0, {{\n")
        f.write(",\n".join(lines))
        f.write("\n}};\n")


def write_desc(path, codepoints, records, place, min_y, max_y):
    # Spans of consecutive codepoints, as the lookup expects
    spans = []

    for index, cp in enumerate(codepoints):
        if spans and spans[-1][1] == cp:
            spans[-1][1] = cp + 1
        else:
            spans.append([cp, cp + 1, index])

    with open(path, "w", encoding="utf-8") as f:
        f.write("// Generated by tools/gal_bitmap_font/make_house_atlas.py, do not modify.\n")
        f.write(f"FONT_INFO_TYPE font_information = {{\n    {BORDER},\n    {min_y:.4f}f,\n    {max_y:.4f}f\n}};\n\n")
        f.write("FONT_SPAN_TYPE font_codepoint_spans[] = {\n")
        f.write("\n".join(f"    {{ {s}, {e}, {c} }}," for s, e, c in spans))
        f.write("\n};\n\n")
        f.write("FONT_GLYPH_TYPE font_codepoint_infos[] = {\n")

        for cp in codepoints:
            w, h, minx, maxx, miny, maxy, advance = records[cp]
            x, y = place[cp]
            f.write(f"{{ {x}, {y}, {w}, {h}, {minx:.4f}f, {maxx:.4f}f, {miny:.4f}f, {maxy:.4f}f, {advance:.4f}f }},\n")

        f.write("};\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    parser.add_argument("--font", required=True, help="MonaspaceXenon-Regular.otf")
    parser.add_argument("--msdf-atlas-gen", required=True, help="the msdf-atlas-gen executable")
    parser.add_argument("--kept-list", help="write the codepoints kept from the upstream atlas to this file")
    args = parser.parse_args()

    upstream = load_upstream()
    codepoints = sorted(upstream["glyphs"])
    pixels, rendered, metrics = render_font(args.font, args.msdf_atlas_gen, codepoints)

    advance = round(rendered[ord("0")]["advance"] * EM_TEXELS, 4)
    cells, records, kept = {}, {}, []

    for cp in codepoints:
        if cp in rendered:
            cell, record = font_cell(pixels, rendered[cp])
        else:
            cell, record = kept_cell(upstream, upstream["glyphs"][cp], advance)
            kept.append(cp)

        cells[cp] = cell
        records[cp] = record

    place, height = pack(cells)
    atlas = np.zeros((height, ATLAS_WIDTH, 3), dtype=np.uint8)

    for cp, (x, y) in place.items():
        h, w = cells[cp].shape[:2]
        atlas[y:y + h, x:x + w] = cells[cp]

    # The font's ascender and descender bound the line, in the atlas's units
    max_y = -metrics["ascender"] * EM_TEXELS
    min_y = -metrics["descender"] * EM_TEXELS

    write_img(os.path.join(GL_DIR, "bitmap_font_xenon_img.c"), ATLAS_WIDTH, height, atlas)
    write_desc(os.path.join(GL_DIR, "bitmap_font_xenon_desc.c"), codepoints, records, place, min_y, max_y)

    if args.kept_list:
        with open(args.kept_list, "w") as f:
            f.write("\n".join(f"U+{cp:04X}" for cp in kept) + "\n")

    print(f"{len(codepoints)} glyphs: {len(codepoints) - len(kept)} from the font, {len(kept)} kept from the "
          f"upstream atlas; atlas {ATLAS_WIDTH}x{height}")
    print(f"** gl_resources.h: the house atlas is {ATLAS_WIDTH} * {height} * 3")


if __name__ == "__main__":
    sys.exit(main())

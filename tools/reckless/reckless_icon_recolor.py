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

"""Recolor KiCad's dark icon set to the Reckless look, mechanically, and preview it. INVESTIGATION ONLY: nothing here is
built into KiCad.

    reckless_icon_recolor.py census                  # count the colors of the dark set, grouped by role
    reckless_icon_recolor.py recolor OUT_DIR         # write the recolored copy of sources/dark/ (every SVG)
    reckless_icon_recolor.py report                  # clean versus special-case counts for the whole set
    reckless_icon_recolor.py preview OUT_DIR         # contact sheets and toolbar strips for the toolbar icons

The rule changes only color values, by text substitution inside fill, stroke, stop-color, flood-color and color
(attributes, style attributes and style sheets), so no path, transform or byte of geometry changes. It is a function of
a color alone, with these classes (OKLab chroma C and hue h of the source color):

  neutral     C < NEUTRAL_CHROMA          the main glyph, outlines, shadows, whites. Cream hue and chroma, at a luminance
                                          from a monotonic piecewise-linear map: black stays black, the dark set's main
                                          glyph (#ded3dd) lands on cream, white stays white.
  blue family hue in BLUE_HUE_RANGE       the dark set's accent. Amber hue and chroma at the source luminance.
  the rest    red, green, orange, purple  the colors that carry meaning. Hue and chroma kept, luminance kept (so the
                                          source's luminance differences between them survive), and nothing else.

Because luminance is kept for the chromatic classes and the neutrals' map is monotonic, the contrast between any two colors of an icon
changes only through the cream scale factor and the cap. A per-icon check then compares every pair of colors before and
after, and an icon where a pair that could be told apart no longer can is a special case; those are redone with the
fallback rule (neutrals only, the chromatic colors left as they are). Contrast is WCAG 2 relative luminance.
"""

import argparse
import collections
import colorsys
import glob
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import reckless_color_theme as theme  # noqa: E402  (the shared color arithmetic and the look's colors)

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SOURCES = os.path.join(REPO, "resources", "bitmaps_png", "sources")
PALETTE_HEADER = os.path.join(REPO, "libs", "kiplatform", "include", "kiplatform", "reckless_palette.h")

NEUTRAL_CHROMA = 0.035  # OKLab chroma under which a color is a gray
BLUE_HUE_RANGE = (185.0, 290.0)  # OKLab hue angle, degrees: cyan through blue, not purple
STOCK_MAIN_GLYPH = "#ded3dd"  # the dark set's main glyph color, which becomes cream
PAIR_LUMINANCE_CONTRAST = 1.2  # two colors differ by lightness at this WCAG contrast ...
PAIR_LUMINANCE_AFTER = 1.12  # the floor after the rule: a pair that was told apart at 1.2 may lose a little, not all
PAIR_HUE_DISTANCE = 0.08  # ... or by this OKLab distance
# `color` counts only as a style declaration (color:#...): as an attribute it is Inkscape's editor grid, which never draws.
PROPERTY = re.compile(
    r"(?<![\w-])(?:(fill|stroke|stop-color|flood-color)(\s*[:=]\s*[\"']?\s*)|(color)(\s*:\s*))([^;\"'>\s]+)")
NAMED = {"white": "#ffffff", "black": "#000000", "red": "#ff0000", "green": "#008000", "blue": "#0000ff",
         "yellow": "#ffff00", "gray": "#808080", "grey": "#808080", "orange": "#ffa500", "lime": "#00ff00",
         "silver": "#c0c0c0", "cyan": "#00ffff", "magenta": "#ff00ff", "purple": "#800080", "maroon": "#800000",
         "navy": "#000080", "teal": "#008080", "olive": "#808000"}


def palette_color(name):
    """A color of the Reckless palette, read from the header the house build uses, never typed here."""
    text = open(PALETTE_HEADER, encoding="utf8").read()
    match = re.search(r"\b" + name + r"\s*=\s*0x([0-9a-fA-F]{6})", text)
    return "#" + match.group(1).lower()


INK = palette_color("INK")
AMBER = palette_color("AMBER")
PANEL = palette_color("PANEL")
WINDOW = palette_color("WINDOW")
PLATE = palette_color("PLATE")


# --- color parsing and the rule ----------------------------------------------------------------------------------------


def parse_color(text):
    """sRGB tuple 0..1 for a hex, rgb() or named color; None for none, url(), currentColor, context-stroke and so on."""
    value = text.strip().lower()
    value = NAMED.get(value, value)

    if re.fullmatch(r"#[0-9a-f]{3}", value):
        value = "#" + "".join(c * 2 for c in value[1:])

    if re.fullmatch(r"#[0-9a-f]{6}", value):
        return theme.parse_hex(value)

    match = re.fullmatch(r"rgb\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)", value)

    if match:
        return tuple(int(g) / 255.0 for g in match.groups())

    return None


def hex_of(rgb):
    return "#%02x%02x%02x" % tuple(round(max(0, min(1, c)) * 255) for c in rgb)


def oklch(rgb):
    _, a, b = theme.to_oklab(rgb)
    return math.hypot(a, b), math.degrees(math.atan2(b, a)) % 360.0


def at_luminance(hue_source, target_luminance):
    """The hue and chroma of hue_source at the OKLab lightness whose WCAG luminance is target_luminance."""
    _, a, b = theme.to_oklab(hue_source)
    low, high = 0.0, 1.0

    for _ in range(60):
        middle = (low + high) / 2

        if theme.luminance(theme.at_lightness(a, b, middle)) < target_luminance:
            low = middle
        else:
            high = middle

    return theme.quantize(theme.at_lightness(a, b, (low + high) / 2))


def color_class(rgb):
    chroma, hue = oklch(rgb)

    if chroma < NEUTRAL_CHROMA:
        return "neutral"

    if BLUE_HUE_RANGE[0] <= hue <= BLUE_HUE_RANGE[1]:
        return "blue"

    if hue < 40 or hue >= 330:
        return "red"

    if hue < 112:
        return "orange"

    if hue < 185:
        return "green"

    return "purple"


CREAM = theme.parse_hex(INK)
AMBER_RGB = theme.parse_hex(AMBER)


def neutral_luminance(source_luminance):
    """Piecewise linear and monotonic: 0 stays 0, the stock glyph luminance becomes cream's, white stays white."""
    glyph = theme.luminance(theme.parse_hex(STOCK_MAIN_GLYPH))
    cream = theme.luminance(CREAM)

    if source_luminance <= glyph:
        return source_luminance * cream / glyph

    return cream + (source_luminance - glyph) * (1.0 - cream) / (1.0 - glyph)


def map_color(rgb, neutrals_only=False):
    """The one rule: a color to a color. See the module text."""
    kind = color_class(rgb)
    source_luminance = theme.luminance(rgb)

    if kind == "neutral":
        return at_luminance(CREAM, neutral_luminance(source_luminance))

    if kind == "blue" and not neutrals_only:
        return at_luminance(AMBER_RGB, source_luminance)

    return rgb


def properties(svg_text):
    """(property, value) of every color-bearing declaration in the text."""
    return [(m.group(1) or m.group(3), m.group(5)) for m in PROPERTY.finditer(svg_text)]


def recolor_text(svg_text, neutrals_only=False):
    """The SVG with every color value rewritten by the rule, and nothing else touched."""

    def replace(match):
        rgb = parse_color(match.group(5))

        if rgb is None:
            return match.group(0)

        return match.group(0)[: match.start(5) - match.start(0)] + hex_of(map_color(rgb, neutrals_only))

    return PROPERTY.sub(replace, svg_text)


def colors_of(svg_text):
    """Every distinct color value in the file, as parsed sRGB, with the property it was found under."""
    found = collections.defaultdict(set)

    for prop, value in properties(svg_text):
        rgb = parse_color(value)

        if rgb is not None:
            found[hex_of(rgb)].add(prop)

    return found


def unparsed_values(svg_text):
    """Values of the color properties that are not plain colors: none, url(...), currentColor, context-stroke."""
    return collections.Counter(
        value.lower() for _, value in properties(svg_text) if parse_color(value) is None
    )


# --- the per-icon check ------------------------------------------------------------------------------------------------


def distinguishable(first, second, luminance_floor=PAIR_LUMINANCE_CONTRAST):
    if theme.contrast(first, second) >= luminance_floor:
        return True

    if color_class(first) == "neutral" and color_class(second) == "neutral":
        return False  # two grays that close in lightness are one tone, not a distinction

    return math.dist(theme.to_oklab(first), theme.to_oklab(second)) >= PAIR_HUE_DISTANCE


def collisions(svg_text, neutrals_only=False):
    """Pairs of colors in this icon that can be told apart in the source and not after the rule."""
    colors = [theme.parse_hex(h) for h in colors_of(svg_text)]
    lost = []

    for i, first in enumerate(colors):
        for second in colors[i + 1 :]:
            if not distinguishable(first, second):
                continue

            mapped = (map_color(first, neutrals_only), map_color(second, neutrals_only))

            if not distinguishable(*mapped, luminance_floor=PAIR_LUMINANCE_AFTER):
                lost.append((hex_of(first), hex_of(second)))

    return lost


def classify(svg_text):
    """'clean', 'fallback' (neutrals-only rule resolves it) or 'review' (still loses a distinction)."""
    if not collisions(svg_text):
        return "clean"

    if not collisions(svg_text, neutrals_only=True):
        return "fallback"

    return "review"


def recolor_icon(svg_text):
    return recolor_text(svg_text, neutrals_only=classify(svg_text) == "fallback")


def dark_svgs():
    return sorted(glob.glob(os.path.join(SOURCES, "dark", "**", "*.svg"), recursive=True))


# --- census ------------------------------------------------------------------------------------------------------------


def role_of(rgb):
    kind = color_class(rgb)

    if kind != "neutral":
        return {"blue": "accent (blue family)", "red": "meaning: red (delete, error, stop)",
                "green": "meaning: green (add, ok, go)", "orange": "meaning: orange and yellow (warn, highlight)",
                "purple": "meaning: purple and other"}[kind]

    luminance = theme.luminance(rgb)

    if luminance >= 0.45:
        return "neutral: main glyph (light)"

    if luminance >= 0.12:
        return "neutral: secondary and disabled-looking (mid)"

    return "neutral: outline and shadow (dark)"


def command_census(_args):
    per_color = collections.Counter()
    per_color_files = collections.defaultdict(set)
    by_property = collections.Counter()
    other_values = collections.Counter()
    other_files = collections.defaultdict(set)
    files = dark_svgs()

    for path in files:
        text = open(path, encoding="utf8", errors="replace").read()

        for prop, value in properties(text):
            rgb = parse_color(value)

            if rgb is None:
                other_values[value.lower()] += 1
                other_files[value.lower()].add(path)
                continue

            per_color[hex_of(rgb)] += 1
            per_color_files[hex_of(rgb)].add(path)
            by_property[prop] += 1

    print("dark SVGs: %d. distinct colors: %d. color uses by property: %s"
          % (len(files), len(per_color), dict(by_property)))
    roles = collections.defaultdict(list)

    for color in per_color:
        roles[role_of(theme.parse_hex(color))].append(color)

    for role in sorted(roles):
        members = sorted(roles[role], key=lambda c: -len(per_color_files[c]))
        icon_count = len(set().union(*(per_color_files[c] for c in members)))
        print("\n%s: %d distinct colors, in %d icons" % (role, len(members), icon_count))

        for color in members[:8]:
            print("    %s  %5d uses  %4d icons  luminance %.3f  contrast on panel %.2f"
                  % (color, per_color[color], len(per_color_files[color]),
                     theme.luminance(theme.parse_hex(color)), theme.contrast(theme.parse_hex(color), theme.parse_hex(PANEL))))

    print("\nvalues that are not a plain color:")

    for value, count in other_values.most_common(8):
        print("    %-40s %5d uses %4d icons" % (value[:40], count, len(other_files[value])))


# --- report -----------------------------------------------------------------------------------------------------------


def command_report(_args):
    counts = collections.Counter()
    review = []
    plain = 0

    for path in dark_svgs():
        text = open(path, encoding="utf8", errors="replace").read()
        kind = classify(text)
        counts[kind] += 1

        if not colors_of(text):
            plain += 1

        if kind == "review":
            review.append((os.path.relpath(path, SOURCES), collisions(text, neutrals_only=True)))

    total = sum(counts.values())
    print("dark SVGs: %d  clean: %d  fallback (neutrals only): %d  review: %d  with no plain color at all: %d"
          % (total, counts["clean"], counts["fallback"], counts["review"], plain))

    for name, lost in review:
        print("    review: %s  %s" % (name, lost[:3]))


def command_verify(_args):
    """No shape changes: the text differs only in color values, and the 24 px alpha masks are identical."""
    import numpy

    bad_text = bad_alpha = 0

    def blank(text):
        """The text with every color value blanked: what is left is geometry and structure."""
        return PROPERTY.sub(lambda m: m.group(0)[: m.start(5) - m.start(0)] + "X"
                            if parse_color(m.group(5)) is not None else m.group(0), text)

    for path in dark_svgs():
        text = open(path, encoding="utf8", errors="replace").read()
        after = recolor_icon(text)

        if blank(text) != blank(after):
            bad_text += 1

        first = numpy.array(render(text, 24))[..., 3].astype(int)
        second = numpy.array(render(after, 24))[..., 3].astype(int)

        if numpy.abs(first - second).max() > 0:
            bad_alpha += 1

    print("text differs beyond color values: %d. 24 px alpha masks that differ: %d (of %d)"
          % (bad_text, bad_alpha, len(dark_svgs())))


def command_recolor(args):
    os.makedirs(args.out, exist_ok=True)
    written = 0

    for path in dark_svgs():
        relative = os.path.relpath(path, os.path.join(SOURCES, "dark"))
        target = os.path.join(args.out, relative)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        text = open(path, encoding="utf8", errors="replace").read()

        with open(target, "w", encoding="utf8", newline="") as handle:
            handle.write(recolor_icon(text))

        written += 1

    print("wrote %d recolored SVGs under %s" % (written, args.out))


# --- toolbars and the preview -------------------------------------------------------------------------------------------

TOOLBAR_FILES = {
    "pcbnew": os.path.join(REPO, "pcbnew", "toolbars_pcb_editor.cpp"),
    "eeschema": os.path.join(REPO, "eeschema", "toolbars_sch_editor.cpp"),
}
ACTION_FILES = [os.path.join(REPO, "common", "tool", "actions.cpp")] + glob.glob(
    os.path.join(REPO, "pcbnew", "tools", "pcb_actions.cpp")) + glob.glob(
    os.path.join(REPO, "eeschema", "tools", "sch_actions.cpp")) + glob.glob(
    os.path.join(REPO, "pcbnew", "tools", "*actions*.cpp"))


def action_icons():
    """Qualified action name to icon name, from the TOOL_ACTION definitions."""
    icons = {}

    for path in sorted(set(ACTION_FILES)):
        text = open(path, encoding="utf8", errors="replace").read()

        for match in re.finditer(r"TOOL_ACTION\s+(\w+::\w+)\s*\(\s*TOOL_ACTION_ARGS\(\)(.*?)\)\s*;", text, re.S):
            icon = re.search(r"\.Icon\(\s*BITMAPS::(\w+)\s*\)", match.group(2))

            if icon:
                icons[match.group(1)] = icon.group(1)

    return icons


def toolbar_icons(frame, location):
    """Icon names of one toolbar's default config, in order: every AppendAction and group member (first of group)."""
    text = open(TOOLBAR_FILES[frame], encoding="utf8").read()
    start = text.index("case TOOLBAR_LOC::%s:" % location)
    end = text.index("break;", start)
    body = text[start:end]
    icons = action_icons()
    names = []

    for match in re.finditer(r"(?:AppendAction|AddAction)\(\s*(\w+::\w+)\s*\)", body):
        if match.group(1) in icons:
            names.append(icons[match.group(1)])

    return names


def toolbar_set():
    """The icons shown at their normal size: pcbnew and eeschema, top-main and left toolbars."""
    result = []

    for frame in ("pcbnew", "eeschema"):
        for location in ("TOP_MAIN", "LEFT"):
            result.append((frame, location, toolbar_icons(frame, location)))

    return result


def render(svg_text, size):
    import io
    import cairosvg
    from PIL import Image

    data = cairosvg.svg2png(bytestring=svg_text.encode("utf8"), output_width=size, output_height=size)
    return Image.open(io.BytesIO(data)).convert("RGBA")


def read_svg(theme_name, icon):
    path = os.path.join(SOURCES, theme_name, icon + ".svg")

    if not os.path.exists(path):  # the constraint pictures live one folder down, named with dashes
        path = os.path.join(SOURCES, theme_name, "constraints", icon.replace("constraint_", "", 1).replace("_", "-") + ".svg")

    if not os.path.exists(path):
        return None

    return open(path, encoding="utf8", errors="replace").read()


def on(background, icon_image):
    from PIL import Image

    base = Image.new("RGBA", icon_image.size, background)
    base.alpha_composite(icon_image)
    return base


def command_preview(args):
    from PIL import Image, ImageDraw, ImageFont

    os.makedirs(args.out, exist_ok=True)
    toolbars = toolbar_set()
    seen = []

    for frame, location, names in toolbars:
        print("%s %s: %d icons" % (frame, location, len(names)))

        for name in names:
            if name not in seen:
                seen.append(name)

    sample = seen if args.all else [seen[int(i * len(seen) / 40)] for i in range(40)]
    print("distinct toolbar icons: %d, sampled: %d" % (len(seen), len(sample)))
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 11)
    big = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 14)
    panel = tuple(round(c * 255) for c in theme.parse_hex(PANEL)) + (255,)
    plate = tuple(round(c * 255) for c in theme.parse_hex(PLATE)) + (255,)
    window = tuple(round(c * 255) for c in theme.parse_hex(WINDOW)) + (255,)
    ink = tuple(round(c * 255) for c in theme.parse_hex(INK)) + (255,)
    zoom = 3
    cell = 24 * zoom + 8

    def variants(name):
        light = read_svg("light", name)
        dark = read_svg("dark", name)
        return [("light", light), ("dark", dark), ("reckless", recolor_icon(dark) if dark else None)]

    columns = 4
    per_column = math.ceil(len(sample) / columns)
    label_width = 150
    block = label_width + 3 * cell + 12
    sheet = Image.new("RGBA", (columns * block + 8, 34 + per_column * (cell + 2) + 8), window)
    draw = ImageDraw.Draw(sheet)
    draw.text((8, 8), "stock light | stock dark | Reckless recolor, each on the panel color, 24 px shown 3x (nearest)",
              font=big, fill=ink)

    for index, name in enumerate(sample):
        column, row = divmod(index, per_column)
        x0, y0 = 8 + column * block, 34 + row * (cell + 2)
        draw.text((x0, y0 + cell // 2 - 6), name[:21], font=font, fill=ink)

        for slot, (_, svg) in enumerate(variants(name)):
            x = x0 + label_width + slot * cell
            tile = Image.new("RGBA", (cell - 2, cell - 2), panel)

            if svg:
                small = on(panel, render(svg, 24))
                tile.paste(small.resize((24 * zoom, 24 * zoom), Image.NEAREST), (4, 4))

            sheet.paste(tile, (x, y0))

    sheet.convert("RGB").save(os.path.join(args.out, "contact-sheet.png"))

    # The toolbars at their normal size (24 px, 1x) and doubled, three ways, on the toolbar plate.
    for frame, location, names in toolbars:
        rows = []

        for label, source in (("stock light", "light"), ("stock dark", "dark"), ("Reckless", "reckless")):
            icons = []

            for name in names:
                dark = read_svg("dark", name)
                svg = recolor_icon(dark) if source == "reckless" and dark else read_svg(source, name)
                icons.append(render(svg, 24) if svg else None)

            rows.append((label, icons))

        for scale in (1, 2):
            horizontal = location == "TOP_MAIN"
            step = 24 * scale + 10 * scale
            length = step * len(names) + 10
            label_h = 18
            if horizontal:
                image = Image.new("RGBA", (length + 130, 3 * (24 * scale + 14) + label_h), window)
            else:
                image = Image.new("RGBA", (3 * (24 * scale + 28) + 6, length + label_h), window)
            drawn = ImageDraw.Draw(image)
            drawn.text((4, 2), "%s %s toolbar, %dx of 24 px" % (frame, location.lower(), scale), font=font, fill=ink)

            for r, (label, icons) in enumerate(rows):
                if horizontal:
                    y = label_h + r * (24 * scale + 14)
                    drawn.rectangle([0, y, length + 130, y + 24 * scale + 12], fill=plate)
                    drawn.text((6, y + 8 * scale), label, font=font, fill=ink)

                    for i, icon in enumerate(icons):
                        if icon:
                            tile = icon if scale == 1 else icon.resize((48, 48), Image.LANCZOS)
                            image.alpha_composite(tile, (130 + 5 + i * step, y + 6))
                else:
                    x = 3 + r * (24 * scale + 28)
                    drawn.rectangle([x, label_h, x + 24 * scale + 22, label_h + length], fill=plate)

                    for i, icon in enumerate(icons):
                        if icon:
                            tile = icon if scale == 1 else icon.resize((48, 48), Image.LANCZOS)
                            image.alpha_composite(tile, (x + 11, label_h + 5 + i * step))

            image.convert("RGB").save(os.path.join(args.out, "toolbar-%s-%s-%dx.png" % (frame, location.lower(), scale)))

    # A pcbnew-shaped mock at 1x: the recolored top toolbar over the left toolbar and an empty canvas.
    frames = {(f, l): n for f, l, n in toolbars}

    for frame in ("pcbnew", "eeschema"):
        top, left = frames[(frame, "TOP_MAIN")], frames[(frame, "LEFT")]
        width, height = 34 * len(top) + 40, 34 * len(left) + 60
        mock = Image.new("RGBA", (max(width, 700), max(height, 300)), window)
        drawn = ImageDraw.Draw(mock)
        drawn.rectangle([0, 0, mock.width, 36], fill=plate)
        drawn.rectangle([0, 36, 36, mock.height], fill=plate)
        drawn.rectangle([37, 37, mock.width, mock.height], fill=panel)

        for i, name in enumerate(top):
            svg = read_svg("dark", name)
            if svg:
                mock.alpha_composite(render(recolor_icon(svg), 24), (6 + i * 34, 6))

        for i, name in enumerate(left):
            svg = read_svg("dark", name)
            if svg:
                mock.alpha_composite(render(recolor_icon(svg), 24), (6, 42 + i * 30))

        mock.convert("RGB").save(os.path.join(args.out, "mock-%s-1x.png" % frame))

    # Contrast table of the sampled icons: the lightest-to-darkest visible colors against the panel.
    with open(os.path.join(args.out, "contrast.txt"), "w") as handle:
        handle.write("icon, class, dark-set colors -> recolored (contrast on panel)\n")

        for name in seen:
            dark = read_svg("dark", name)

            if not dark:
                continue

            pairs = []

            for color in sorted(colors_of(dark)):
                rgb = theme.parse_hex(color)
                target = map_color(rgb, neutrals_only=classify(dark) == "fallback")
                pairs.append("%s %.2f -> %s %.2f" % (color, theme.contrast(rgb, theme.parse_hex(PANEL)), hex_of(target),
                                                    theme.contrast(target, theme.parse_hex(PANEL))))

            handle.write("%s [%s]: %s\n" % (name, classify(dark), "; ".join(pairs)))

    print("preview written to", args.out)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("census").set_defaults(run=command_census)
    sub.add_parser("report").set_defaults(run=command_report)
    sub.add_parser("verify").set_defaults(run=command_verify)
    recolor = sub.add_parser("recolor")
    recolor.add_argument("out")
    recolor.set_defaults(run=command_recolor)
    preview = sub.add_parser("preview")
    preview.add_argument("out")
    preview.add_argument("--all", action="store_true", help="every distinct toolbar icon, not 40 spread evenly across them")
    preview.set_defaults(run=command_preview)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()

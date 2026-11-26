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

"""Make the Reckless color theme and measure its contrast.

Every color in the theme is either a color of the Reckless look taken as it is, or the hue and chroma of one of those
colors moved to a chosen luminance. The choice is made by contrast, not by hue: each item is given a WCAG contrast
ratio against its canvas, and the script finds the color of that hue with that ratio. So the layers that must be told
apart are told apart by lightness, which reads for every kind of color vision, and the hue only adds to it.

    reckless_color_theme.py --json common/settings/reckless_color_theme.json   # write the theme
    reckless_color_theme.py --table                                           # print the contrast table
    reckless_color_theme.py --check common/settings/reckless_color_theme.json  # fail if the file is stale

Contrast is WCAG 2 relative luminance: (L1 + 0.05) / (L2 + 0.05). A translucent color is first composited over its
canvas in sRGB, as KiCad's canvases blend. 3:1 is the floor for a drawn shape.
"""

import argparse
import json
import sys

# The colors of the Reckless look, from source. Hex, then where it is defined.
SOURCES = {
    "ink": ("#e8dcc8", "reckless-toolkit look/LookMetrics.h:12 colors::kInk"),
    "amber": ("#e0a038", "reckless-toolkit look/LookMetrics.h:13 colors::kAmber"),
    "lens_rim": ("#7d7a75", "reckless-toolkit look/LookMetrics.h:14 colors::kLensRim"),
    "window": ("#29282c", "amphitheater-chess src/app/Theme.h:22 kWindow (AppTheme panel)"),
    "panel": ("#1b1a1d", "amphitheater-chess src/app/Theme.h:23 kPanel (AppTheme plate)"),
    "rule": ("#3b3936", "amphitheater-chess src/app/Theme.h:25 kRule (AppTheme track)"),
    "muted": ("#9c9483", "amphitheater-chess src/app/Theme.h:27 kMuted (AppTheme ink_dim)"),
    "faint": ("#5f5a52", "amphitheater-chess src/app/Theme.h:28 kFaint (AppTheme ink_faint)"),
    "amber_dim": ("#6e5428", "amphitheater-chess src/app/Theme.h:30 kAmberDim (AppTheme accent_dim)"),
    "good": ("#86c9a2", "amphitheater-chess src/app/Theme.h:32 kPlus (AppTheme good)"),
    "bad": ("#e2705f", "amphitheater-chess src/app/Theme.h:33 kMinus (AppTheme bad)"),
    "blue": ("#5bb3dc", "amphitheater-chess src/app/Theme.h:34 kSeriesBlue"),
    "violet": ("#b996e2", "amphitheater-chess src/app/Theme.h:35 kSeriesViolet"),
    "phase": ("#7fb8c9", "Reckless IR Capture, its app theme's kPhase (the phase strip)"),
}

# The canvas of every editor: the near-black of the series' plates and screens.
CANVAS = "panel"


# --- color arithmetic -------------------------------------------------------------------------------------------------


def parse_hex(text):
    text = text.lstrip("#")
    return tuple(int(text[i : i + 2], 16) / 255.0 for i in (0, 2, 4))


def to_linear(channel):
    return channel / 12.92 if channel <= 0.04045 else ((channel + 0.055) / 1.055) ** 2.4


def to_gamma(channel):
    return 12.92 * channel if channel <= 0.0031308 else 1.055 * channel ** (1 / 2.4) - 0.055


def luminance(rgb):
    r, g, b = (to_linear(c) for c in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(first, second):
    high, low = sorted((luminance(first), luminance(second)), reverse=True)
    return (high + 0.05) / (low + 0.05)


def composite(rgb, alpha, under):
    return tuple(c * alpha + u * (1 - alpha) for c, u in zip(rgb, under))


def quantize(rgb):
    return tuple(round(max(0.0, min(1.0, c)) * 255) / 255 for c in rgb)


def to_oklab(rgb):
    r, g, b = (to_linear(c) for c in rgb)
    l_ = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1 / 3)
    m_ = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1 / 3)
    s_ = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1 / 3)
    return (
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720174 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_,
    )


def from_oklab(lab):
    lightness, a, b = lab
    l_ = lightness + 0.3963377774 * a + 0.2158037573 * b
    m_ = lightness - 0.1055613458 * a - 0.0638541728 * b
    s_ = lightness - 0.0894841775 * a - 1.2914855480 * b
    l, m, s = l_**3, m_**3, s_**3
    linear = (
        4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
    )
    return linear


def in_gamut(linear):
    return all(-1e-6 <= c <= 1 + 1e-6 for c in linear)


def at_lightness(a, b, lightness):
    """The color of this OKLab hue direction at this lightness, its chroma reduced until it is in sRGB."""
    scale_low, scale_high = 0.0, 1.0

    if in_gamut(from_oklab((lightness, a, b))):
        scale_low = 1.0
    else:
        for _ in range(40):
            middle = (scale_low + scale_high) / 2
            if in_gamut(from_oklab((lightness, a * middle, b * middle))):
                scale_low = middle
            else:
                scale_high = middle

    linear = from_oklab((lightness, a * scale_low, b * scale_low))
    return tuple(to_gamma(max(0.0, min(1.0, c))) for c in linear)


def with_contrast(source_rgb, ratio, canvas_rgb):
    """The source's hue and chroma at the OKLab lightness that gives this contrast ratio over the canvas."""
    canvas_luminance = luminance(canvas_rgb)
    target = ratio * (canvas_luminance + 0.05) - 0.05
    _, a, b = to_oklab(source_rgb)
    low, high = 0.0, 1.0

    for _ in range(60):
        middle = (low + high) / 2
        if luminance(at_lightness(a, b, middle)) < target:
            low = middle
        else:
            high = middle

    return quantize(at_lightness(a, b, (low + high) / 2))


# --- the theme --------------------------------------------------------------------------------------------------------
#
# Each entry is (JSON path, source, contrast ratio against the canvas or None for the source as it is, alpha, role).


def item(path, source, ratio=None, alpha=1.0, role=""):
    return (path, source, ratio, alpha, role)


# Copper: the front a middling warm, so that the silkscreen over a front pour keeps its contrast; the back the dimmest
# and cool; the inner layers alternating bright and middling, so that any two neighbours in the stack differ by
# lightness (at least 1.4:1) as well as by hue.
COPPER_FRONT = ("bad", 4.6)
COPPER_BACK = ("blue", 3.0)
COPPER_INNER = [("good", 8.5), ("violet", 5.8), ("phase", 8.5), ("good", 5.8), ("violet", 8.5), ("phase", 5.8)]

# User layers repeat four looks, bright and dim in turn.
USER_LAYERS = [("lens_rim", 7.0), ("blue", 4.5), ("good", 7.0), ("phase", 4.5)]

# Gerber layers in a viewer have no stack order, so they cycle the copper looks: bright and dim in turn.
GERBER_LAYERS = [
    ("bad", 4.6),
    ("good", 8.5),
    ("violet", 5.8),
    ("phase", 8.5),
    ("good", 5.8),
    ("violet", 8.5),
    ("phase", 5.8),
    ("blue", 3.0),
]


def schematic_items():
    s = "schematic."
    return [
        item(s + "background", "panel", role="canvas"),
        item(s + "grid", "faint", role="grid"),
        item(s + "grid_axes", "muted"),
        item(s + "page_limits", "faint"),
        item(s + "worksheet", "muted", role="drawing sheet"),
        item(s + "cursor", "ink"),
        item(s + "aux_items", "ink"),
        item(s + "anchor", "muted"),
        item(s + "wire", "good", 5.0, role="wires"),
        item(s + "junction", "good", 5.0),
        item(s + "bus", "blue", 3.2, role="buses, drawn thicker"),
        item(s + "bus_junction", "blue", 3.2),
        item(s + "component_outline", "amber_dim", 3.6, role="a plate's edge, the accent at rest"),
        item(s + "component_body", "window", role="a plate"),
        item(s + "pin", "amber_dim", 3.6),
        item(s + "pin_name", "ink", 9.0),
        item(s + "pin_number", "muted"),
        item(s + "reference", "ink", role="text"),
        item(s + "value", "muted", role="secondary text"),
        item(s + "fields", "lens_rim", 4.5),
        item(s + "label_local", "ink"),
        item(s + "label_global", "phase", 11.0),
        item(s + "label_hier", "violet", 5.0),
        item(s + "netclass_flag", "muted"),
        item(s + "no_connect", "blue", 6.0),
        item(s + "note", "ink"),
        item(s + "private_note", "muted"),
        item(s + "note_background", "panel", alpha=0.0),
        item(s + "sheet", "violet", 6.0),
        item(s + "sheet_background", "window"),
        item(s + "sheet_name", "ink"),
        item(s + "sheet_filename", "muted"),
        item(s + "sheet_fields", "muted"),
        item(s + "sheet_label", "violet", 5.0),
        item(s + "hidden", "muted", 3.0, role="hidden pins and fields"),
        item(s + "brightened", "amber", role="highlighted net"),
        item(s + "hovered", "amber"),
        item(s + "shadow", "amber", alpha=0.5, role="selection: the accent at half strength"),
        item(s + "erc_error", "bad", 7.5, alpha=0.8),
        item(s + "erc_warning", "amber", 4.5, alpha=0.8),
        item(s + "erc_exclusion", "muted", alpha=0.8),
        item(s + "dnp_marker", "bad", 6.0, alpha=0.85),
        item(s + "excluded_from_sim", "muted", alpha=0.95),
        item(s + "drag_net_collision", "bad", 7.5, alpha=0.8),
        item(s + "rule_area", "violet", 6.0),
        item(s + "op_voltages", "phase", 8.0),
        item(s + "op_currents", "bad", 6.0),
    ]


def board_items():
    b = "board."
    items = [
        item(b + "background", "panel", role="canvas"),
        item(b + "grid", "faint", role="grid"),
        item(b + "grid_axes", "muted"),
        item(b + "subgrids", "rule"),
        item(b + "page_limits", "faint"),
        item(b + "worksheet", "muted", role="drawing sheet"),
        item(b + "cursor", "ink"),
        item(b + "aux_items", "ink"),
        item(b + "anchor", "muted"),
        item(b + "points", "violet", 6.0),
        item(b + "outline_area", "rule", alpha=0.5),
        item(b + "locked_shadow", "amber", alpha=0.5),
        item(b + "conflicts_shadow", "bad", alpha=0.5),
        item(b + "constraint_shadow", "blue", alpha=0.5),
        item(b + "constraint_under", "blue", 4.0, alpha=0.9),
        item(b + "constraint_well", "good", 8.0, alpha=0.9),
        item(b + "constraint_over", "bad", 5.5, alpha=0.9),
        item(b + "drc_error", "bad", 4.5, alpha=0.8),
        item(b + "drc_warning", "amber", alpha=0.8),
        item(b + "drc_exclusion", "muted", alpha=0.8),
        item(b + "drc_highlighted", "amber", role="the item of a chosen DRC marker"),
        item(b + "ratsnest", "ink", alpha=0.45),
        item(b + "pad_plated_hole", "panel", role="unused: KiCad draws it in the canvas color"),
        item(b + "plated_hole", "phase", 6.0, role="non-plated holes"),
        item(b + "via_hole", "faint"),
        item(b + "via_hole_walls", "muted"),
        item(b + "track_net_names", "ink", alpha=0.7),
        item(b + "pad_net_names", "ink", alpha=0.9),
        item(b + "via_net_names", "panel", alpha=0.9),
        item(b + "via_stitching", "good", 6.0),
        item(b + "copper.f", *COPPER_FRONT, role="front copper"),
    ]

    for index in range(1, 31):
        source, ratio = COPPER_INNER[(index - 1) % len(COPPER_INNER)]
        items.append(item(b + "copper.in%d" % index, source, ratio))

    items += [
        item(b + "copper.b", *COPPER_BACK, role="back copper"),
        item(b + "f_silks", "ink", role="front silkscreen"),
        item(b + "b_silks", "muted", 8.5, role="back silkscreen"),
        item(b + "f_mask", "violet", 9.0, alpha=0.4),
        item(b + "b_mask", "phase", 3.0, alpha=0.4),
        item(b + "f_paste", "lens_rim", 9.5, alpha=0.9),
        item(b + "b_paste", "phase", 4.0, alpha=0.9),
        item(b + "f_adhes", "violet", 4.0),
        item(b + "b_adhes", "blue", 3.0),
        item(b + "f_fab", "muted", 3.2),
        item(b + "b_fab", "muted", 4.4),
        item(b + "f_crtyd", "violet", 7.0),
        item(b + "b_crtyd", "phase", 6.2),
        item(b + "edge_cuts", "lens_rim", 9.0, role="board outline"),
        item(b + "margin", "amber_dim", 4.5),
        item(b + "dwgs_user", "lens_rim", 7.0),
        item(b + "cmts_user", "blue", 4.5),
        item(b + "eco1_user", "good", 7.0),
        item(b + "eco2_user", "phase", 4.5),
    ]

    for index in range(1, 46):
        source, ratio = USER_LAYERS[(index - 1) % len(USER_LAYERS)]
        items.append(item(b + "user_%d" % index, source, ratio))

    return items


def gerbview_items():
    g = "gerbview."
    items = [
        item(g + "background", "panel", role="canvas"),
        item(g + "grid", "faint"),
        item(g + "axes", "muted"),
        item(g + "dcodes", "ink"),
        item(g + "negative_objects", "faint"),
        item(g + "worksheet", "muted"),
        item(g + "page_limits", "faint"),
    ]

    # KiCad themes the first 64 gerber layers (its default table stops there); the rest keep their default.
    for index in range(64):
        source, ratio = GERBER_LAYERS[index % len(GERBER_LAYERS)]
        items.append(item(g + "layers.%d" % index, source, ratio))

    return items


def viewer_3d_items():
    v = "3d_viewer."
    # The board, copper, mask and paste keep KiCad's physical colors (and a board's stackup overrides them).
    items = [
        item(v + "background_top", "rule", role="the gradient's top"),
        item(v + "background_bottom", "panel", role="the gradient's foot"),
        item(v + "silkscreen_top", "ink"),
        item(v + "silkscreen_bottom", "ink"),
        item(v + "user_comments", "blue", 4.5),
        item(v + "user_drawings", "lens_rim", 7.0),
        item(v + "user_eco1", "good", 7.0),
        item(v + "user_eco2", "phase", 4.5),
        item(v + "front_fab", "muted", 3.2),
        item(v + "back_fab", "muted", 4.4),
        item(v + "front_courtyard", "violet", 7.0),
        item(v + "back_courtyard", "phase", 6.2),
        item(v + "f_fab", "muted", 3.2),
        item(v + "b_fab", "muted", 4.4),
        item(v + "f_courtyard", "violet", 7.0),
        item(v + "b_courtyard", "phase", 6.2),
    ]

    for index in range(1, 46):
        source, ratio = USER_LAYERS[(index - 1) % len(USER_LAYERS)]
        items.append(item(v + "user_%d" % index, source, ratio))

    return items


def all_items():
    return schematic_items() + board_items() + gerbview_items() + viewer_3d_items()


def resolve(source, ratio):
    rgb = parse_hex(SOURCES[source][0])

    if ratio is None:
        return rgb

    return with_contrast(rgb, ratio, parse_hex(SOURCES[CANVAS][0]))


def css(rgb, alpha):
    r, g, b = (round(c * 255) for c in rgb)

    if alpha >= 1.0:
        return "rgb(%d, %d, %d)" % (r, g, b)

    return "rgba(%d, %d, %d, %.3f)" % (r, g, b, alpha)


def hex_of(rgb):
    return "#%02x%02x%02x" % tuple(round(c * 255) for c in rgb)


def build_json():
    document = {"meta": {"name": "Reckless", "version": 6}, "schematic": {"override_item_colors": False}}

    for path, source, ratio, alpha, _ in all_items():
        node = document
        parts = path.split(".")

        for part in parts[:-1]:
            node = node.setdefault(part, {})

        node[parts[-1]] = css(resolve(source, ratio), alpha)

    # KiCad stores the gerber layers as an array.
    layers = document["gerbview"].pop("layers")
    document["gerbview"]["layers"] = [layers[str(index)] for index in range(len(layers))]
    return document


def render_json():
    return json.dumps(build_json(), indent=2) + "\n"


# --- the contrast table -----------------------------------------------------------------------------------------------


def color_of(path):
    for item_path, source, ratio, alpha, _ in all_items():
        if item_path == path:
            return resolve(source, ratio), alpha

    raise KeyError(path)


def on_canvas(path):
    rgb, alpha = color_of(path)
    return composite(rgb, alpha, parse_hex(SOURCES[CANVAS][0]))


def brightened(rgb, factor):
    return tuple(c * (1 - factor) + factor for c in rgb)


def darkened(rgb, factor):
    return tuple(c * (1 - factor) for c in rgb)


def kicad_brightness(rgb):
    r, g, b = rgb
    return r * 0.299 + g * 0.587 + b * 0.117


def pcb_selected(rgb):
    """The PCB editors' selected color (RENDER_SETTINGS::update, select_factor 0.75)."""
    factor = min(1.0, 0.75 * 0.5 + kicad_brightness(rgb) ** 3)
    return brightened(rgb, factor)


def table_rows():
    canvas = parse_hex(SOURCES[CANVAS][0])
    rows = []

    for path, source, ratio, alpha, role in all_items():
        rgb = resolve(source, ratio)
        shown = composite(rgb, alpha, canvas)
        how = "as is" if ratio is None else "hue at %.1f:1" % ratio
        rows.append((path, css(rgb, alpha), hex_of(rgb), source, how, contrast(shown, canvas), role))

    return rows


def pair_rows():
    """Pairs that must be told apart, with their contrast against each other."""
    pairs = [
        ("board.f_silks", "board.copper.f", "front silk over front copper"),
        ("board.b_silks", "board.copper.b", "back silk over back copper"),
        ("board.copper.f", "board.copper.b", "front against back copper"),
        ("board.copper.f", "board.copper.in1", "front against In1"),
    ]

    for index in range(1, 30):
        pairs.append(("board.copper.in%d" % index, "board.copper.in%d" % (index + 1), "neighbours in the stack"))

    pairs += [
        ("board.copper.in2", "board.copper.b", "last inner of a 4-layer board against back"),
        ("board.copper.in4", "board.copper.b", "last inner of a 6-layer board against back"),
        ("board.copper.in6", "board.copper.b", "last inner of an 8-layer board against back"),
        ("board.f_silks", "board.edge_cuts", "front silk against the board outline"),
        ("board.f_fab", "board.b_fab", "front against back fab"),
        ("board.f_crtyd", "board.b_crtyd", "front against back courtyard"),
        ("board.f_mask", "board.b_mask", "front against back mask (composited)"),
        ("board.copper.f", "board.f_crtyd", "front copper against front courtyard"),
        ("board.copper.f", "board.f_fab", "front copper against front fab"),
        ("board.copper.b", "board.b_fab", "back copper against back fab"),
        ("board.b_fab", "board.b_crtyd", "back fab against back courtyard"),
        ("board.b_crtyd", "board.b_silks", "back courtyard against back silk"),
        ("board.f_crtyd", "board.f_silks", "front courtyard against front silk"),
        ("board.f_crtyd", "board.edge_cuts", "front courtyard against the board outline"),
        ("board.drc_error", "board.drc_warning", "DRC error against warning"),
        ("schematic.wire", "schematic.bus", "wire against bus"),
        ("schematic.wire", "schematic.component_outline", "wire against a symbol's outline"),
        ("schematic.wire", "schematic.brightened", "a wire against a highlighted wire"),
        ("schematic.component_outline", "schematic.brightened", "an outline against a highlighted pin"),
        ("schematic.label_local", "schematic.brightened", "a local label against a highlighted one"),
        ("schematic.label_global", "schematic.brightened", "a global label against a highlighted one"),
        ("schematic.label_hier", "schematic.brightened", "a hierarchical label against a highlighted one"),
        ("schematic.label_local", "schematic.label_global", "local against global label"),
        ("schematic.label_global", "schematic.label_hier", "global against hierarchical label"),
        ("schematic.reference", "schematic.value", "reference against value"),
        ("schematic.erc_error", "schematic.erc_warning", "ERC error against warning"),
        ("schematic.reference", "schematic.shadow", "text of a selected symbol on its selection halo"),
        ("schematic.wire", "schematic.shadow", "a selected wire on its selection halo"),
        ("schematic.component_outline", "schematic.shadow", "a selected outline on its selection halo"),
        ("schematic.reference", "schematic.component_body", "text on a symbol's body"),
        ("schematic.component_body", "schematic.background", "a symbol's body against the canvas"),
    ]

    rows = []

    for first, second, what in pairs:
        rows.append((first, second, contrast(on_canvas(first), on_canvas(second)), what))

    return rows


def selection_rows():
    """How the PCB editors' selection and net highlight change a layer, by luminance."""
    rows = []

    for path in ["board.copper.f", "board.copper.in1", "board.copper.in2", "board.copper.b", "board.f_silks",
                 "board.edge_cuts", "board.f_fab", "board.f_crtyd"]:
        rgb, _ = color_of(path)
        rows.append((path, contrast(pcb_selected(rgb), rgb), contrast(brightened(rgb, 0.5), darkened(rgb, 0.5))))

    return rows


def render_table():
    lines = ["Contrast is WCAG 2: (L1 + 0.05) / (L2 + 0.05), translucent colors composited over the canvas "
             "%s (%s)." % (SOURCES[CANVAS][0], CANVAS), "", "Sources:", ""]

    for name, (value, where) in SOURCES.items():
        lines.append("  %-10s %s  %s" % (name, value, where))

    lines += ["", "Every item against the canvas:", "",
              "| Item | Value | Hex | Source | How | Contrast | Role |", "|---|---|---|---|---|---|---|"]

    for path, value, hex_value, source, how, ratio, role in table_rows():
        lines.append("| %s | %s | %s | %s | %s | %.2f | %s |" % (path, value, hex_value, source, how, ratio, role))

    lines += ["", "Pairs that must be told apart:", "", "| First | Second | Contrast | What |", "|---|---|---|---|"]

    for first, second, ratio, what in pair_rows():
        lines.append("| %s | %s | %.2f | %s |" % (first, second, ratio, what))

    lines += ["", "PCB editors (selection and highlight are computed from the layer color, not themed):", "",
              "| Layer | Selected against normal | Highlighted net against the dimmed rest |", "|---|---|---|"]

    for path, selected, highlighted in selection_rows():
        lines.append("| %s | %.2f | %.2f |" % (path, selected, highlighted))

    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--json", help="write the theme to this file")
    parser.add_argument("--check", help="fail if this theme file differs from what the script makes")
    parser.add_argument("--table", action="store_true", help="print the contrast table")
    arguments = parser.parse_args()

    if arguments.json:
        with open(arguments.json, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(render_json())

    if arguments.check:
        with open(arguments.check, encoding="utf-8") as stream:
            if stream.read() != render_json():
                print("%s is stale: run %s --json %s" % (arguments.check, sys.argv[0], arguments.check))
                return 1

    if arguments.table:
        sys.stdout.write(render_table())

    return 0


if __name__ == "__main__":
    sys.exit(main())

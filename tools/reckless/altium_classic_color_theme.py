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

"""Make the Altium classic color theme.

The look of Altium Designer's classic 2D colors, for the interaction profile that mirrors its keys and views: in the
PCB editors the top layer red, the bottom layer blue, the overlay yellow and the multi-layer items gray on a black
canvas; in the schematic editors a white sheet, dark-blue wires, blue parts and dark-red labels. Any other theme stays
selectable in Preferences > Colors.

The colors are written out here, not derived. Two are lifted from the classic values so that they read on the black
canvas for every kind of color vision, by lightness and not by hue alone: the bottom layer's blue is 4.5:1 against the
canvas where the classic pure blue is 1.4:1, and the back overlay is a dim yellow with the same lift. Nothing else is
moved.

    altium_classic_color_theme.py --json common/settings/altium_classic_color_theme.json   # write the theme
    altium_classic_color_theme.py --table                                                  # print the contrast table
    altium_classic_color_theme.py --check common/settings/altium_classic_color_theme.json  # fail if the file is stale

Contrast is WCAG 2 relative luminance: (L1 + 0.05) / (L2 + 0.05), a translucent color first composited over its canvas.
"""

import argparse
import json
import sys

# (red, green, blue) in 0..255, or (red, green, blue, alpha) with alpha in 0..1.

BOARD_CANVAS = (0, 0, 0)
SCHEMATIC_CANVAS = (255, 255, 255)

WHITE = (255, 255, 255)
RED = (255, 0, 0)
BOTTOM_BLUE = (51, 102, 255)  # the classic blue (0, 0, 255), lifted to 4.5:1 on the black canvas
YELLOW = (255, 255, 0)
BACK_YELLOW = (170, 170, 0)
MULTI_LAYER_GRAY = (133, 133, 133)
MAGENTA = (255, 0, 255)
DARK_MAGENTA = (176, 0, 176)
CYAN = (0, 200, 200)
GREEN = (0, 200, 0)
ORANGE = (255, 153, 0)

# The inner layers cycle through these, so that neighbours in the stack differ by hue and by lightness.
INNER_LAYERS = [(255, 153, 0), (0, 200, 0), (230, 90, 230), (0, 200, 200), (190, 190, 0), (160, 110, 255)]

# User layers repeat these four looks.
USER_LAYERS = [(200, 200, 200), (128, 160, 255), (0, 255, 0), (255, 153, 0)]

GERBER_LAYERS = [RED, BOTTOM_BLUE, YELLOW, GREEN, MAGENTA, CYAN, ORANGE, (200, 200, 200)]

# The schematic's colors are the classic sheet's. KiCad draws a power port in the symbol's outline color: there is no
# separate color for it, so its blue is the part blue.
SCHEMATIC = {
    "background": (255, 255, 255),
    "wire": (0, 0, 128),
    "bus": (0, 0, 128),
    "bus_junction": (0, 0, 128),
    "junction": (0, 0, 128),
    "label_local": (128, 0, 0),
    "label_global": (128, 0, 0),
    "label_hier": (128, 0, 0),
    "netclass_flag": (255, 0, 0),
    "pin": (0, 0, 0),
    "pin_name": (0, 0, 0),
    "pin_number": (0, 0, 0),
    "component_outline": (0, 0, 255),
    "component_body": (255, 255, 176),
    "reference": (0, 0, 128),
    "value": (0, 0, 128),
    "fields": (0, 0, 128),
    "sheet": (128, 0, 0),
    "sheet_background": (128, 255, 128, 1.0),
    "sheet_filename": (0, 0, 128),
    "sheet_name": (0, 0, 128),
    "sheet_fields": (0, 0, 128),
    "sheet_label": (128, 0, 0),
    "note": (0, 0, 128),
    "note_background": (0, 0, 0, 0.0),
    "private_note": (0, 0, 128),
    "no_connect": (255, 0, 0),
    "worksheet": (0, 0, 0),
    "page_limits": (181, 181, 181),
    "hidden": (132, 132, 132),
    "grid": (204, 204, 204),
    "grid_axes": (0, 0, 132),
    "anchor": (0, 0, 255),
    "aux_items": (0, 0, 0),
    "erc_error": (230, 9, 13, 0.8),
    "erc_warning": (209, 146, 0, 0.8),
    "dnp_marker": (220, 9, 13, 0.906),
    "excluded_from_sim": (0, 0, 0, 0.157),
    "shadow": (199, 235, 255, 0.8),
    "cursor": (0, 0, 0),
    "hovered": (0, 0, 255),
    "brightened": (255, 0, 255),
    "op_voltages": (132, 0, 50),
    "op_currents": (224, 0, 12),
    "rule_area": (255, 0, 0, 0.8),
    "drag_net_collision": (128, 0, 0, 0.8),
    "erc_exclusion": (194, 194, 194, 0.8),
}


def board_colors():
    colors = {
        "background": BOARD_CANVAS,
        "grid": (90, 90, 90),
        "grid_axes": (200, 200, 200),
        "subgrids": (45, 45, 45),
        "page_limits": (90, 90, 90),
        "worksheet": (200, 200, 200),
        "cursor": WHITE,
        "aux_items": WHITE,
        "anchor": YELLOW,
        "points": (160, 110, 255),
        "outline_area": (90, 90, 90, 0.5),
        "locked_shadow": (255, 255, 255, 0.35),
        "conflicts_shadow": (255, 64, 64, 0.5),
        "constraint_shadow": BOTTOM_BLUE + (0.5,),
        "constraint_under": BOTTOM_BLUE + (0.9,),
        "constraint_well": GREEN + (0.9,),
        "constraint_over": (255, 64, 64, 0.9),
        "drc_error": (255, 64, 64, 0.8),
        "drc_warning": (255, 215, 0, 0.8),
        "drc_exclusion": (180, 180, 180, 0.8),
        "drc_highlighted": YELLOW,
        "ratsnest": (210, 210, 210, 0.6),
        "pad_plated_hole": BOARD_CANVAS,
        "plated_hole": MULTI_LAYER_GRAY,
        "via_hole": MULTI_LAYER_GRAY,
        "via_hole_walls": (190, 190, 190),
        "track_net_names": (255, 255, 255, 0.7),
        "pad_net_names": (255, 255, 255, 0.9),
        "via_net_names": (0, 0, 0, 0.9),
        "via_stitching": GREEN,
        "f_silks": YELLOW,
        "b_silks": BACK_YELLOW,
        "f_mask": (160, 32, 160, 0.5),
        "b_mask": (140, 80, 255, 0.5),
        "f_paste": (0, 150, 150, 0.9),
        "b_paste": (120, 190, 190, 0.9),
        "f_adhes": (160, 160, 255),
        "b_adhes": (110, 110, 200),
        "f_fab": (255, 128, 0),
        "b_fab": (190, 100, 30),
        "f_crtyd": CYAN,
        "b_crtyd": (0, 150, 170),
        "edge_cuts": MAGENTA,
        "margin": DARK_MAGENTA,
        "dwgs_user": USER_LAYERS[0],
        "cmts_user": USER_LAYERS[1],
        "eco1_user": USER_LAYERS[2],
        "eco2_user": USER_LAYERS[3],
    }

    for index in range(1, 46):
        colors["user_%d" % index] = USER_LAYERS[(index - 1) % len(USER_LAYERS)]

    return colors


def css(color):
    red, green, blue = color[:3]

    if len(color) == 3 or color[3] >= 1.0:
        return "rgb(%d, %d, %d)" % (red, green, blue)

    return "rgba(%d, %d, %d, %.3f)" % (red, green, blue, color[3])


def build_json():
    board = {key: css(value) for key, value in board_colors().items()}
    board["copper"] = {"f": css(RED)}

    for index in range(1, 31):
        board["copper"]["in%d" % index] = css(INNER_LAYERS[(index - 1) % len(INNER_LAYERS)])

    board["copper"]["b"] = css(BOTTOM_BLUE)

    schematic = {"override_item_colors": False}
    schematic.update({key: css(value) for key, value in SCHEMATIC.items()})

    gerbview = {
        "background": css(BOARD_CANVAS),
        "grid": css((90, 90, 90)),
        "axes": css((200, 200, 200)),
        "dcodes": css(WHITE),
        "negative_objects": css((90, 90, 90)),
        "worksheet": css((200, 200, 200)),
        "page_limits": css((90, 90, 90)),
        "layers": [css(GERBER_LAYERS[index % len(GERBER_LAYERS)]) for index in range(64)],
    }

    return {
        "meta": {"name": "Altium classic", "version": 6},
        "schematic": schematic,
        "board": board,
        "gerbview": gerbview,
    }


def render_json():
    return json.dumps(build_json(), indent=2) + "\n"


def luminance(color):
    def linear(channel):
        channel /= 255.0
        return channel / 12.92 if channel <= 0.04045 else ((channel + 0.055) / 1.055) ** 2.4

    red, green, blue = (linear(channel) for channel in color[:3])
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue


def composite(color, canvas):
    alpha = color[3] if len(color) == 4 else 1.0
    return tuple(c * alpha + u * (1 - alpha) for c, u in zip(color[:3], canvas))


def contrast(color, canvas):
    high, low = sorted((luminance(composite(color, canvas)), luminance(canvas)), reverse=True)
    return (high + 0.05) / (low + 0.05)


def render_table():
    lines = ["Contrast against the canvas (WCAG 2), translucent colors composited over it.", ""]

    lines.append("PCB on black:")
    for key, value in board_colors().items():
        lines.append("  board.%-18s %-26s %.2f" % (key, css(value), contrast(value, BOARD_CANVAS)))

    for name, value in [("copper.f", RED), ("copper.b", BOTTOM_BLUE)] + [
        ("copper.in%d" % (i + 1), v) for i, v in enumerate(INNER_LAYERS)
    ]:
        lines.append("  board.%-18s %-26s %.2f" % (name, css(value), contrast(value, BOARD_CANVAS)))

    lines += ["", "Schematic on white:"]
    for key, value in SCHEMATIC.items():
        lines.append("  schematic.%-18s %-26s %.2f" % (key, css(value), contrast(value, SCHEMATIC_CANVAS)))

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

#
# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright The KiCad Developers, see AUTHORS.txt for contributors.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 3
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

"""A pad's rectangular hole through kicad-cli's non-fabrication exporters.

rectangular_holes/fab.kicad_pcb holds four rectangular holes, a round hole and an oval slot.
VRML and Specctra write the rectangle; HyperLynx and GenCAD, whose drill is one diameter, write it
round and say so once; the same goes for the oval slot.
"""

import math
import re

from conftest import KiTestFixture
import utils


# Reference: (center x, center y, width, height, degrees, plated), in board millimeters
RECTANGULAR_HOLES = {
    "SQ": (20.0, 20.0, 2.0, 2.0, 0.0, True),
    "NR": (30.0, 20.0, 3.0, 2.0, 0.0, False),
    "R30": (40.0, 20.0, 2.5, 1.5, 30.0, True),
    "R90": (70.0, 20.0, 2.5, 1.5, 90.0, True),
}


def _export(kitest: KiTestFixture, fmt: str, name: str, *options: str):
    board = kitest.get_data_file_path("pcbnew/rectangular_holes/fab.kicad_pcb")
    target = kitest.get_output_path("cli/exporter_rectangular_holes/") / name
    # The output directory outlives a run
    target.unlink(missing_ok=True)
    command = [utils.kicad_cli(), "pcb", "export", fmt, *options, "-o", str(target), str(board)]
    stdout, stderr, exitcode = utils.run_and_capture(command)
    return target, stdout + stderr, exitcode


def _corners(center_x, center_y, width, height, degrees):
    """The corners in a Y-up frame: KiCad's Y axis points down, and its angles turn counterclockwise."""
    angle = math.radians(degrees)
    corners = []
    for x, y in ((-width / 2, -height / 2), (width / 2, -height / 2), (width / 2, height / 2), (-width / 2, height / 2)):
        corners.append((center_x + x * math.cos(angle) - y * math.sin(angle),
                        -center_y + x * math.sin(angle) + y * math.cos(angle)))
    return corners


def test_vrml_cuts_the_rectangle(kitest: KiTestFixture):
    target, output, exitcode = _export(kitest, "vrml", "fab.wrl", "--units", "mm", "--user-origin", "0x0mm")
    assert exitcode == 0, output

    wrl = target.read_text(encoding="utf-8")

    # The board's points are in tenths of an inch inside an outer Transform that scales them to mm
    scale = float(re.search(r"scale ([0-9.]+) ", wrl).group(1))
    points = []
    for block in re.findall(r"point\s*\[([^\]]*)\]", wrl):
        values = [float(value) * scale for value in re.split(r"[\s,]+", block.strip()) if value]
        points.extend(zip(values[0::3], values[1::3]))

    assert points, "the VRML file holds no geometry"

    # The board's cutout runs through the hole's corners, a plated one's 0.0025 mm outside them on
    # each side, as a round plated hole is cut 0.005 mm wider (PLATE_OFFSET); a round hole of the
    # rectangle's X size would pass nowhere near a corner
    for reference, (center_x, center_y, width, height, degrees, _plated) in RECTANGULAR_HOLES.items():
        for corner_x, corner_y in _corners(center_x, center_y, width, height, degrees):
            assert any(abs(x - corner_x) < 4e-3 and abs(y - corner_y) < 4e-3 for x, y in points), \
                f"{reference}: corner ({corner_x:.4f}, {corner_y:.4f}) missing"


def test_specctra_keeps_out_the_unplated_rectangle(kitest: KiTestFixture):
    target, output, exitcode = _export(kitest, "specctra", "fab.dsn")
    assert exitcode == 0, output

    dsn = target.read_text(encoding="utf-8")
    image = re.search(r'\(image "?probe:NR"?(.*?)(?=\(image |\(padstack )', dsn, re.S)
    assert image, "no image for the unplated rectangle"
    assert "(pin " not in image.group(1)
    # With a hole clearance the keepout is a polygon with rounded corners, not a rect
    assert len(re.findall(r'\(keepout "" \(polygon ', image.group(1))) == 2
    assert '(rect ' not in image.group(1)


def test_hyperlynx_reports_the_rectangle_once(kitest: KiTestFixture):
    target, output, exitcode = _export(kitest, "hyperlynx", "fab.hyp")
    assert exitcode == 0, output
    assert target.exists()
    assert output.count("Hyperlynx has no rectangular hole: 4 rectangular pad holes") == 1, output


def test_hyperlynx_reports_the_slot_once(kitest: KiTestFixture):
    target, output, exitcode = _export(kitest, "hyperlynx", "slot.hyp")
    assert exitcode == 0, output
    assert output.count("Hyperlynx has no oval slot: 1 oval pad slots") == 1, output


def test_gencad_reports_the_slot_once(kitest: KiTestFixture):
    target, output, exitcode = _export(kitest, "gencad", "slot.cad")
    assert exitcode == 0, output
    assert output.count("GenCAD has no oval slot: 1 oval pad slots") == 1, output


def test_gencad_reports_the_rectangle_once(kitest: KiTestFixture):
    target, output, exitcode = _export(kitest, "gencad", "fab.cad")
    assert exitcode == 0, output
    assert target.exists()
    assert output.count("GenCAD has no rectangular hole: 4 rectangular pad holes") == 1, output


def test_idf_drills_only_the_round_hole(kitest: KiTestFixture):
    target, output, exitcode = _export(kitest, "idf", "fab.emn")
    assert exitcode == 0, output

    emn = target.read_text(encoding="utf-8")
    holes = re.search(r"\.DRILLED_HOLES\n(.*?)\.END_DRILLED_HOLES", emn, re.S)
    assert holes, "no DRILLED_HOLES section"
    records = [line.split() for line in holes.group(1).splitlines() if line.strip()]
    assert len(records) == 1, records
    assert math.isclose(float(records[0][0]), 1.0)

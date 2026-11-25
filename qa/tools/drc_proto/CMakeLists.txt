#
# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright (C) 2017 CERN
# @author Alejandro García Montoro <alejandro.garciamontoro@gmail.com>
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

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
find_package( wxWidgets 3.0.0 COMPONENTS gl aui adv html core net base xml stc REQUIRED )


add_compile_definitions( BOOST_TEST_DYN_LINK PCBNEW DRC_PROTO TEST_APP_NO_MAIN)

add_executable( drc_proto
    drc_proto_test.cpp
    drc_proto.cpp
    ../../../pcbnew/net_chain_bridging.cpp
    ../../../pcbnew/drc/drc_creepage_engine.cpp
    ../../../pcbnew/drc/drc_creepage_utils.cpp
    ../../../pcbnew/drc/drc_rule.cpp
    ../../../pcbnew/drc/drc_rule_condition.cpp
    ../../../pcbnew/drc/drc_rule_parser.cpp
    ../../../pcbnew/drc/drc_test_provider.cpp
    ../../../pcbnew/drc/drc_test_provider_creepage.cpp
    ../../../pcbnew/drc/drc_test_provider_copper_clearance.cpp
    ../../../pcbnew/board_stackup_manager/stackup_predefined_prms.cpp
    ../../../pcbnew/drc/drc_test_provider_hole_to_hole.cpp
    ../../../pcbnew/drc/drc_test_provider_edge_clearance.cpp
    ../../../pcbnew/drc/drc_test_provider_hole_size.cpp
    ../../../pcbnew/drc/drc_test_provider_disallow.cpp
    ../../../pcbnew/drc/drc_test_provider_track_width.cpp
    ../../../pcbnew/drc/drc_test_provider_annular_width.cpp
    ../../../pcbnew/drc/drc_test_provider_connectivity.cpp
    ../../../pcbnew/drc/drc_test_provider_courtyard_clearance.cpp
    ../../../pcbnew/drc/drc_test_provider_via_diameter.cpp
    ../../../pcbnew/drc/drc_test_provider_schematic_parity.cpp
    ../../../pcbnew/drc/drc_test_provider_misc.cpp
    ../../../pcbnew/drc/drc_test_provider_solder_mask.cpp
    ../../../pcbnew/drc/drc_test_provider_silk_clearance.cpp
    ../../../pcbnew/drc/drc_test_provider_matched_length.cpp
    ../../../pcbnew/drc/drc_test_provider_diff_pair_coupling.cpp
    ../../../pcbnew/drc/drc_chain_topology.cpp
    ../../../pcbnew/drc/drc_engine.cpp
    ../../../pcbnew/drc/drc_item.cpp
    ../../qa_utils/mocks.cpp
    ../../pcbnew_utils/board_file_utils.cpp
    ../../qa_utils/test_app_main.cpp
    ../../qa_utils/utility_program.cpp
    ../../qa_utils/stdstream_line_reader.cpp
)

# A GUI wxApp: without the manifest, wxWidgets stops it at startup on Windows
kicad_qa_tool_manifest( drc_proto )

add_dependencies( drc_proto pnsrouter pcbcommon ${PCBNEW_IO_LIBRARIES} )

include_directories( BEFORE ${INC_BEFORE} )
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3d-viewer
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/pcbnew
    ${CMAKE_SOURCE_DIR}/pcbnew/router
    ${CMAKE_SOURCE_DIR}/pcbnew/tools
    ${CMAKE_SOURCE_DIR}/pcbnew/dialogs
    ${CMAKE_SOURCE_DIR}/polygon
    ${CMAKE_SOURCE_DIR}/common/geometry
    ${CMAKE_SOURCE_DIR}/libs/kimath/include/math
    ${CMAKE_SOURCE_DIR}/qa/common
    ${CMAKE_SOURCE_DIR}/qa
    ${CMAKE_SOURCE_DIR}/qa/qa_utils
    ${CMAKE_SOURCE_DIR}/qa/qa_utils/include
    ${CMAKE_SOURCE_DIR}/qa/pcbnew_utils/include
    ${INC_AFTER}
)

target_link_libraries( drc_proto
    qa_pcbnew_utils
    connectivity
    pcbcommon
    pnsrouter
    gal
    common
    gal
    qa_utils
    dxflib_qcad
    tinyspline_lib
    nanosvg
    markdown_lib
    # A static library only resolves symbols from the libraries after it, and these three refer to one another in a
    # circle: pcbcommon's settings need EDA_3D_VIEWER_SETTINGS from 3d-viewer, 3d-viewer's board adapter needs
    # PROJECT_PCB from pcbcommon, and that pulls in PCB_IO_MGR, which needs every PCB_IO_* in the IO libraries, which
    # in turn need S3D_CACHE from 3d-viewer. So the order is pcbcommon, 3d-viewer, pcbcommon, the IO libraries, then
    # 3d-viewer again. Do not collapse the repeats, and do not drop a library: the link is order-sensitive with
    # GNU ld, though MSVC does not mind.
    3d-viewer
    pcbcommon
    Boost::headers
    ${PCBNEW_IO_LIBRARIES}
    3d-viewer
    ${wxWidgets_LIBRARIES}
    ${GDI_PLUS_LIBRARIES}
    ${PCBNEW_EXTRA_LIBS}    # -lrt must follow Boost
)

# Command-line behavior: help is not a board, a missing board is an error and not a crash, and a real board runs to the end
# (a wx assert dialog used to block it for ever when nobody could dismiss it). Like the other tests, run under xvfb-run.
add_test( NAME drc_proto_help COMMAND drc_proto --help )
set_tests_properties( drc_proto_help PROPERTIES PASS_REGULAR_EXPRESSION "usage:" )

add_test( NAME drc_proto_missing_board COMMAND drc_proto ${CMAKE_CURRENT_BINARY_DIR}/no_such_board.kicad_pcb )
set_tests_properties( drc_proto_missing_board PROPERTIES PASS_REGULAR_EXPRESSION "cannot load a board" TIMEOUT 60 )

add_test( NAME drc_proto_runs_to_completion
          COMMAND drc_proto ${CMAKE_SOURCE_DIR}/qa/data/pcbnew/issue18839.kicad_pcb )
set_tests_properties( drc_proto_runs_to_completion PROPERTIES TIMEOUT 300 )

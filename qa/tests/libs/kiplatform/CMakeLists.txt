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

# The window chrome code of the platform layer: it needs a display, and it changes the process's GTK theme, so it has
# an executable of its own rather than sharing qa_common's process.
add_executable( qa_kiplatform
    kiplatform_test_module.cpp
    test_reckless_chrome.cpp
    test_interface_font.cpp
    )

target_link_libraries( qa_kiplatform
    kiplatform
    Boost::headers
    Boost::unit_test_framework
    ${wxWidgets_LIBRARIES}
    )

# It starts wx, which on Windows stops at a modal warning box without the Common Controls v6 manifest
kicad_qa_tool_manifest( qa_kiplatform )

if( KICAD_HOUSE_THEME )
    target_compile_definitions( qa_kiplatform PRIVATE KICAD_HOUSE_THEME )
endif()

# The bundled house fonts, read from the source tree
if( KICAD_HOUSE_FONTS )
    target_compile_definitions( qa_kiplatform PRIVATE
        "QA_HOUSE_FONTS_DIR=\"${CMAKE_SOURCE_DIR}/resources/fonts/house\"" )
endif()

kicad_add_boost_test( qa_kiplatform qa_kiplatform )

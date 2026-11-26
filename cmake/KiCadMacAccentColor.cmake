#
#  This program source code file is part of KICAD, a free EDA CAD application.
#
#  Copyright The KiCad Developers, see AUTHORS.txt for contributors.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

# Reckless build (KICAD_HOUSE_THEME): the house accent color, #e0a038, in each macOS program bundle.
#
# Xcode's actool compiles resources/macos/house_theme/Assets.xcassets into an Assets.car holding the color asset
# AccentColor, which goes into every bundle's Resources, and NSAccentColorName in the bundle's Info.plist names it.
# AppKit then gives that color as controlAccentColor and wxWidgets as wxSYS_COLOUR_HIGHLIGHT. macOS uses a program's
# own accent only while the user's accent is Multicolor (System Settings, Appearance); any other choice of theirs wins.
#
# Without KICAD_HOUSE_THEME nothing here runs: KICAD_MACOS_ACCENT_COLOR_ENTRY is empty, so the Info.plist templates
# (resources/macos/plist) come out without the key, and no bundle gets an Assets.car.
#
# Included from resources/CMakeLists.txt before the Info.plist files are configured.

set( KICAD_MACOS_ACCENT_COLOR_ENTRY "" )

if( NOT KICAD_HOUSE_THEME )
    return()
endif()

execute_process( COMMAND xcrun --find actool
                 OUTPUT_VARIABLE _KICAD_ACTOOL
                 OUTPUT_STRIP_TRAILING_WHITESPACE
                 RESULT_VARIABLE _KICAD_ACTOOL_RESULT
                 ERROR_QUIET )

if( NOT _KICAD_ACTOOL_RESULT EQUAL 0 OR NOT _KICAD_ACTOOL )
    message( FATAL_ERROR "KICAD_HOUSE_THEME needs Xcode's actool for the macOS accent color, and xcrun finds none "
                         "(the Command Line Tools alone do not carry it): install Xcode, or turn KICAD_HOUSE_THEME off" )
endif()

set( KICAD_MACOS_ACCENT_COLOR_ENTRY "<key>NSAccentColorName</key>                <string>AccentColor</string>" )

set( _KICAD_ACCENT_ASSETS ${PROJECT_SOURCE_DIR}/resources/macos/house_theme/Assets.xcassets )
set( _KICAD_ACCENT_OUTPUT_DIR ${PROJECT_BINARY_DIR}/resources/macos/house_theme )
set( _KICAD_ACCENT_CAR ${_KICAD_ACCENT_OUTPUT_DIR}/Assets.car )

if( CMAKE_OSX_DEPLOYMENT_TARGET )
    set( _KICAD_ACCENT_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET} )
else()
    set( _KICAD_ACCENT_DEPLOYMENT_TARGET 11.0 )
endif()

add_custom_command(
    OUTPUT ${_KICAD_ACCENT_CAR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_KICAD_ACCENT_OUTPUT_DIR}
    COMMAND ${_KICAD_ACTOOL} --compile ${_KICAD_ACCENT_OUTPUT_DIR}
            --platform macosx
            --minimum-deployment-target ${_KICAD_ACCENT_DEPLOYMENT_TARGET}
            --accent-color AccentColor
            --output-partial-info-plist ${_KICAD_ACCENT_OUTPUT_DIR}/partial.plist
            --errors --warnings --output-format human-readable-text
            ${_KICAD_ACCENT_ASSETS}
    DEPENDS ${_KICAD_ACCENT_ASSETS}/Contents.json
            ${_KICAD_ACCENT_ASSETS}/AccentColor.colorset/Contents.json
    COMMENT "Compiling the macOS accent color asset"
    VERBATIM
    )

add_custom_target( kicad_macos_accent_color DEPENDS ${_KICAD_ACCENT_CAR} )

# The bundles are made in directories processed after this one, so they are given the asset once the top level is done.
# That call runs in the top directory's scope, which does not see this one's variables: the path is made again here.
function( _kicad_add_macos_accent_color )
    set( _car ${PROJECT_BINARY_DIR}/resources/macos/house_theme/Assets.car )

    foreach( _bundle kicad eeschema pcbnew gerbview pl_editor pcb_calculator )
        if( NOT TARGET ${_bundle} )
            continue()
        endif()

        target_sources( ${_bundle} PRIVATE ${_car} )
        set_source_files_properties( ${_car} TARGET_DIRECTORY ${_bundle}
                                     PROPERTIES GENERATED TRUE MACOSX_PACKAGE_LOCATION Resources )
        add_dependencies( ${_bundle} kicad_macos_accent_color )
    endforeach()
endfunction()

cmake_language( DEFER DIRECTORY ${PROJECT_SOURCE_DIR} CALL _kicad_add_macos_accent_color )

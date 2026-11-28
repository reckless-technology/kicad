/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file test_project_local_settings.cpp
 *
 * Guards issue #24193 where opening a V8 project in V10 left footprint
 * reference and value text invisible because the PRL schema 3->5 migration
 * mistranslated the legacy numeric layer ids.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <interaction_profile.h>
#include <project.h>
#include <project/project_local_settings.h>
#include <settings/json_settings_internals.h>
#include <settings/layer_settings_utils.h>
#include <settings/common_settings.h>
#include <settings/settings_manager.h>
#include <wildcards_and_files_ext.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;


class PROJECT_LOCAL_SETTINGS_FIXTURE
{
public:
    PROJECT_LOCAL_SETTINGS_FIXTURE()
    {
        m_originalDir = fs::current_path();
        m_tempDir = fs::temp_directory_path() / "kicad_project_local_settings_test";
        fs::remove_all( m_tempDir );
        fs::create_directories( m_tempDir );
    }

    ~PROJECT_LOCAL_SETTINGS_FIXTURE()
    {
        fs::current_path( m_originalDir );
        fs::remove_all( m_tempDir );
    }

    void writeAndLoadProject( const std::string& aProjectName, const std::string& aPrlContents,
                              SETTINGS_MANAGER& aSettingsManager )
    {
        fs::path projectDir = m_tempDir / aProjectName;
        fs::create_directories( projectDir );

        fs::path prlPath = projectDir
                           / ( aProjectName + "." + FILEEXT::ProjectLocalSettingsFileExtension );

        {
            std::ofstream prlFile( prlPath );
            prlFile << aPrlContents;
        }

        std::string proContents = R"({ "meta": { "filename": ")" + aProjectName + R"(.kicad_pro", "version": 3 } })";

        fs::path proPath = projectDir / ( aProjectName + ".kicad_pro" );

        {
            std::ofstream proFile( proPath );
            proFile << proContents;
        }

        BOOST_REQUIRE( aSettingsManager.LoadProject( wxString( proPath.string() ) ) );
    }

    fs::path m_originalDir;
    fs::path m_tempDir;
};


BOOST_FIXTURE_TEST_SUITE( ProjectLocalSettings, PROJECT_LOCAL_SETTINGS_FIXTURE )


/**
 * https://gitlab.com/kicad/code/kicad/-/issues/24193
 */
BOOST_AUTO_TEST_CASE( V8PrlFootprintTextStaysVisible )
{
    // Verbatim v8 export from the issue #24193 reproduction case.
    std::string prlContent = R"({
        "board": {
            "active_layer": 0,
            "active_layer_preset": "All Layers",
            "auto_track_width": true,
            "hidden_netclasses": [],
            "hidden_nets": [],
            "high_contrast_mode": 0,
            "net_color_mode": 1,
            "opacity": {
                "images": 0.6,
                "pads": 1.0,
                "tracks": 1.0,
                "vias": 1.0,
                "zones": 0.6
            },
            "selection_filter": {
                "dimensions": true,
                "footprints": true,
                "graphics": true,
                "keepouts": true,
                "lockedItems": false,
                "otherItems": true,
                "pads": true,
                "text": true,
                "tracks": true,
                "vias": true,
                "zones": true
            },
            "visible_items": [
                0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19,
                20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 32, 33, 34, 35,
                36, 39, 40
            ],
            "visible_layers": "fffffff_ffffffff",
            "zone_display_mode": 0
        },
        "meta": {
            "filename": "v8_project.kicad_prl",
            "version": 3
        },
        "project": {
            "files": []
        }
    })";

    SETTINGS_MANAGER settingsManager;
    writeAndLoadProject( "v8_project", prlContent, settingsManager );

    PROJECT_LOCAL_SETTINGS& localSettings = settingsManager.Prj().GetLocalSettings();

    BOOST_CHECK_MESSAGE(
            localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_FP_REFERENCES ) ),
            "LAYER_FP_REFERENCES should be visible after migrating a V8 PRL" );

    BOOST_CHECK_MESSAGE(
            localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_FP_VALUES ) ),
            "LAYER_FP_VALUES should be visible after migrating a V8 PRL" );

    BOOST_CHECK_MESSAGE(
            localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_FP_TEXT ) ),
            "LAYER_FP_TEXT should be visible after migrating a V8 PRL" );
}


/**
 * Offset 5 maps to LAYER_FP_TEXT through the frozen table, not the live enum
 * where commit 43e327b6 shifted it to 6.  The negative assertions prove the
 * migration consumed the numeric list rather than falling back to defaults.
 */
BOOST_AUTO_TEST_CASE( MinimalVisibleItemsAppliesMigration )
{
    // Offsets 5 (FP_TEXT), 17 (FP_VALUES), 18 (FP_REFERENCES) in the v8 enum.
    std::string prlContent = R"({
        "board": {
            "visible_items": [5, 17, 18],
            "visible_layers": "fffffff_ffffffff"
        },
        "meta": {
            "filename": "minimal_project.kicad_prl",
            "version": 3
        },
        "project": { "files": [] }
    })";

    SETTINGS_MANAGER settingsManager;
    writeAndLoadProject( "minimal_project", prlContent, settingsManager );

    PROJECT_LOCAL_SETTINGS& localSettings = settingsManager.Prj().GetLocalSettings();

    BOOST_CHECK( localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_FP_TEXT ) ) );
    BOOST_CHECK( localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_FP_VALUES ) ) );
    BOOST_CHECK( localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_FP_REFERENCES ) ) );

    BOOST_CHECK( !localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_TRACKS ) ) );
    BOOST_CHECK( !localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_ZONES ) ) );
    BOOST_CHECK( !localSettings.m_VisibleItems.test( GAL_LAYER_INDEX( LAYER_PADS ) ) );
}


/**
 * https://gitlab.com/kicad/code/kicad/-/issues/18812
 */
BOOST_AUTO_TEST_CASE( FirstPrlWriterKeepsUserOpacityDefaults )
{
    // As the schematic editor writes it before the board is ever opened
    std::string prlContent = R"({
        "meta": { "filename": "new_project.kicad_prl", "version": 7 },
        "project": { "files": [] }
    })";

    {
        SETTINGS_MANAGER settingsManager;
        BOARD_OPACITY_DEFAULTS& userDefaults = settingsManager.GetCommonSettings()->m_BoardOpacityDefaults;

        userDefaults.m_TrackOpacity = 0.25;
        userDefaults.m_OutsideGroupOpacity = 0.5;
        writeAndLoadProject( "new_project", prlContent, settingsManager );

        PROJECT_LOCAL_SETTINGS& localSettings = settingsManager.Prj().GetLocalSettings();

        BOOST_CHECK_EQUAL( localSettings.m_TrackOpacity, 0.25 );
        BOOST_CHECK_EQUAL( localSettings.m_OutsideGroupOpacity, 0.5 );

        localSettings.SaveToFile( settingsManager.Prj().GetProjectPath(), true );
    }

    SETTINGS_MANAGER settingsManager;
    settingsManager.GetCommonSettings()->m_BoardOpacityDefaults = BOARD_OPACITY_DEFAULTS();

    fs::path proPath = m_tempDir / "new_project" / "new_project.kicad_pro";
    BOOST_REQUIRE( settingsManager.LoadProject( wxString( proPath.string() ) ) );

    BOOST_CHECK_EQUAL( settingsManager.Prj().GetLocalSettings().m_TrackOpacity, 0.25 );
}


/**
 * A project with no stored opacities opens with the Altium-style profile's transparent 2D look (design note, 5.7), and
 * one that stores them keeps them.
 */
BOOST_AUTO_TEST_CASE( ANewProjectOpensTransparentUnderTheAltiumStyleProfile )
{
    // As the schematic editor writes it before the board is ever opened
    const std::string newPrl = R"({
        "meta": { "filename": "new_project.kicad_prl", "version": 7 },
        "project": { "files": [] }
    })";

    SETTINGS_MANAGER settingsManager;
    settingsManager.GetCommonSettings()->m_Input.interaction_profile = INTERACTION_PROFILE_ID::ALTIUM;
    settingsManager.GetCommonSettings()->m_BoardOpacityProfileLook = -1;
    writeAndLoadProject( "new_project", newPrl, settingsManager );

    PROJECT_LOCAL_SETTINGS& settings = settingsManager.Prj().GetLocalSettings();

    BOOST_CHECK_EQUAL( settings.m_PadOpacity, 0.80 );
    BOOST_CHECK_EQUAL( settings.m_ViaOpacity, 1.00 );
    BOOST_CHECK_EQUAL( settings.m_TrackOpacity, 0.85 );
    BOOST_CHECK_EQUAL( settings.m_ZoneOpacity, 0.30 );
    BOOST_CHECK_EQUAL( settings.m_ShapeOpacity, 0.60 );
}


BOOST_AUTO_TEST_CASE( ANewProjectKeepsTheStockOpacitiesInTheStockProfile )
{
    const std::string newPrl = R"({
        "meta": { "filename": "stock_project.kicad_prl", "version": 7 },
        "project": { "files": [] }
    })";

    SETTINGS_MANAGER settingsManager;
    settingsManager.GetCommonSettings()->m_Input.interaction_profile = INTERACTION_PROFILE_ID::STOCK;
    settingsManager.GetCommonSettings()->m_BoardOpacityProfileLook = -1;
    writeAndLoadProject( "stock_project", newPrl, settingsManager );

    PROJECT_LOCAL_SETTINGS&      settings = settingsManager.Prj().GetLocalSettings();
    const BOARD_OPACITY_DEFAULTS stock;

    BOOST_CHECK_EQUAL( settings.m_PadOpacity, stock.m_PadOpacity );
    BOOST_CHECK_EQUAL( settings.m_ViaOpacity, stock.m_ViaOpacity );
    BOOST_CHECK_EQUAL( settings.m_TrackOpacity, stock.m_TrackOpacity );
    BOOST_CHECK_EQUAL( settings.m_ZoneOpacity, stock.m_ZoneOpacity );
    BOOST_CHECK_EQUAL( settings.m_ShapeOpacity, stock.m_ShapeOpacity );
}


BOOST_AUTO_TEST_CASE( AProjectThatStoresItsOpacitiesKeepsThemUnderTheAltiumStyleProfile )
{
    const std::string storedPrl = R"({
        "board": {
            "opacity": {
                "images": 0.6,
                "pads": 1.0,
                "shapes": 1.0,
                "tracks": 1.0,
                "vias": 0.5,
                "zones": 0.6
            }
        },
        "meta": { "filename": "stored_project.kicad_prl", "version": 7 },
        "project": { "files": [] }
    })";

    SETTINGS_MANAGER settingsManager;
    settingsManager.GetCommonSettings()->m_Input.interaction_profile = INTERACTION_PROFILE_ID::ALTIUM;
    settingsManager.GetCommonSettings()->m_BoardOpacityProfileLook = -1;
    writeAndLoadProject( "stored_project", storedPrl, settingsManager );

    PROJECT_LOCAL_SETTINGS& settings = settingsManager.Prj().GetLocalSettings();

    BOOST_CHECK_EQUAL( settings.m_PadOpacity, 1.0 );
    BOOST_CHECK_EQUAL( settings.m_ViaOpacity, 0.5 );
    BOOST_CHECK_EQUAL( settings.m_TrackOpacity, 1.0 );
    BOOST_CHECK_EQUAL( settings.m_ZoneOpacity, 0.6 );
    BOOST_CHECK_EQUAL( settings.m_ShapeOpacity, 1.0 );
}


BOOST_AUTO_TEST_CASE( TheProfileLookCanBeTurnedOffAndThenTheUsersOwnDefaultsApply )
{
    const std::string newPrl = R"({
        "meta": { "filename": "off_project.kicad_prl", "version": 7 },
        "project": { "files": [] }
    })";

    SETTINGS_MANAGER settingsManager;
    COMMON_SETTINGS* common = settingsManager.GetCommonSettings();

    common->m_Input.interaction_profile = INTERACTION_PROFILE_ID::ALTIUM;
    common->m_BoardOpacityProfileLook = 0;
    common->m_BoardOpacityDefaults.m_PadOpacity = 0.4;
    writeAndLoadProject( "off_project", newPrl, settingsManager );

    BOOST_CHECK_EQUAL( settingsManager.Prj().GetLocalSettings().m_PadOpacity, 0.4 );
}


/**
 * "Use starting track width" follows the interaction profile unless it was chosen (note 5.8, step 5): an int that is -1
 * until then, beside the older switch the stock profile reads, so that a project saved before the setting existed
 * keeps what its owner chose in the stock profile.
 */
BOOST_AUTO_TEST_CASE( UseStartingTrackWidthFollowsTheProfileUntilItIsChosen )
{
    // A project that switched it off, from before the setting
    std::string prlContent = R"({
        "board": { "auto_track_width": false },
        "meta": { "filename": "old_project.kicad_prl", "version": 3 }
    })";

    SETTINGS_MANAGER settingsManager;
    writeAndLoadProject( "old_project", prlContent, settingsManager );

    PROJECT_LOCAL_SETTINGS& localSettings = settingsManager.Prj().GetLocalSettings();

    BOOST_CHECK_EQUAL( localSettings.m_AutoTrackWidthMode, -1 );
    BOOST_CHECK( !localSettings.m_AutoTrackWidth );

    // The stock profile keeps the switch; the Altium-style profile starts with it on
    BOOST_CHECK( !INTERACTION_PROFILE::IsAutoTrackWidthOn(
            localSettings.m_AutoTrackWidthMode, localSettings.m_AutoTrackWidth, INTERACTION_PROFILE_ID::STOCK ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsAutoTrackWidthOn(
            localSettings.m_AutoTrackWidthMode, localSettings.m_AutoTrackWidth, INTERACTION_PROFILE_ID::ALTIUM ) );

    // A project with no such setting at all starts on, as it always has
    SETTINGS_MANAGER freshManager;
    writeAndLoadProject( "new_project", R"({ "meta": { "filename": "new_project.kicad_prl", "version": 3 } })",
                         freshManager );

    BOOST_CHECK_EQUAL( freshManager.Prj().GetLocalSettings().m_AutoTrackWidthMode, -1 );
    BOOST_CHECK( freshManager.Prj().GetLocalSettings().m_AutoTrackWidth );

    // It is stored under its own key
    localSettings.m_AutoTrackWidthMode = 0;
    localSettings.Store();

    BOOST_CHECK_EQUAL( localSettings.Internals()->Get<int>( "board.auto_track_width_mode" ), 0 );
}


BOOST_AUTO_TEST_SUITE_END()

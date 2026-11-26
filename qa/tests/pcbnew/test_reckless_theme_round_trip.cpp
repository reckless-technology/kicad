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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file test_reckless_theme_round_trip.cpp
 * A board saved with the Reckless color theme as the PCB editor's theme has the content it was loaded with, and the
 * content a save under KiCad Default gives. The comparison is item by item, by UUID, rather than byte by byte,
 * because a save can write the same track segments in another order.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <board.h>
#include <footprint.h>
#include <pcb_group.h>
#include <pcb_track.h>
#include <pcbnew_settings.h>
#include <pcbnew_utils/board_file_utils.h>
#include <pgm_base.h>
#include <settings/color_settings.h>
#include <settings/settings_manager.h>
#include <zone.h>

#include <filesystem>
#include <vector>


namespace
{

std::vector<BOARD_ITEM*> boardItems( const BOARD& aBoard )
{
    std::vector<BOARD_ITEM*> items;

    for( PCB_TRACK* track : aBoard.Tracks() )
        items.push_back( track );

    for( FOOTPRINT* footprint : aBoard.Footprints() )
        items.push_back( footprint );

    for( BOARD_ITEM* drawing : aBoard.Drawings() )
        items.push_back( drawing );

    for( ZONE* zone : aBoard.Zones() )
        items.push_back( zone );

    for( PCB_GROUP* group : aBoard.Groups() )
        items.push_back( group );

    return items;
}


/// Check that two boards hold the same items with the same content, whatever order a file listed them in.
void checkSameContent( const BOARD& aExpected, const BOARD& aActual )
{
    std::vector<BOARD_ITEM*> expected = boardItems( aExpected );

    BOOST_CHECK_EQUAL( expected.size(), boardItems( aActual ).size() );
    BOOST_CHECK_EQUAL( aExpected.GetNetCount(), aActual.GetNetCount() );
    BOOST_CHECK( aExpected.GetEnabledLayers() == aActual.GetEnabledLayers() );

    int missing = 0;
    int different = 0;

    for( BOARD_ITEM* item : expected )
    {
        BOARD_ITEM* other = aActual.ResolveItem( item->m_Uuid, true );

        if( !other )
        {
            BOOST_TEST_MESSAGE( "Missing " << item->m_Uuid.AsStdString() );
            missing++;
        }
        else if( !( *item == *other ) )
        {
            BOOST_TEST_MESSAGE( "Changed " << item->m_Uuid.AsStdString() );
            different++;
        }
    }

    BOOST_CHECK_EQUAL( missing, 0 );
    BOOST_CHECK_EQUAL( different, 0 );
}


std::unique_ptr<BOARD> saveAndReload( BOARD& aBoard, const wxString& aTheme, const std::filesystem::path& aPath )
{
    PCBNEW_SETTINGS* cfg = Pgm().GetSettingsManager().GetAppSettings<PCBNEW_SETTINGS>( "pcbnew" );
    BOOST_REQUIRE( cfg );

    const wxString previous = cfg->m_ColorTheme;
    cfg->m_ColorTheme = aTheme;

    // The theme is the one the editor would draw in
    BOOST_REQUIRE( Pgm().GetSettingsManager().GetColorSettings( cfg->m_ColorTheme )->GetFilename() == aTheme );

    KI_TEST::DumpBoardToFile( aBoard, aPath );
    cfg->m_ColorTheme = previous;

    return KI_TEST::ReadBoardFromFile( aPath );
}

} // namespace


BOOST_AUTO_TEST_SUITE( RecklessThemeRoundTrip )


BOOST_AUTO_TEST_CASE( BoardContentIsUnchanged )
{
    // Tracks and vias on both copper layers, zones, footprints, text and graphics
    std::unique_ptr<BOARD> original = KI_TEST::RequireBoardFromPcbnewQAData( "api_kitchen_sink.kicad_pcb" );
    BOOST_REQUIRE( original );
    BOOST_REQUIRE( !original->Tracks().empty() );
    BOOST_REQUIRE( !original->Footprints().empty() );

    std::filesystem::path dir = std::filesystem::temp_directory_path() / "reckless_theme_round_trip";
    std::filesystem::create_directories( dir );

    std::unique_ptr<BOARD> underReckless =
            saveAndReload( *original, COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS, dir / "reckless.kicad_pcb" );
    std::unique_ptr<BOARD> underDefault =
            saveAndReload( *original, COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT, dir / "default.kicad_pcb" );
    BOOST_REQUIRE( underReckless && underDefault );

    checkSameContent( *original, *underReckless );
    checkSameContent( *underDefault, *underReckless );

    // A second save of the saved board, which is where the order of track segments can change
    std::unique_ptr<BOARD> again =
            saveAndReload( *underReckless, COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS, dir / "again.kicad_pcb" );
    BOOST_REQUIRE( again );
    checkSameContent( *underReckless, *again );

    std::filesystem::remove_all( dir );
}


BOOST_AUTO_TEST_SUITE_END()

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
 * A schematic saved with the Reckless color theme as the schematic editor's theme has the content it was loaded
 * with, and the content a save under KiCad Default gives, compared item by item by UUID.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <schematic_utils/schematic_file_util.h>

#include <eeschema_settings.h>
#include <pgm_base.h>
#include <richio.h>
#include <sch_io/kicad_sexpr/sch_io_kicad_sexpr.h>
#include <sch_screen.h>
#include <sch_sheet.h>
#include <sch_sheet_path.h>
#include <schematic.h>
#include <settings/color_settings.h>
#include <settings/settings_manager.h>

#include <filesystem>
#include <map>


namespace
{

std::map<KIID, SCH_ITEM*> screenItems( SCH_SCREEN& aScreen )
{
    std::map<KIID, SCH_ITEM*> items;

    for( SCH_ITEM* item : aScreen.Items() )
        items[item->m_Uuid] = item;

    return items;
}


/// Check that two root screens hold the same items with the same content, whatever order a file listed them in.
void checkSameContent( SCH_SCREEN& aExpected, SCH_SCREEN& aActual )
{
    std::map<KIID, SCH_ITEM*> expected = screenItems( aExpected );
    std::map<KIID, SCH_ITEM*> actual = screenItems( aActual );

    BOOST_CHECK_EQUAL( expected.size(), actual.size() );

    int missing = 0;
    int different = 0;

    for( const auto& [uuid, item] : expected )
    {
        auto other = actual.find( uuid );

        if( other == actual.end() )
        {
            BOOST_TEST_MESSAGE( "Missing " << uuid.AsStdString() );
            missing++;
        }
        else if( !( *item == *other->second ) )
        {
            BOOST_TEST_MESSAGE( "Changed " << uuid.AsStdString() );
            different++;
        }
    }

    BOOST_CHECK_EQUAL( missing, 0 );
    BOOST_CHECK_EQUAL( different, 0 );
}


struct RECKLESS_THEME_ROUND_TRIP_FIXTURE
{
    RECKLESS_THEME_ROUND_TRIP_FIXTURE() :
            m_dir( std::filesystem::temp_directory_path() / "reckless_theme_sch_round_trip" )
    {
        std::filesystem::create_directories( m_dir );
    }

    ~RECKLESS_THEME_ROUND_TRIP_FIXTURE() { std::filesystem::remove_all( m_dir ); }

    std::unique_ptr<SCHEMATIC> saveAndReload( SCHEMATIC& aSchematic, const wxString& aTheme, const std::string& aName )
    {
        EESCHEMA_SETTINGS* cfg = Pgm().GetSettingsManager().GetAppSettings<EESCHEMA_SETTINGS>( "eeschema" );
        BOOST_REQUIRE( cfg );

        const wxString previous = cfg->m_ColorTheme;
        cfg->m_ColorTheme = aTheme;

        // The theme is the one the editor would draw in
        BOOST_REQUIRE( Pgm().GetSettingsManager().GetColorSettings( cfg->m_ColorTheme )->GetFilename() == aTheme );

        std::string path = ( m_dir / aName ).string();
        SCH_SHEET*  sheet = aSchematic.GetTopLevelSheet( 0 );
        BOOST_REQUIRE( sheet );
        KI_TEST::DumpSchematicToFile( aSchematic, *sheet, path );
        cfg->m_ColorTheme = previous;

        // Reloaded the way KI_TEST::LoadSchematic loads the original, so both sides are linked alike
        std::unique_ptr<SCHEMATIC> reloaded = std::make_unique<SCHEMATIC>( &m_settingsManager.Prj() );
        reloaded->Reset();
        SCH_SHEET* emptySheet = reloaded->GetTopLevelSheet( 0 );

        SCH_IO_KICAD_SEXPR io;
        SCH_SHEET*         loadedSheet = io.LoadSchematicFile( path, reloaded.get() );
        BOOST_REQUIRE( loadedSheet );
        reloaded->AddTopLevelSheet( loadedSheet );
        reloaded->RemoveTopLevelSheet( emptySheet );
        delete emptySheet;

        SCH_SCREENS screens( reloaded->Root() );

        for( SCH_SCREEN* screen = screens.GetFirst(); screen; screen = screens.GetNext() )
            screen->UpdateLocalLibSymbolLinks();

        SCH_SHEET_LIST sheets = reloaded->BuildSheetListSortedByPageNumbers();
        sheets.UpdateSymbolInstanceData( reloaded->RootScreen()->GetSymbolInstances() );
        sheets.UpdateSheetInstanceData( reloaded->RootScreen()->GetSheetInstances() );

        return reloaded;
    }

    std::filesystem::path      m_dir;
    SETTINGS_MANAGER           m_settingsManager;
    std::unique_ptr<SCHEMATIC> m_schematic;
};

} // namespace


BOOST_FIXTURE_TEST_SUITE( RecklessThemeSchematicRoundTrip, RECKLESS_THEME_ROUND_TRIP_FIXTURE )


BOOST_AUTO_TEST_CASE( SchematicContentIsUnchanged )
{
    // Symbols, wires, buses, junctions, labels, text, graphics and a table on one sheet
    KI_TEST::LoadSchematic( m_settingsManager, wxT( "api_kitchen_sink" ), m_schematic );
    BOOST_REQUIRE( m_schematic && m_schematic->RootScreen() );
    BOOST_REQUIRE( m_schematic->RootScreen()->Items().size() > 0 );

    std::unique_ptr<SCHEMATIC> underReckless =
            saveAndReload( *m_schematic, COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS, "reckless.kicad_sch" );
    std::unique_ptr<SCHEMATIC> underDefault =
            saveAndReload( *m_schematic, COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT, "default.kicad_sch" );
    BOOST_REQUIRE( underReckless && underReckless->RootScreen() );
    BOOST_REQUIRE( underDefault && underDefault->RootScreen() );

    checkSameContent( *m_schematic->RootScreen(), *underReckless->RootScreen() );
    checkSameContent( *underDefault->RootScreen(), *underReckless->RootScreen() );
}


BOOST_AUTO_TEST_SUITE_END()

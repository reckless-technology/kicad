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
 * @file test_reckless_color_theme.cpp
 * The built-in Reckless color theme: it is registered and read-only, it carries every color of its file and a
 * color for every layer KiCad Default colors, a reload or reset leaves it alone, and it is the default theme of a
 * new configuration exactly when the build has KICAD_HOUSE_THEME.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <layer_ids.h>
#include <locale_io.h>
#include <settings/app_settings.h>
#include <settings/color_settings.h>
#include <settings/json_settings_internals.h>

#include <fstream>
#include <memory>
#include <sstream>
#include <vector>


namespace
{

/// Owns the built-in themes COLOR_SETTINGS::CreateBuiltinColorSettings() makes, and finds one by its key.
struct BUILTIN_THEMES
{
    BUILTIN_THEMES()
    {
        for( COLOR_SETTINGS* theme : COLOR_SETTINGS::CreateBuiltinColorSettings() )
            themes.emplace_back( theme );
    }

    COLOR_SETTINGS* Find( const wxString& aFilename ) const
    {
        for( const std::unique_ptr<COLOR_SETTINGS>& theme : themes )
        {
            if( theme->GetFilename() == aFilename )
                return theme.get();
        }

        return nullptr;
    }

    std::vector<std::unique_ptr<COLOR_SETTINGS>> themes;
};


std::string readThemeFile()
{
    std::ifstream     stream( std::string( QA_SRC_ROOT ) + "/common/settings/reckless_color_theme.json" );
    std::stringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}


/// The number of layers on which two themes give different colors.
int countDifferences( const COLOR_SETTINGS& aFirst, const COLOR_SETTINGS& aSecond )
{
    int differences = 0;

    for( int layer = 0; layer < LAYER_ID_COUNT; ++layer )
    {
        if( aFirst.GetColor( layer ) != aSecond.GetColor( layer ) )
        {
            BOOST_TEST_MESSAGE( "Layer " << layer << " differs" );
            differences++;
        }
    }

    return differences;
}

} // namespace


BOOST_AUTO_TEST_SUITE( RecklessColorTheme )


BOOST_AUTO_TEST_CASE( IsBuiltInAndReadOnly )
{
    BUILTIN_THEMES  builtins;
    COLOR_SETTINGS* reckless = builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS );

    BOOST_REQUIRE( reckless );
    BOOST_CHECK_EQUAL( reckless->GetName(), wxString( wxS( "Reckless" ) ) );
    BOOST_CHECK( reckless->IsReadOnly() );

    // Upstream's themes stay beside it
    BOOST_CHECK( builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT ) );
    BOOST_CHECK( builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_CLASSIC ) );
}


BOOST_AUTO_TEST_CASE( CarriesItsFile )
{
    BUILTIN_THEMES  builtins;
    COLOR_SETTINGS* reckless = builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS );
    BOOST_REQUIRE( reckless );

    // The file read the way the built-in reads it: through a theme with every parameter
    std::string themeFile = readThemeFile();
    BOOST_REQUIRE( !themeFile.empty() );

    COLOR_SETTINGS fromFile( wxS( "reckless_from_file" ) );

    {
        LOCALE_IO toggle;
        BOOST_REQUIRE( fromFile.LoadFromBuffer( themeFile ) );
    }

    BOOST_CHECK_EQUAL( countDifferences( *reckless, fromFile ), 0 );

    // And it is not KiCad Default's colors under another name, which is what a file that failed to parse gives
    COLOR_SETTINGS* kicadDefault = builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT );
    BOOST_REQUIRE( kicadDefault );
    BOOST_CHECK_NE( reckless->GetColor( LAYER_PCB_BACKGROUND ), kicadDefault->GetColor( LAYER_PCB_BACKGROUND ) );
    BOOST_CHECK_NE( reckless->GetColor( LAYER_SCHEMATIC_BACKGROUND ),
                    kicadDefault->GetColor( LAYER_SCHEMATIC_BACKGROUND ) );

    // The look's own colors, where the theme takes them as they are
    const COLOR4D plate( wxString( wxS( "#1b1a1d" ) ) );
    const COLOR4D ink( wxString( wxS( "#e8dcc8" ) ) );
    const COLOR4D amber( wxString( wxS( "#e0a038" ) ) );

    BOOST_CHECK_EQUAL( reckless->GetColor( LAYER_PCB_BACKGROUND ), plate );
    BOOST_CHECK_EQUAL( reckless->GetColor( LAYER_SCHEMATIC_BACKGROUND ), plate );
    BOOST_CHECK_EQUAL( reckless->GetColor( LAYER_GERBVIEW_BACKGROUND ), plate );
    BOOST_CHECK_EQUAL( reckless->GetColor( F_SilkS ), ink );
    BOOST_CHECK_EQUAL( reckless->GetColor( LAYER_REFERENCEPART ), ink );
    BOOST_CHECK_EQUAL( reckless->GetColor( LAYER_BRIGHTENED ), amber );
    // The selection halo is the accent at half strength, written as CSS (an alpha of 0.5 is stored in 8 bits)
    BOOST_CHECK_EQUAL( reckless->GetColor( LAYER_SELECTION_SHADOWS ),
                       COLOR4D( wxString( wxS( "rgba(224, 160, 56, 0.500)" ) ) ) );
}


BOOST_AUTO_TEST_CASE( ColorsEveryLayerKiCadDefaultColors )
{
    BUILTIN_THEMES  builtins;
    COLOR_SETTINGS* reckless = builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS );
    COLOR_SETTINGS* kicadDefault = builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT );
    BOOST_REQUIRE( reckless && kicadDefault );

    int missing = 0;

    for( int layer = 0; layer < LAYER_ID_COUNT; ++layer )
    {
        if( kicadDefault->GetColor( layer ) != COLOR4D::UNSPECIFIED
            && reckless->GetColor( layer ) == COLOR4D::UNSPECIFIED )
        {
            BOOST_TEST_MESSAGE( "Layer " << layer << " has no color" );
            missing++;
        }
    }

    BOOST_CHECK_EQUAL( missing, 0 );
}


BOOST_AUTO_TEST_CASE( SurvivesReloadAndReset )
{
    BUILTIN_THEMES  builtins;
    COLOR_SETTINGS* reckless = builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS );
    BOOST_REQUIRE( reckless );

    COLOR_SETTINGS before( *reckless );

    // What SETTINGS_MANAGER::Load() and ResetToDefaults() do to every registered theme
    reckless->Internals()->clear();
    reckless->Load();
    reckless->ResetToDefaults();

    BOOST_CHECK_EQUAL( countDifferences( *reckless, before ), 0 );
}


BOOST_AUTO_TEST_CASE( IsTheDefaultOnlyInTheHouseBuild )
{
#ifdef KICAD_HOUSE_THEME
    const wxString expected = COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS;
    const bool     printsInATheme = true;
    const wxString printTheme = COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT;
#else
    const wxString expected = COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT;
    const bool     printsInATheme = false;
    const wxString printTheme = wxEmptyString;
#endif

    BOOST_CHECK_EQUAL( COLOR_SETTINGS::DefaultBuiltinTheme(), expected );

    // A new configuration: an application's settings with no file behind them
    APP_SETTINGS_BASE settings( "reckless_color_theme_test", 0 );
    settings.Load();

    BOOST_CHECK_EQUAL( settings.m_ColorTheme, expected );
    BOOST_CHECK_EQUAL( settings.m_Printing.use_theme, printsInATheme );
    BOOST_CHECK_EQUAL( settings.m_Printing.color_theme, printTheme );
}


BOOST_AUTO_TEST_SUITE_END()

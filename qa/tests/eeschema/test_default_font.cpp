/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.TXT for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


/**
 * @file test_default_font.cpp
 * Test the shipped default for the font of new schematic text: Noto Serif where it is installed, the KiCad
 * stroke font where it is not, and never an override of a font the user has chosen.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <font/fontconfig.h>
#include <font/kicad_font_name.h>
#include <eeschema_settings.h>
#include <pgm_base.h>
#include <sch_text.h>
#include <sch_field.h>
#include <sch_label.h>
#include <sch_textbox.h>
#include <sch_sheet.h>
#include <sch_sheet_pin.h>

#include <string>
#include <vector>


BOOST_AUTO_TEST_SUITE( DefaultFont )


BOOST_AUTO_TEST_CASE( PreferredDefaultIsNotoSerif )
{
    BOOST_CHECK_EQUAL( EESCHEMA_SETTINGS::PreferredDefaultFont(), wxString( "Noto Serif" ) );
}


BOOST_AUTO_TEST_CASE( ChooseDefaultFontUsesNotoSerifWhenInstalled )
{
    std::vector<std::string> fonts = { "DejaVu Sans", "Noto Serif", "Noto Serif CJK SC" };

    BOOST_CHECK_EQUAL( EESCHEMA_SETTINGS::ChooseDefaultFont( fonts ), wxString( "Noto Serif" ) );

    // Family names are compared without regard to case, and the canonical spelling is what is returned.
    fonts = { "noto serif" };
    BOOST_CHECK_EQUAL( EESCHEMA_SETTINGS::ChooseDefaultFont( fonts ), wxString( "Noto Serif" ) );
}


BOOST_AUTO_TEST_CASE( ChooseDefaultFontFallsBackToKiCadFont )
{
    // Not installed: neither an empty list nor one holding only other families, including other Noto Serif
    // families, which merely start with the same words.
    std::vector<std::string> fonts;
    BOOST_CHECK_EQUAL( EESCHEMA_SETTINGS::ChooseDefaultFont( fonts ), wxString( KICAD_FONT_NAME ) );

    fonts = { "DejaVu Sans", "Noto Serif CJK SC", "Noto Serif Display", "Noto Sans" };
    BOOST_CHECK_EQUAL( EESCHEMA_SETTINGS::ChooseDefaultFont( fonts ), wxString( KICAD_FONT_NAME ) );
}


BOOST_AUTO_TEST_CASE( SettingsDefaultMatchesInstalledFonts )
{
    EESCHEMA_SETTINGS settings;

    // Loading with no file on disk leaves every parameter at its default.
    settings.Load();

    BOOST_CHECK_EQUAL( settings.m_Appearance.default_font, EESCHEMA_SETTINGS::InstalledDefaultFont() );

    // The default is one of the two allowed values, whichever machine this runs on.
    BOOST_CHECK( settings.m_Appearance.default_font == wxString( "Noto Serif" )
                 || settings.m_Appearance.default_font == wxString( KICAD_FONT_NAME ) );

    std::vector<std::string> families;
    Fontconfig()->ListFonts( families, std::string( Pgm().GetLanguageTag().utf8_str() ) );

    bool isInstalled = false;

    for( const std::string& family : families )
        isInstalled |= wxString::FromUTF8( family ) == wxString( "Noto Serif" );

    if( isInstalled )
        BOOST_CHECK_EQUAL( settings.m_Appearance.default_font, wxString( "Noto Serif" ) );
    else
        BOOST_CHECK_EQUAL( settings.m_Appearance.default_font, wxString( KICAD_FONT_NAME ) );
}


BOOST_AUTO_TEST_CASE( ExplicitChoiceIsNeverOverwritten )
{
    // Files at the current schema and at the previous one, which is migrated on load. The old shipped default
    // and a chosen font are both kept as written.
    const char* const files[] = {
        R"({"meta":{"version":4},"appearance":{"default_font":"KiCad Font"}})",
        R"({"meta":{"version":3},"appearance":{"default_font":"KiCad Font"}})",
        R"({"meta":{"version":4},"appearance":{"default_font":"Arial"}})",
        R"({"meta":{"version":3},"appearance":{"default_font":"Arial"}})",
    };

    const wxString expected[] = { KICAD_FONT_NAME, KICAD_FONT_NAME, wxS( "Arial" ), wxS( "Arial" ) };

    for( size_t i = 0; i < std::size( files ); ++i )
    {
        EESCHEMA_SETTINGS settings;

        BOOST_REQUIRE( settings.LoadFromBuffer( files[i] ) );
        BOOST_CHECK_EQUAL( settings.m_Appearance.default_font, expected[i] );
    }
}


BOOST_AUTO_TEST_CASE( FileWithoutTheKeyGetsTheDefault )
{
    EESCHEMA_SETTINGS settings;

    BOOST_REQUIRE( settings.LoadFromBuffer( R"({"meta":{"version":4},"appearance":{}})" ) );
    BOOST_CHECK_EQUAL( settings.m_Appearance.default_font, EESCHEMA_SETTINGS::InstalledDefaultFont() );
}


BOOST_AUTO_TEST_CASE( NewTextItemsNameNoFontOfTheirOwn )
{
    // New text carries no font, so it follows the default when drawn instead of freezing it into the file.
    SCH_TEXT      text( VECTOR2I( 0, 0 ), wxS( "text" ) );
    SCH_TEXTBOX   textBox( LAYER_NOTES );
    SCH_FIELD     field( nullptr, FIELD_T::USER, wxS( "field" ) );
    SCH_LABEL     label( VECTOR2I( 0, 0 ), wxS( "label" ) );
    SCH_SHEET     sheet;
    SCH_SHEET_PIN sheetPin( &sheet, VECTOR2I( 0, 0 ), wxS( "pin" ) );

    BOOST_CHECK( text.GetFont() == nullptr );
    BOOST_CHECK( textBox.GetFont() == nullptr );
    BOOST_CHECK( field.GetFont() == nullptr );
    BOOST_CHECK( label.GetFont() == nullptr );
    BOOST_CHECK( sheetPin.GetFont() == nullptr );
}


BOOST_AUTO_TEST_SUITE_END()

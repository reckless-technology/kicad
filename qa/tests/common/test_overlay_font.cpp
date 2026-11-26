/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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
 * @file
 * The face of the canvas overlay text: the shipped default, the setting, and the GAL drawing in it.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <callback_gal.h>
#include <font/font.h>
#include <font/fontconfig.h>
#include <font/kicad_font_name.h>
#include <gal/gal_display_options.h>
#include <pgm_base.h>
#include <settings/common_settings.h>

#include <string>
#include <vector>


namespace
{
std::vector<std::string> installedFamilies()
{
    std::vector<std::string> families;
    Fontconfig()->ListFonts( families, std::string( Pgm().GetLanguageTag().utf8_str() ) );
    return families;
}


// An installed family that loads as an outline font, the house face first, or empty if there is none
wxString installedOutlineFamily()
{
    std::vector<std::string> families = installedFamilies();
    std::vector<wxString>    candidates = { COMMON_SETTINGS::PreferredOverlayFont() };

    for( const std::string& family : families )
        candidates.push_back( wxString::FromUTF8( family ) );

    for( const wxString& candidate : candidates )
    {
        for( const std::string& family : families )
        {
            if( wxString::FromUTF8( family ) != candidate )
                continue;

            if( KIFONT::FONT* font = KIFONT::FONT::GetFont( candidate ); font && font->IsOutline() )
                return candidate;
        }
    }

    return wxEmptyString;
}


// Counts what a GAL is asked to draw, as stroke segments and as filled outlines
struct DRAW_COUNTS
{
    int strokes = 0;
    int outlines = 0;
};


void drawOverlayText( KIGFX::GAL_DISPLAY_OPTIONS& aOptions, const wxString& aText, DRAW_COUNTS& aCounts )
{
    CALLBACK_GAL gal(
            aOptions,
            [&]( const VECTOR2I&, const VECTOR2I& )
            {
                aCounts.strokes++;
            },
            [&]( const SHAPE_LINE_CHAIN& )
            {
                aCounts.outlines++;
            } );

    gal.SetGlyphSize( VECTOR2I( 1000000, 1000000 ) );
    gal.SetLineWidth( 100000 );
    gal.SetFontBold( true );
    gal.BitmapText( aText, VECTOR2I( 0, 0 ), ANGLE_HORIZONTAL );
}
} // namespace


BOOST_AUTO_TEST_SUITE( OverlayFont )


BOOST_AUTO_TEST_CASE( PreferredOverlayFontIsMonaspaceXenon )
{
    BOOST_CHECK_EQUAL( COMMON_SETTINGS::PreferredOverlayFont(), wxString( "Monaspace Xenon" ) );
}


BOOST_AUTO_TEST_CASE( ChooseOverlayFontUsesXenonWhenInstalled )
{
    std::vector<std::string> fonts = { "DejaVu Sans Mono", "Monaspace Xenon", "Monaspace Neon" };

    BOOST_CHECK_EQUAL( COMMON_SETTINGS::ChooseOverlayFont( fonts ), wxString( "Monaspace Xenon" ) );

    // Family names are compared without regard to case, and the canonical spelling is what is returned
    fonts = { "monaspace xenon" };
    BOOST_CHECK_EQUAL( COMMON_SETTINGS::ChooseOverlayFont( fonts ), wxString( "Monaspace Xenon" ) );
}


BOOST_AUTO_TEST_CASE( ChooseOverlayFontFallsBackToKiCadFont )
{
    // Not installed: an empty list, or one holding only other families, including ones that merely share words
    std::vector<std::string> fonts;
    BOOST_CHECK_EQUAL( COMMON_SETTINGS::ChooseOverlayFont( fonts ), wxString( KICAD_FONT_NAME ) );

    fonts = { "Monaspace Neon", "Monaspace Xenon Var", "MonaspiceXe Nerd Font", "Noto Serif" };
    BOOST_CHECK_EQUAL( COMMON_SETTINGS::ChooseOverlayFont( fonts ), wxString( KICAD_FONT_NAME ) );
}


BOOST_AUTO_TEST_CASE( SettingsDefaultMatchesTheBuildAndTheMachine )
{
    COMMON_SETTINGS settings;
    settings.ResetToDefaults();

#ifdef KICAD_HOUSE_FONTS
    BOOST_CHECK_EQUAL( settings.m_Appearance.overlay_font, COMMON_SETTINGS::ChooseOverlayFont( installedFamilies() ) );
#else
    BOOST_CHECK_EQUAL( settings.m_Appearance.overlay_font, wxString( KICAD_FONT_NAME ) );
#endif
}


BOOST_AUTO_TEST_CASE( ExplicitChoiceIsKept )
{
    const wxString faces[] = { KICAD_FONT_NAME, wxS( "Monaspace Xenon" ), wxS( "DejaVu Sans Mono" ) };

    for( const wxString& face : faces )
    {
        COMMON_SETTINGS settings;
        const wxString  file = wxString::Format( R"({"meta":{"version":7},"appearance":{"overlay_font":"%s"}})", face );

        BOOST_REQUIRE( settings.LoadFromBuffer( file.ToStdString() ) );
        BOOST_CHECK_EQUAL( settings.m_Appearance.overlay_font, face );
    }
}


BOOST_AUTO_TEST_CASE( FileWithoutTheKeyGetsTheDefault )
{
    COMMON_SETTINGS defaults;
    COMMON_SETTINGS settings;
    defaults.ResetToDefaults();

    BOOST_REQUIRE( settings.LoadFromBuffer( R"({"meta":{"version":7},"appearance":{}})" ) );
    BOOST_CHECK_EQUAL( settings.m_Appearance.overlay_font, defaults.m_Appearance.overlay_font );
}


BOOST_AUTO_TEST_CASE( GalHasNoOverlayFaceForTheBuiltInFonts )
{
    KIGFX::GAL_DISPLAY_OPTIONS options;
    CALLBACK_GAL               gal( options,
                                    []( const SHAPE_LINE_CHAIN& )
                                    {
                                    } );

    BOOST_CHECK( gal.GetOverlayFont() == nullptr );

    options.m_overlayFontName = KICAD_FONT_NAME;
    options.NotifyChanged();

    BOOST_CHECK( gal.GetOverlayFont() == nullptr );
    BOOST_CHECK( gal.GetOverlayFont( true ) == nullptr );
}


BOOST_AUTO_TEST_CASE( GalResolvesAnOutlineOverlayFace )
{
    const wxString family = installedOutlineFamily();

    if( family.IsEmpty() )
    {
        BOOST_TEST_MESSAGE( "No outline font is installed; skipped" );
        return;
    }

    KIGFX::GAL_DISPLAY_OPTIONS options;
    options.m_overlayFontName = family;

    // A GAL made after the choice picks it up, and one made before follows a change
    CALLBACK_GAL laterGal( options,
                           []( const SHAPE_LINE_CHAIN& )
                           {
                           } );
    BOOST_REQUIRE( laterGal.GetOverlayFont() != nullptr );
    BOOST_CHECK( laterGal.GetOverlayFont()->IsOutline() );
    BOOST_REQUIRE( laterGal.GetOverlayFont( true ) != nullptr );
    BOOST_CHECK( laterGal.GetOverlayFont( true )->IsOutline() );

    KIGFX::GAL_DISPLAY_OPTIONS otherOptions;
    CALLBACK_GAL               earlierGal( otherOptions,
                                           []( const SHAPE_LINE_CHAIN& )
                                           {
                                           } );
    BOOST_CHECK( earlierGal.GetOverlayFont() == nullptr );

    otherOptions.m_overlayFontName = family;
    otherOptions.NotifyChanged();
    BOOST_CHECK( earlierGal.GetOverlayFont() == laterGal.GetOverlayFont() );

    // Turned off for text that is not overlay text, and back on
    earlierGal.SetOverlayFontEnabled( false );
    BOOST_CHECK( earlierGal.GetOverlayFont() == nullptr );
    BOOST_CHECK( earlierGal.GetOverlayFont( true ) == nullptr );
    earlierGal.SetOverlayFontEnabled( true );
    BOOST_CHECK( earlierGal.GetOverlayFont() != nullptr );

    // And back to the built-in fonts
    otherOptions.m_overlayFontName = KICAD_FONT_NAME;
    otherOptions.NotifyChanged();
    BOOST_CHECK( earlierGal.GetOverlayFont() == nullptr );
}


BOOST_AUTO_TEST_CASE( BitmapTextDrawsInTheOverlayFace )
{
    // The generic GAL draws the built-in case in the stroke font, as Cairo does
    KIGFX::GAL_DISPLAY_OPTIONS builtIn;
    builtIn.m_overlayFontName = KICAD_FONT_NAME;

    DRAW_COUNTS strokeCounts;
    drawOverlayText( builtIn, wxS( "GND" ), strokeCounts );
    BOOST_CHECK_GT( strokeCounts.strokes, 0 );
    BOOST_CHECK_EQUAL( strokeCounts.outlines, 0 );

    const wxString family = installedOutlineFamily();

    if( family.IsEmpty() )
    {
        BOOST_TEST_MESSAGE( "No outline font is installed; the outline half is skipped" );
        return;
    }

    KIGFX::GAL_DISPLAY_OPTIONS overlay;
    overlay.m_overlayFontName = family;

    DRAW_COUNTS outlineCounts;
    drawOverlayText( overlay, wxS( "GND" ), outlineCounts );
    BOOST_CHECK_EQUAL( outlineCounts.strokes, 0 );
    BOOST_CHECK_GE( outlineCounts.outlines, 3 );
}


BOOST_AUTO_TEST_SUITE_END()

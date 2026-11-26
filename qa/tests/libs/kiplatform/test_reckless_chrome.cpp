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
#include <boost/test/unit_test.hpp>

#include <kiplatform/app.h>
#include <kiplatform/reckless_palette.h>

#include <wx/settings.h>

namespace
{
namespace P = KIPLATFORM::RECKLESS_PALETTE;

// Only the palette cases read colors back, and a platform can have none of them: clang's -Wunused-function would stop a
// -Werror build there
[[maybe_unused]] unsigned rgbOf( const wxColour& aColour )
{
    return ( static_cast<unsigned>( aColour.Red() ) << 16 ) | ( static_cast<unsigned>( aColour.Green() ) << 8 )
           | static_cast<unsigned>( aColour.Blue() );
}

[[maybe_unused]] unsigned systemColour( wxSystemColour aIndex )
{
    return rgbOf( wxSystemSettings::GetColour( aIndex ) );
}
} // namespace


BOOST_AUTO_TEST_SUITE( RecklessChrome )

#if defined( KICAD_HOUSE_THEME ) && defined( __WXGTK__ )

BOOST_AUTO_TEST_CASE( DarkModeColorsWxFromThePalette )
{
    // Read a color first, as a program does before it starts the theme: wx caches it, and the theme has to flush that.
    const unsigned before = systemColour( wxSYS_COLOUR_WINDOW );
    BOOST_TEST_MESSAGE( "window color before: " << std::hex << before );

    KIPLATFORM::APP::EnableDarkMode( true );

    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_WINDOW ), P::PANEL );
    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_WINDOWTEXT ), P::INK );
    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_HIGHLIGHT ), P::SELECT );
    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_BTNFACE ), P::PLATE );
    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_MENUBAR ), P::WINDOW );
    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_3DSHADOW ), P::RULE );
}

BOOST_AUTO_TEST_CASE( DarkModeIsIdempotent )
{
    KIPLATFORM::APP::EnableDarkMode( true );
    KIPLATFORM::APP::EnableDarkMode( true );

    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_WINDOW ), P::PANEL );
}

#elif defined( __WXGTK__ )

BOOST_AUTO_TEST_CASE( UpstreamBuildLeavesTheDesktopThemeAlone )
{
    // Without the house theme, EnableDarkMode is the upstream stub on GTK: the system colors are the desktop's.
    const unsigned window = systemColour( wxSYS_COLOUR_WINDOW );
    const unsigned text = systemColour( wxSYS_COLOUR_WINDOWTEXT );

    KIPLATFORM::APP::EnableDarkMode( true );

    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_WINDOW ), window );
    BOOST_CHECK_EQUAL( systemColour( wxSYS_COLOUR_WINDOWTEXT ), text );
}

#endif

BOOST_AUTO_TEST_CASE( EnableDarkModeCanBeCalledTwice )
{
    // Holds on every platform and build, so the suite is never empty where no palette case applies
    KIPLATFORM::APP::EnableDarkMode( true );
    KIPLATFORM::APP::EnableDarkMode( true );

    BOOST_CHECK( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ).IsOk() );
}

BOOST_AUTO_TEST_SUITE_END()

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
 * @file
 * The house fonts the Reckless build bundles (KICAD_HOUSE_FONTS): with nothing installed, only the bundled
 * directory registered with KiCad's fontconfig, FindFont() finds both families.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <font/fontconfig.h>
#include <paths.h>

#include <wx/filename.h>

#ifdef KICAD_HOUSE_FONTS

using fontconfig::FONTCONFIG;

BOOST_AUTO_TEST_SUITE( HouseFonts )


BOOST_AUTO_TEST_CASE( BundledDirectoryHasTheFourFacesAndTheirLicenses )
{
    for( const wxString& name :
         { wxS( "NotoSerif-Regular.ttf" ), wxS( "NotoSerif-Bold.ttf" ), wxS( "MonaspaceXenon-Regular.otf" ),
           wxS( "MonaspaceXenon-Bold.otf" ), wxS( "OFL-NotoSerif.txt" ), wxS( "OFL-MonaspaceXenon.txt" ) } )
    {
        BOOST_TEST_CONTEXT( name )
        {
            BOOST_CHECK( wxFileName::FileExists( wxString::FromUTF8( QA_HOUSE_FONTS_DIR ) + wxS( "/" ) + name ) );
        }
    }
}


BOOST_AUTO_TEST_CASE( FindFontFindsBothFamiliesWithNoSystemFonts )
{
    // Fontconfig() first, so that its one-time startup registration lands in the process's own configuration
    Fontconfig();

    // A private configuration that names no system directory, so only the bundled directory can answer
    FcConfig* previous = FcConfigGetCurrent();
    FcConfigReference( previous );

    FcConfig* isolated = FcConfigCreate();
    BOOST_REQUIRE( isolated );
    BOOST_REQUIRE( FcConfigSetCurrent( isolated ) );
    FcConfigDestroy( isolated );

    // Nothing is installed in it: a family nobody bundles has no file
    {
        wxString file;
        int      index = 0;
        BOOST_CHECK( Fontconfig()->FindFont( wxS( "Noto Serif" ), file, index, false, false )
                     == FONTCONFIG::FF_RESULT::FF_ERROR );
    }

    const wxString directory = wxString::FromUTF8( QA_HOUSE_FONTS_DIR );
    BOOST_REQUIRE( FONTCONFIG::AddApplicationFontDirectory( directory ) );

    for( const wxString& family : { wxS( "Noto Serif" ), wxS( "Monaspace Xenon" ) } )
    {
        for( bool isBold : { false, true } )
        {
            BOOST_TEST_CONTEXT( family << ( isBold ? " Bold" : " Regular" ) )
            {
                wxString file;
                int      index = -1;

                BOOST_CHECK( Fontconfig()->FindFont( family, file, index, isBold, false )
                             == FONTCONFIG::FF_RESULT::FF_OK );
                BOOST_CHECK( file.StartsWith( directory ) );
                BOOST_CHECK_EQUAL( file.Lower().Contains( wxS( "bold" ) ), isBold );
            }
        }
    }

    FcConfigSetCurrent( previous );
    FcConfigDestroy( previous );
}


BOOST_AUTO_TEST_CASE( StartupRegistersTheBundledDirectory )
{
    // Through the process's own configuration, as the GUI and kicad-cli see it. Run with FONTCONFIG_FILE naming a
    // configuration with no system directories, this passes only if the startup registration found the data
    // directory; with system fonts installed it passes anyway.
    for( const wxString& family : { wxS( "Noto Serif" ), wxS( "Monaspace Xenon" ) } )
    {
        BOOST_TEST_CONTEXT( family )
        {
            wxString file;
            int      index = 0;

            BOOST_CHECK( Fontconfig()->FindFont( family, file, index, false, false ) == FONTCONFIG::FF_RESULT::FF_OK );
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()

#endif // KICAD_HOUSE_FONTS

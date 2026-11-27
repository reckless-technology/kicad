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

#include <dialog_shim.h>
#include <font/fontconfig.h>
#include <kiplatform/ui.h>
#include <paths.h>
#include <widgets/ui_common.h>

#include <wx/filename.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

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


#if defined( __WXMAC__ ) || defined( __WXMSW__ )
// wxOSX and wxMSW have no settable GUI default font, so the house family reaches the controls only through
// DIALOG_SHIM's constructor (KIPLATFORM::UI::ApplyInterfaceFont), which they inherit from as they are made. The
// dialog is made, not shown.
//
// It also crosses a module boundary: the family is set here, in the test program's own copy of kiplatform, and
// read by DIALOG_SHIM and KIUI in kicommon's. A family kept in a static of one copy reads as unset in the other.
BOOST_AUTO_TEST_CASE( DialogControlsTakeTheInterfaceFont )
{
    if( !KI_TEST::CanDoDisplayTests() )
        return;

    KIPLATFORM::UI::RegisterPrivateFonts( PATHS::GetBundledFontsPath() );
    KIPLATFORM::UI::SetInterfaceFontFamily( "Noto Serif" );
    BOOST_REQUIRE_EQUAL( KIPLATFORM::UI::GetInterfaceFontFamily(), wxString( wxS( "Noto Serif" ) ) );

    struct TEST_DIALOG : public DIALOG_SHIM
    {
        TEST_DIALOG() : DIALOG_SHIM( nullptr, wxID_ANY, wxS( "House font" ) )
        {
            m_label = new wxStaticText( this, wxID_ANY, wxS( "Label" ) );
            m_text = new wxTextCtrl( this, wxID_ANY );
        }

        wxStaticText* m_label;
        wxTextCtrl*   m_text;
    };

    // Heap-allocated and released with Destroy(): deleting a top-level window here, with no event loop, crashes
    TEST_DIALOG* dialog = new TEST_DIALOG();

    BOOST_CHECK_EQUAL( dialog->GetFont().GetFaceName(), wxString( wxS( "Noto Serif" ) ) );
    BOOST_CHECK_EQUAL( dialog->m_label->GetFont().GetFaceName(), wxString( wxS( "Noto Serif" ) ) );
    BOOST_CHECK_EQUAL( dialog->m_text->GetFont().GetFaceName(), wxString( wxS( "Noto Serif" ) ) );

    // At the size a control has without the house font
#ifdef __WXMAC__
    const int systemSize = wxFont( wxOSX_SYSTEM_FONT_NORMAL ).GetPointSize();
#else
    const int systemSize = wxSystemSettings::GetFont( wxSYS_DEFAULT_GUI_FONT ).GetPointSize();
#endif
    BOOST_CHECK_EQUAL( dialog->m_text->GetFont().GetPointSize(), systemSize );

    // The KIUI getters follow the family
    BOOST_CHECK_EQUAL( KIUI::GetControlFont( dialog->m_text ).GetFaceName(), wxString( wxS( "Noto Serif" ) ) );

    dialog->Destroy();
}
#endif


BOOST_AUTO_TEST_SUITE_END()

#endif // KICAD_HOUSE_FONTS

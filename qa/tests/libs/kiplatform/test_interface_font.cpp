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
 * The house interface font on wxMSW: the bundled faces registered as private fonts, and new windows and their
 * controls in that family at the system's size.
 */

#include <boost/test/unit_test.hpp>

#include <kiplatform/ui.h>

#include <wx/dialog.h>
#include <wx/fontenum.h>
#include <wx/frame.h>
#include <wx/settings.h>
#include <wx/stattext.h>

#if defined( KICAD_HOUSE_FONTS ) && defined( __WXMSW__ )

BOOST_AUTO_TEST_SUITE( InterfaceFont )


BOOST_AUTO_TEST_CASE( AnUnknownFamilyChangesNothing )
{
    KIPLATFORM::UI::SetInterfaceFontFamily( "No Such Family Anywhere" );
    BOOST_CHECK( KIPLATFORM::UI::GetInterfaceFontFamily().IsEmpty() );

    wxFrame*     frame = new wxFrame( nullptr, wxID_ANY, wxS( "font" ) );
    const wxFont before = frame->GetFont();
    KIPLATFORM::UI::ApplyInterfaceFont( frame );
    BOOST_CHECK( frame->GetFont() == before );
    frame->Destroy();
}


BOOST_AUTO_TEST_CASE( NewWindowsAndTheirControlsUseTheBundledFamily )
{
    // Not installed on the test machines: only the private registration can make it resolve
    KIPLATFORM::UI::RegisterPrivateFonts( wxString::FromUTF8( QA_HOUSE_FONTS_DIR ) + wxS( "/" ) );
    BOOST_REQUIRE( wxFontEnumerator::IsValidFacename( wxS( "Noto Serif" ) ) );
    BOOST_CHECK( wxFontEnumerator::IsValidFacename( wxS( "Monaspace Xenon" ) ) );

    KIPLATFORM::UI::SetInterfaceFontFamily( "Noto Serif" );
    BOOST_CHECK_EQUAL( KIPLATFORM::UI::GetInterfaceFontFamily().ToStdString(), "Noto Serif" );

    const wxFont system = wxSystemSettings::GetFont( wxSYS_DEFAULT_GUI_FONT );

    // Windows that are never shown, given the font as EDA_BASE_FRAME and DIALOG_SHIM do, before their controls
    wxFrame* frame = new wxFrame( nullptr, wxID_ANY, wxS( "font" ) );
    KIPLATFORM::UI::ApplyInterfaceFont( frame );
    wxStaticText* label = new wxStaticText( frame, wxID_ANY, wxS( "label" ) );

    wxDialog* dialog = new wxDialog( nullptr, wxID_ANY, wxS( "dialog" ) );
    KIPLATFORM::UI::ApplyInterfaceFont( dialog );
    wxStaticText* dialogLabel = new wxStaticText( dialog, wxID_ANY, wxS( "label" ) );

    for( wxWindow* window : { static_cast<wxWindow*>( frame ), static_cast<wxWindow*>( label ),
                              static_cast<wxWindow*>( dialog ), static_cast<wxWindow*>( dialogLabel ) } )
    {
        BOOST_TEST_CONTEXT( wxString( window->GetClassInfo()->GetClassName() ).ToStdString() )
        {
            const wxFont font = window->GetFont();

            BOOST_CHECK_EQUAL( font.GetFaceName().ToStdString(), "Noto Serif" );
            BOOST_CHECK_EQUAL( font.GetFractionalPointSize(), system.GetFractionalPointSize() );
            BOOST_CHECK_EQUAL( font.GetWeight(), system.GetWeight() );
        }
    }

    dialog->Destroy();
    frame->Destroy();
}


BOOST_AUTO_TEST_SUITE_END()

#endif

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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <key_sequence_rows.h>
#include <menu_def.h>
#include <dialogs/key_sequence_popup.h>
#include <tool/actions.h>

#include <wx/colour.h>
#include <wx/font.h>

// The rows of the key-sequence hint popup (interaction-profile note, 5.3), on a small synthetic tree.

namespace
{
MENU_DEF syntheticNode()
{
    using M = MENU_DEF;

    return M::Submenu(
            wxS( "&Place" ),
            { M::Item( wxS( "&Undo" ), ACTIONS::undo ), M::Item( wxS( "Cu&t" ), ACTIONS::cut ), M::Separator(),
              M::Submenu( wxS( "&More && Less" ), { M::Item( wxS( "&Copy" ), ACTIONS::copy ) } ),
              M::Item( wxS( "&Hidden" ), ACTIONS::paste ).KeyOnly(), M::Slot( wxS( "&Language" ), "languages" ),
              M::Item( wxS( "&Redo" ), ACTIONS::redo )
                      .When(
                              []()
                              {
                                  return false;
                              } ) } );
}
} // namespace


BOOST_AUTO_TEST_SUITE( KeySequenceRows )

BOOST_AUTO_TEST_CASE( LettersAndLabelsComeFromTheMnemonics )
{
    std::vector<KEY_SEQUENCE_ROW> rows = BuildKeySequenceRows( syntheticNode() );

    BOOST_REQUIRE_EQUAL( rows.size(), 5u ); // key-only and slot skipped, the unavailable one kept

    BOOST_CHECK( rows[0].m_letter == 'U' );
    BOOST_CHECK_EQUAL( rows[0].m_label, wxS( "Undo" ) );
    BOOST_CHECK( rows[1].m_letter == 'T' );
    BOOST_CHECK_EQUAL( rows[1].m_label, wxS( "Cut" ) );
    BOOST_CHECK( rows[3].m_letter == 'M' );
    BOOST_CHECK_EQUAL( rows[3].m_label, wxS( "More & Less" ) );
}


BOOST_AUTO_TEST_CASE( SubmenusSeparatorsAndDisabledRowsAreMarked )
{
    std::vector<KEY_SEQUENCE_ROW> rows = BuildKeySequenceRows( syntheticNode() );

    BOOST_REQUIRE_EQUAL( rows.size(), 5u );

    BOOST_CHECK( !rows[0].m_isSubmenu );
    BOOST_CHECK( rows[0].m_isEnabled );
    BOOST_CHECK( rows[2].m_isSeparator );
    BOOST_CHECK( rows[2].m_letter == 0 );
    BOOST_CHECK( rows[3].m_isSubmenu );
    BOOST_CHECK( rows[3].m_isEnabled );
    BOOST_CHECK( rows[4].m_letter == 'R' );
    BOOST_CHECK( !rows[4].m_isEnabled );
}


BOOST_AUTO_TEST_CASE( KeyOnlyEntriesAndSlotsAreSkipped )
{
    for( const KEY_SEQUENCE_ROW& row : BuildKeySequenceRows( syntheticNode() ) )
    {
        BOOST_CHECK( row.m_label != wxS( "Hidden" ) );
        BOOST_CHECK( row.m_label != wxS( "Language" ) );
    }
}


BOOST_AUTO_TEST_CASE( HotkeyTextComesFromTheProviderForActionsOnly )
{
    KEY_SEQUENCE_HOTKEY_TEXT provider = []( const TOOL_ACTION& aAction ) -> wxString
    {
        return &aAction == &ACTIONS::undo ? wxString( wxS( "Ctrl+Z" ) ) : wxString();
    };

    std::vector<KEY_SEQUENCE_ROW> rows = BuildKeySequenceRows( syntheticNode(), provider );

    BOOST_REQUIRE_EQUAL( rows.size(), 5u );
    BOOST_CHECK_EQUAL( rows[0].m_hotkeyText, wxS( "Ctrl+Z" ) );
    BOOST_CHECK( rows[1].m_hotkeyText.IsEmpty() );
    BOOST_CHECK( rows[3].m_hotkeyText.IsEmpty() ); // a submenu has no hotkey

    for( const KEY_SEQUENCE_ROW& row : BuildKeySequenceRows( syntheticNode() ) )
        BOOST_CHECK( row.m_hotkeyText.IsEmpty() ); // no provider, no text
}


BOOST_AUTO_TEST_CASE( MarkersAreStrippedFromLabels )
{
    BOOST_CHECK_EQUAL( StripMnemonicMarkers( wxS( "&Save" ) ), wxS( "Save" ) );
    BOOST_CHECK_EQUAL( StripMnemonicMarkers( wxS( "Cu&t" ) ), wxS( "Cut" ) );
    BOOST_CHECK_EQUAL( StripMnemonicMarkers( wxS( "A && B" ) ), wxS( "A & B" ) );
    BOOST_CHECK_EQUAL( StripMnemonicMarkers( wxS( "&Zoom\tCtrl+Z" ) ), wxS( "Zoom" ) );
}

BOOST_AUTO_TEST_SUITE_END()


namespace
{
/// The luminance of \a aColor, 0 to 255
int luminance( const wxColour& aColor )
{
    return ( 299 * aColor.Red() + 587 * aColor.Green() + 114 * aColor.Blue() ) / 1000;
}

/// Fixed-width text: 8 pixels a character, 16 high, bold no wider
wxSize fixedExtent( const wxString& aText, bool )
{
    return wxSize( 8 * static_cast<int>( aText.length() ), 16 );
}
} // namespace


BOOST_AUTO_TEST_SUITE( KeySequencePopup )

BOOST_AUTO_TEST_CASE( TheLayoutHitsItsRowsAndSeparatorsAndTitleAreNoRows )
{
    KEY_SEQUENCE_POPUP_LAYOUT layout;

    layout.Measure( fixedExtent, wxFont(), wxS( "Place" ), BuildKeySequenceRows( syntheticNode() ) );

    BOOST_REQUIRE_EQUAL( layout.Rows().size(), 5u );
    BOOST_REQUIRE( layout.Size().x > 0 && layout.Size().y > 0 );

    std::vector<int> firstY( layout.Rows().size(), -1 );

    for( int y = 0; y < layout.Size().y; ++y )
    {
        int row = layout.RowAt( wxPoint( 5, y ) );

        if( row >= 0 && firstY[row] < 0 )
            firstY[row] = y;
    }

    BOOST_CHECK_EQUAL( layout.RowAt( wxPoint( 5, 0 ) ), -1 ); // the title
    BOOST_CHECK_EQUAL( firstY[2], -1 );                       // the separator
    BOOST_CHECK( firstY[0] >= 0 && firstY[1] > firstY[0] && firstY[3] > firstY[1] && firstY[4] > firstY[3] );
    BOOST_CHECK_EQUAL( layout.RowAt( wxPoint( 5, layout.Size().y - 1 ) ), -1 );

    // Wide enough for the longest label and its letter, and every row below the title
    BOOST_CHECK( layout.Size().x >= 8 * 13 + 8 );
}


BOOST_AUTO_TEST_CASE( AHoveredRowIsHighlightedByLuminanceAndADisabledOneNever )
{
    KEY_SEQUENCE_ROW enabled;
    KEY_SEQUENCE_ROW disabled;

    disabled.m_isEnabled = false;

    // A light theme and a dark one
    for( const KEY_SEQUENCE_POPUP_COLORS& colors :
         { KEY_SEQUENCE_POPUP_COLORS{ wxColour( 250, 250, 250 ), wxColour( 20, 20, 20 ), wxColour( 140, 140, 140 ) },
           KEY_SEQUENCE_POPUP_COLORS{ wxColour( 30, 30, 34 ), wxColour( 230, 230, 230 ), wxColour( 120, 120, 120 ) } } )
    {
        KEY_SEQUENCE_ROW_COLORS plain = KeySequenceRowColors( enabled, false, colors );
        KEY_SEQUENCE_ROW_COLORS hovered = KeySequenceRowColors( enabled, true, colors );
        KEY_SEQUENCE_ROW_COLORS hoveredDisabled = KeySequenceRowColors( disabled, true, colors );

        BOOST_CHECK( !plain.m_hasFill );
        BOOST_REQUIRE( hovered.m_hasFill );

        // The fill differs from the background, and the text from the fill, by luminance and not by hue
        BOOST_CHECK_GT( std::abs( luminance( hovered.m_fill ) - luminance( colors.m_background ) ), 150 );
        BOOST_CHECK_GT( std::abs( luminance( hovered.m_text ) - luminance( hovered.m_fill ) ), 150 );

        BOOST_CHECK( !hoveredDisabled.m_hasFill );
        BOOST_CHECK( hoveredDisabled.m_text == colors.m_dimmed );
    }
}

BOOST_AUTO_TEST_SUITE_END()

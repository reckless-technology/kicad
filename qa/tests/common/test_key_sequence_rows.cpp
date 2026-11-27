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
#include <tool/actions.h>

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

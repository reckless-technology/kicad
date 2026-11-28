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

#ifndef KEY_SEQUENCE_ROWS_H
#define KEY_SEQUENCE_ROWS_H

#include <functional>
#include <vector>

#include <wx/string.h>

#include <menu_def.h>

class TOOL_ACTION;


/**
 * One row of the key-sequence hint popup (interaction-profile note, 5.3): what the popup draws for one entry of a menu
 * node. It holds no wx widgets, so a test can read it without a window.
 */
struct KEY_SEQUENCE_ROW
{
    wxUniChar m_letter = 0; ///< the mnemonic, upper case; 0 for a separator or an entry without one
    wxString  m_label;      ///< the label without its `&` mnemonic marker
    wxString  m_hotkeyText; ///< the action's own hotkey, for the right-hand column; empty for none
    bool      m_isSeparator = false;
    bool      m_isSubmenu = false;
    bool      m_isEnabled = true; ///< false when the entry's availability predicate answers no
};


/// The text of an action's hotkey for the popup's right-hand column; empty for an action with none.
using KEY_SEQUENCE_HOTKEY_TEXT = std::function<wxString( const TOOL_ACTION& aAction )>;


/**
 * The rows the hint popup draws for the entries of \a aNode, in menu order.
 *
 * Entries that are key-only are skipped as the menu builders skip them. A slot has nothing a sequence can select and is
 * skipped too. An entry whose `When()` predicate answers false stays, as a disabled row, so the popup shows what the
 * node holds and which part of it cannot be used just now.
 *
 * @param aNode the pending node.
 * @param aHotkeyText gives an action's hotkey text; without it the right-hand column is empty.
 */
std::vector<KEY_SEQUENCE_ROW> BuildKeySequenceRows( const MENU_DEF&                 aNode,
                                                    const KEY_SEQUENCE_HOTKEY_TEXT& aHotkeyText = {} );

/// \a aLabel without its mnemonic markers: a single `&` is dropped, `&&` becomes `&`, and anything after a tab is cut.
wxString StripMnemonicMarkers( const wxString& aLabel );

#endif // KEY_SEQUENCE_ROWS_H

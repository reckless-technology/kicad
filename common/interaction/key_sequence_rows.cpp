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

#include <key_sequence_rows.h>

#include <tool/tool_action.h>


wxString StripMnemonicMarkers( const wxString& aLabel )
{
    wxString text = aLabel.BeforeFirst( '\t' );
    wxString stripped;

    for( size_t i = 0; i < text.length(); ++i )
    {
        if( text[i] == '&' )
        {
            if( i + 1 < text.length() && text[i + 1] == '&' )
            {
                stripped += '&';
                ++i;
            }

            continue;
        }

        stripped += text[i];
    }

    return stripped;
}


std::vector<KEY_SEQUENCE_ROW> BuildKeySequenceRows( const MENU_DEF& aNode, const KEY_SEQUENCE_HOTKEY_TEXT& aHotkeyText )
{
    std::vector<KEY_SEQUENCE_ROW> rows;

    for( const MENU_DEF& entry : aNode.m_children )
    {
        if( entry.m_isKeyOnly || entry.IsSlot() )
            continue;

        KEY_SEQUENCE_ROW row;

        if( entry.IsSeparator() )
        {
            row.m_isSeparator = true;
            rows.push_back( row );
            continue;
        }

        row.m_letter = entry.Mnemonic();
        row.m_label = StripMnemonicMarkers( entry.m_label );
        row.m_isSubmenu = entry.IsSubmenu();
        row.m_isEnabled = entry.IsAvailable();

        const TOOL_ACTION* action = entry.m_action ? entry.m_action : entry.KeyAction();

        if( action && aHotkeyText )
            row.m_hotkeyText = aHotkeyText( *action );

        rows.push_back( row );
    }

    return rows;
}

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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

#include <key_sequence.h>

#include <wx/defs.h>


const MENU_DEF* KEY_SEQUENCE::FindChild( const MENU_DEF& aMenu, wxUniChar aMnemonic )
{
    if( !aMnemonic )
        return nullptr;

    for( const MENU_DEF& child : aMenu.m_children )
    {
        if( !child.IsSeparator() && !child.IsSlot() && child.IsAvailable() && child.Mnemonic() == aMnemonic )
            return &child;
    }

    return nullptr;
}


const MENU_DEF* KEY_SEQUENCE::FindRoot( const MENU_DEF& aBar, wxUniChar aMnemonic )
{
    if( const MENU_DEF* menu = FindChild( aBar, aMnemonic ) )
        return menu;

    for( const MENU_DEF& menu : aBar.m_children )
    {
        if( !menu.IsAvailable() )
            continue;

        for( const MENU_DEF& submenu : menu.m_children )
        {
            if( submenu.IsSubmenu() && submenu.IsAvailable() && submenu.Mnemonic() == aMnemonic )
                return &submenu;
        }
    }

    return nullptr;
}


const MENU_DEF* KEY_SEQUENCE::Resolve( const MENU_DEF& aBar, const std::vector<wxUniChar>& aKeys )
{
    if( aKeys.empty() )
        return nullptr;

    const MENU_DEF* node = FindRoot( aBar, aKeys[0] );

    for( size_t i = 1; node && i < aKeys.size(); ++i )
        node = FindChild( *node, aKeys[i] );

    return node;
}


bool KEY_SEQUENCE::IsModifierOnly( int aKeyCode )
{
    // An if, not a switch: WXK_CONTROL and WXK_RAW_CONTROL are the same code except on macOS
    return aKeyCode == WXK_SHIFT || aKeyCode == WXK_ALT || aKeyCode == WXK_CONTROL || aKeyCode == WXK_RAW_CONTROL
           || aKeyCode == WXK_MENU || aKeyCode == WXK_WINDOWS_LEFT || aKeyCode == WXK_WINDOWS_RIGHT
           || aKeyCode == WXK_CAPITAL;
}


void KEY_SEQUENCE::SetTree( std::shared_ptr<const MENU_DEF> aTree )
{
    Clear();
    m_tree = std::move( aTree );
}


void KEY_SEQUENCE::Clear()
{
    m_node = nullptr;
    m_typed.clear();
    ++m_token;
}


KEY_SEQUENCE::RESULT KEY_SEQUENCE::Enter( wxUniChar aRootMnemonic )
{
    Clear();

    RESULT result;

    if( !m_tree )
        return result;

    const MENU_DEF* root = FindRoot( *m_tree, aRootMnemonic );

    if( !root || !root->IsSubmenu() )
        return result;

    m_node = root;
    m_typed.push_back( aRootMnemonic );
    result.m_outcome = RESULT::OUTCOME::CONSUMED;
    return result;
}


KEY_SEQUENCE::RESULT KEY_SEQUENCE::HandleKey( int aKeyCode, bool aHasCommandModifier )
{
    RESULT result;

    if( !m_node )
        return result;

    if( IsModifierOnly( aKeyCode ) )
    {
        result.m_outcome = RESULT::OUTCOME::CONSUMED;
        return result;
    }

    if( aKeyCode == WXK_ESCAPE )
    {
        Clear();
        result.m_outcome = RESULT::OUTCOME::CONSUMED;
        return result;
    }

    const MENU_DEF* child = nullptr;

    if( !aHasCommandModifier && aKeyCode > 0 && aKeyCode < 0x250 )
    {
        int letter = aKeyCode >= 'a' && aKeyCode <= 'z' ? aKeyCode - 'a' + 'A' : aKeyCode;

        child = FindChild( *m_node, wxUniChar( letter ) );
    }

    if( !child )
    {
        Clear(); // a mistyped sequence: the key is dispatched as if there had been none
        return result;
    }

    if( child->IsSubmenu() )
    {
        m_node = child;
        m_typed.push_back( child->Mnemonic() );
        ++m_token;
        result.m_outcome = RESULT::OUTCOME::CONSUMED;
        return result;
    }

    const TOOL_ACTION* action = child->KeyAction();

    Clear();

    if( action )
    {
        result.m_outcome = RESULT::OUTCOME::RUN_ACTION;
        result.m_action = action;
    }
    else
    {
        result.m_outcome = RESULT::OUTCOME::CONSUMED;
    }

    return result;
}


KEY_SEQUENCE::RESULT KEY_SEQUENCE::HintTimerFired( unsigned aToken )
{
    RESULT result;

    if( !m_node || aToken != m_token )
        return result;

    result.m_outcome = RESULT::OUTCOME::SHOW_NODE;
    result.m_node = m_node;

    // The menu takes over from here: it has its own mnemonics, and the keys typed so far are in it
    m_node = nullptr;
    m_typed.clear();
    return result;
}


wxString KEY_SEQUENCE::KeysTyped() const
{
    wxString text;

    for( wxUniChar key : m_typed )
        text += wxString( key ) + wxS( " " );

    return text.IsEmpty() ? text : text + wxString( wxUniChar( 0x203A ) );
}

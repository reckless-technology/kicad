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

#include <menu_def.h>

#include <algorithm>
#include <map>

#include <tool/tool_action.h>


MENU_DEF MENU_DEF::Item( const wxString& aLabel, const TOOL_ACTION& aAction )
{
    MENU_DEF def;
    def.m_label = aLabel;
    def.m_action = &aAction;
    return def;
}


MENU_DEF MENU_DEF::CheckItem( const wxString& aLabel, const TOOL_ACTION& aAction )
{
    MENU_DEF def = Item( aLabel, aAction );
    def.m_isCheck = true;
    return def;
}


MENU_DEF MENU_DEF::Item( const wxString& aLabel, const TOOL_ACTION& aAction, const SELECTION_CONDITION& aCondition )
{
    MENU_DEF def = Item( aLabel, aAction );
    def.m_show = aCondition;
    return def;
}


MENU_DEF MENU_DEF::Submenu( const wxString& aLabel, std::initializer_list<MENU_DEF> aChildren )
{
    MENU_DEF def;
    def.m_label = aLabel;
    def.m_children.assign( aChildren.begin(), aChildren.end() );
    return def;
}


MENU_DEF MENU_DEF::Submenu( const wxString& aLabel, const SELECTION_CONDITION& aCondition,
                            std::initializer_list<MENU_DEF> aChildren )
{
    MENU_DEF def = Submenu( aLabel, aChildren );
    def.m_show = aCondition;
    return def;
}


MENU_DEF MENU_DEF::Separator()
{
    MENU_DEF def;
    def.m_isSeparator = true;
    return def;
}


MENU_DEF MENU_DEF::Slot( const wxString& aLabel, const std::string& aSlot )
{
    MENU_DEF def;
    def.m_label = aLabel;
    def.m_slot = aSlot;
    return def;
}


MENU_DEF MENU_DEF::When( const std::function<bool()>& aPredicate ) const
{
    MENU_DEF def = *this;
    def.m_isAvailable = aPredicate;
    return def;
}


wxUniChar MENU_DEF::Mnemonic() const
{
    for( size_t i = 0; i + 1 < m_label.length(); ++i )
    {
        if( m_label[i] != '&' )
            continue;

        if( m_label[i + 1] == '&' )
        {
            ++i; // an escaped ampersand
            continue;
        }

        return wxUniChar( m_label.Mid( i + 1, 1 ).Upper()[0] );
    }

    return 0;
}


void MENU_DEF::Walk( const std::function<void( const MENU_DEF&, const std::vector<wxString>& )>& aVisitor ) const
{
    std::vector<wxString> path;

    std::function<void( const MENU_DEF& )> visit = [&]( const MENU_DEF& aNode )
    {
        for( const MENU_DEF& child : aNode.m_children )
        {
            aVisitor( child, path );

            if( !child.m_children.empty() )
            {
                path.push_back( child.m_label );
                visit( child );
                path.pop_back();
            }
        }
    };

    visit( *this );
}


std::vector<const TOOL_ACTION*> MENU_DEF::Actions() const
{
    std::vector<const TOOL_ACTION*> actions;

    Walk(
            [&]( const MENU_DEF& aNode, const std::vector<wxString>& )
            {
                if( aNode.m_action )
                    actions.push_back( aNode.m_action );
            } );

    return actions;
}


bool MENU_DEF::Contains( const TOOL_ACTION& aAction ) const
{
    std::vector<const TOOL_ACTION*> actions = Actions();

    return std::find( actions.begin(), actions.end(), &aAction ) != actions.end();
}


std::vector<std::string> MENU_DEF::Slots() const
{
    std::vector<std::string> slots;

    Walk(
            [&]( const MENU_DEF& aNode, const std::vector<wxString>& )
            {
                if( aNode.IsSlot() )
                    slots.push_back( aNode.m_slot );
            } );

    return slots;
}


static wxString joinPath( const std::vector<wxString>& aPath, const wxString& aLeaf )
{
    wxString text;

    for( const wxString& part : aPath )
        text += ( text.IsEmpty() ? wxString() : wxString( wxS( " > " ) ) ) + part;

    if( aLeaf.IsEmpty() )
        return text;

    return text.IsEmpty() ? aLeaf : text + wxS( " > " ) + aLeaf;
}


/// The menus of a tree: the root's children are one menu (a menu bar) and so is each submenu.
static void collectMenus( const MENU_DEF& aNode, const std::vector<wxString>& aPath,
                          const std::function<void( const MENU_DEF&, const std::vector<wxString>& )>& aMenu )
{
    aMenu( aNode, aPath );

    for( const MENU_DEF& child : aNode.m_children )
    {
        if( child.m_children.empty() )
            continue;

        std::vector<wxString> path = aPath;
        path.push_back( child.m_label );
        collectMenus( child, path, aMenu );
    }
}


std::vector<wxString> MENU_DEF::DuplicateMnemonics() const
{
    std::vector<wxString> messages;

    collectMenus( *this, std::vector<wxString>(),
                  [&]( const MENU_DEF& aMenu, const std::vector<wxString>& aPath )
                  {
                      std::map<wxUniChar, std::vector<wxString>> byMnemonic;

                      for( const MENU_DEF& entry : aMenu.m_children )
                      {
                          if( entry.IsSeparator() || !entry.Mnemonic() )
                              continue;

                          byMnemonic[entry.Mnemonic()].push_back( entry.m_label );
                      }

                      for( const auto& [mnemonic, labels] : byMnemonic )
                      {
                          if( labels.size() < 2 )
                              continue;

                          wxString names;

                          for( const wxString& label : labels )
                              names += wxString::Format( wxS( " \"%s\"" ), label );

                          messages.push_back( wxString::Format( wxS( "menu \"%s\": mnemonic %c is shared by%s" ),
                                                                aPath.empty() ? wxString( wxS( "(top level)" ) )
                                                                              : joinPath( aPath, wxEmptyString ),
                                                                mnemonic.GetValue(), names ) );
                      }
                  } );

    return messages;
}


std::vector<wxString> MENU_DEF::EntriesWithoutMnemonic() const
{
    std::vector<wxString> messages;

    Walk(
            [&]( const MENU_DEF& aNode, const std::vector<wxString>& aPath )
            {
                if( !aNode.IsSeparator() && !aNode.IsSlot() && !aNode.Mnemonic() )
                    messages.push_back( joinPath( aPath, aNode.m_label ) + wxS( " has no mnemonic" ) );
            } );

    return messages;
}


std::vector<wxString> MENU_DEF::UnresolvedActions( const std::list<TOOL_ACTION*>& aRegistered ) const
{
    std::vector<wxString> messages;

    Walk(
            [&]( const MENU_DEF& aNode, const std::vector<wxString>& aPath )
            {
                wxString where = joinPath( aPath, aNode.m_label );

                if( aNode.IsSeparator() || aNode.IsSlot() )
                    return;

                if( aNode.m_children.empty() && !aNode.m_action )
                {
                    messages.push_back( where + wxS( ": a leaf with no action, or a submenu with no entries" ) );
                    return;
                }

                if( !aNode.m_action )
                    return;

                if( aNode.m_action->GetName().empty() )
                    messages.push_back( where + wxS( ": the action has no name, so nothing registers it" ) );
                else if( std::find( aRegistered.begin(), aRegistered.end(), aNode.m_action ) == aRegistered.end() )
                    messages.push_back( where + wxS( ": action " ) + wxString( aNode.m_action->GetName() )
                                        + wxS( " is not registered" ) );
            } );

    return messages;
}

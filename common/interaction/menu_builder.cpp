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

#include <tool/action_menu.h>
#include <tool/conditional_menu.h>
#include <tool/tool_action.h>
#include <widgets/wx_menubar.h>


ACTION_MENU* BuildActionMenu( const MENU_DEF& aSubmenu, TOOL_INTERACTIVE* aTool, const MENU_SLOT_FILLER& aFillSlot )
{
    ACTION_MENU* menu = new ACTION_MENU( false, aTool );
    menu->SetTitle( aSubmenu.m_label );

    for( const MENU_DEF& entry : aSubmenu.m_children )
    {
        if( !entry.IsAvailable() || entry.m_isKeyOnly )
            continue;

        if( entry.IsSeparator() )
        {
            menu->AppendSeparator();
        }
        else if( entry.IsSlot() )
        {
            if( aFillSlot )
                aFillSlot( entry.m_slot, entry, *menu );
        }
        else if( entry.IsAction() )
        {
            menu->Add( *entry.m_action, entry.m_isCheck, entry.m_label );
        }
        else
        {
            menu->Add( BuildActionMenu( entry, aTool, aFillSlot ) );
        }
    }

    return menu;
}


CONDITIONAL_MENU* BuildConditionalMenu( const MENU_DEF& aSubmenu, TOOL_INTERACTIVE* aTool )
{
    CONDITIONAL_MENU* menu = new CONDITIONAL_MENU( aTool );
    menu->SetTitle( aSubmenu.m_label );

    for( const MENU_DEF& entry : aSubmenu.m_children )
    {
        if( !entry.IsAvailable() || entry.m_isKeyOnly )
            continue;

        if( entry.IsSeparator() )
        {
            menu->AddSeparator( entry.m_show );
        }
        else if( entry.IsSlot() )
        {
            continue; // only a frame's menu bar has slots
        }
        else if( entry.IsAction() )
        {
            if( entry.m_isCheck )
                menu->AddCheckItem( *entry.m_action, entry.m_show );
            else
                menu->AddItem( *entry.m_action, entry.m_show );
        }
        else
        {
            menu->AddMenu( BuildConditionalMenu( entry, aTool ), entry.m_show );
        }
    }

    return menu;
}


WX_MENUBAR* BuildMenuBar( const MENU_DEF& aBar, TOOL_INTERACTIVE* aTool, const MENU_SLOT_FILLER& aFillSlot )
{
    WX_MENUBAR* menuBar = new WX_MENUBAR();

    for( const MENU_DEF& menu : aBar.m_children )
    {
        if( menu.IsAvailable() && !menu.m_isKeyOnly )
            menuBar->Append( BuildActionMenu( menu, aTool, aFillSlot ), menu.m_label );
    }

    return menuBar;
}

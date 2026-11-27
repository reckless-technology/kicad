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

#ifndef CONTEXT_MENU_CHECKS_H
#define CONTEXT_MENU_CHECKS_H

#include <boost/test/unit_test.hpp>

#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <wx/menu.h>

#include <menu_def.h>
#include <tool/tool_action.h>

#ifndef KICAD_SOURCE_DIR
#error "KICAD_SOURCE_DIR must name the top of the source tree"
#endif


/**
 * A source file in which a tool puts actions into the selection tool's idle context menu, and the names of the other
 * menus the file adds actions to (a command's own menu), which are not read.
 */
struct STOCK_MENU_SOURCE
{
    const char*              m_path; ///< relative to the top of the source tree
    std::vector<std::string> m_otherMenus;
};


/// An action KiCad's own context menu has and the Altium-style tree deliberately leaves out, or reaches another way
struct MENU_DROP
{
    const char* m_identifier; ///< the action as the source names it: `SCH_ACTIONS::drawWire`
    const char* m_reason;
};


/**
 * Checks that the Altium-style right-click tree loses nothing KiCad's own idle context menu offers.
 *
 * KiCad's menu is built by the Init() of a dozen tools, each of which needs its editor frame, and a test cannot make
 * a frame. So the stock side is read from the tools' source files: every action a tool passes to Add(), AddItem(),
 * AddCheckItem() or AddMenu() of a menu that is not a command's own. Reading source is a looser check than running
 * the menu, and it is the nearest honest one: nothing here is a copy of the list.
 */
namespace CONTEXT_MENU_CHECKS
{

inline std::string ReadSource( const std::string& aRelativePath )
{
    std::ifstream     file( std::string( KICAD_SOURCE_DIR ) + "/" + aRelativePath );
    std::stringstream text;

    text << file.rdbuf();

    return text.str();
}


/// Each action a set of files defines, as `SCH_ACTIONS::drawWire`, with the name it is registered under
inline std::map<std::string, std::string> NamesByIdentifier( const std::vector<std::string>& aDefinitionFiles )
{
    static const std::regex definition(
            R"re(TOOL_ACTION\s+(\w+)::(\w+)\s*\(\s*TOOL_ACTION_ARGS\(\)\s*\.Name\(\s*"([^"]+)")re" );

    std::map<std::string, std::string> names;

    for( const std::string& file : aDefinitionFiles )
    {
        std::string text = ReadSource( file );

        for( std::sregex_iterator it( text.begin(), text.end(), definition ), end; it != end; ++it )
            names[( *it )[1].str() + "::" + ( *it )[2].str()] = ( *it )[3].str();
    }

    return names;
}


/// The actions the tools put into the stock menu: `SCH_ACTIONS::drawWire`
inline std::set<std::string> StockIdentifiers( const std::vector<STOCK_MENU_SOURCE>&     aSources,
                                               const std::map<std::string, std::string>& aNames )
{
    // `menu.AddItem( X::y`, `menu->Add( X::y`, and a bare `Add( X::y` inside a menu class
    static const std::regex withReceiver(
            R"((\w+)\s*(?:\.|->)\s*Add(?:Item|CheckItem|Menu)?\(\s*(\w*ACTIONS)::(\w+))" );
    static const std::regex bare( R"((?:^|[^\w.>])Add(?:Item|CheckItem)?\(\s*(\w*ACTIONS)::(\w+))" );

    std::set<std::string> identifiers;

    for( const STOCK_MENU_SOURCE& source : aSources )
    {
        std::string text = ReadSource( source.m_path );
        size_t      found = 0;

        for( std::sregex_iterator it( text.begin(), text.end(), withReceiver ), end; it != end; ++it )
        {
            const std::string& receiver = ( *it )[1].str();

            if( std::find( source.m_otherMenus.begin(), source.m_otherMenus.end(), receiver )
                != source.m_otherMenus.end() )
                continue;

            identifiers.insert( ( *it )[2].str() + "::" + ( *it )[3].str() );
            found++;
        }

        // A bare Add() is read line by line, so that `^` is a line start
        std::istringstream lines( text );
        std::string        line;

        while( std::getline( lines, line ) )
        {
            std::smatch match;

            if( std::regex_search( line, match, bare ) )
            {
                identifiers.insert( match[1].str() + "::" + match[2].str() );
                found++;
            }
        }

        // A file that gives nothing means the pattern or the file has changed, and the check has gone blind
        BOOST_CHECK_MESSAGE( found > 0, source.m_path << " gave no menu entries" );
    }

    // An editor's actions inherit the common ones, so a source may name `PCB_ACTIONS::cut` for `ACTIONS::cut`
    std::set<std::string> canonical;

    for( const std::string& identifier : identifiers )
    {
        std::string shared = "ACTIONS::" + identifier.substr( identifier.find( "::" ) + 2 );

        canonical.insert( !aNames.count( identifier ) && aNames.count( shared ) ? shared : identifier );
    }

    return canonical;
}


/// The identifiers of the actions in a tree
inline std::set<std::string> TreeIdentifiers( const MENU_DEF& aTree, const std::map<std::string, std::string>& aNames )
{
    std::map<std::string, std::string> identifierByName;

    for( const auto& [identifier, name] : aNames )
        identifierByName[name] = identifier;

    std::set<std::string> identifiers;

    for( const TOOL_ACTION* action : aTree.Actions() )
    {
        auto found = identifierByName.find( action->GetName() );

        BOOST_CHECK_MESSAGE( found != identifierByName.end(),
                             "the action " << action->GetName() << " has no definition" );

        if( found != identifierByName.end() )
            identifiers.insert( found->second );
    }

    return identifiers;
}


/**
 * The problems with a claim that the tree loses nothing: an action of the stock menu that is neither in the tree nor in
 * \a aDrops nor in \a aReachedByShared, and a named exception that is not true (not in the stock menu, or in the tree).
 *
 * @param aReachedByShared the actions of submenus that the tree shows as they are (a slot), so the tree has no
 *                         entry for them, with the reason
 */
inline std::vector<std::string> Problems( const std::set<std::string>& aStock, const std::set<std::string>& aTree,
                                          const std::vector<MENU_DROP>& aDrops,
                                          const std::vector<MENU_DROP>& aReachedByShared )
{
    std::vector<std::string> problems;
    std::set<std::string>    excused;

    for( const std::vector<MENU_DROP>* list : { &aDrops, &aReachedByShared } )
    {
        for( const MENU_DROP& drop : *list )
        {
            excused.insert( drop.m_identifier );

            if( !aStock.count( drop.m_identifier ) )
                problems.push_back( std::string( drop.m_identifier )
                                    + " is excused but the stock menu does not have it" );

            if( aTree.count( drop.m_identifier ) )
                problems.push_back( std::string( drop.m_identifier ) + " is excused but the tree has it" );

            if( std::string( drop.m_reason ).empty() )
                problems.push_back( std::string( drop.m_identifier ) + " is excused without a reason" );
        }
    }

    for( const std::string& identifier : aStock )
    {
        if( !aTree.count( identifier ) && !excused.count( identifier ) )
            problems.push_back( identifier + " is in the stock menu and neither in the tree nor excused" );
    }

    return problems;
}


/// The submenu of \a aMenu titled \a aLabel, or null
inline wxMenu* Submenu( const wxMenu& aMenu, const wxString& aLabel )
{
    for( const wxMenuItem* item : aMenu.GetMenuItems() )
    {
        if( item->IsSubMenu() && item->GetItemLabel() == aLabel )
            return item->GetSubMenu();
    }

    return nullptr;
}


/// Whether \a aMenu has an entry that runs \a aAction, not counting its submenus
inline bool HasAction( const wxMenu& aMenu, const TOOL_ACTION& aAction )
{
    for( const wxMenuItem* item : aMenu.GetMenuItems() )
    {
        if( item->GetId() == aAction.GetUIId() )
            return true;
    }

    return false;
}

} // namespace CONTEXT_MENU_CHECKS

#endif // CONTEXT_MENU_CHECKS_H

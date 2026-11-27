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

#ifndef MENU_TREE_CHECKS_H
#define MENU_TREE_CHECKS_H

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include <interaction_profile.h>
#include <key_sequence.h>
#include <menu_def.h>
#include <tool/action_manager.h>
#include <tool/tool_action.h>


/// A row of the interaction-profile note's mapping table that is marked "=": the action exists.
struct MENU_TREE_ROW
{
    const char* m_sequence;   ///< the keys, as the note writes them: "P V N"
    const char* m_actionName; ///< the action's registered name
    bool        m_isMenuPath; ///< whether the keys name a path of mnemonics through the tree
};


/// Checks of an editor's menu tree, shared by the executables that link that editor's actions.
namespace MENU_TREE_CHECKS
{

inline const TOOL_ACTION* FindRegistered( const std::string& aName )
{
    for( const TOOL_ACTION* action : ACTION_MANAGER::GetActionList() )
    {
        if( action->GetName() == aName )
            return action;
    }

    return nullptr;
}


/// The keys of a sequence as the note writes it ("P V N"): the first letter of each word
inline std::vector<wxUniChar> SequenceKeys( const std::string& aSequence )
{
    std::vector<wxUniChar> keys;
    std::istringstream     stream( aSequence );
    std::string            key;

    while( stream >> key )
        keys.push_back( wxUniChar( key[0] ) );

    return keys;
}


/// The entry a sequence of keys selects, by the rule the key-sequence engine uses: the first key names a menu or a
/// submenu of one, the rest are mnemonics.
inline const MENU_DEF* ResolveSequence( const MENU_DEF& aBar, const std::string& aSequence )
{
    return KEY_SEQUENCE::Resolve( aBar, SequenceKeys( aSequence ) );
}


/// Log each problem and fail when there is one. The problems come from the functions below, which are plain functions
/// so a test can also show that a faulty tree produces some.
inline void ExpectNoProblems( const std::vector<std::string>& aProblems, const char* aWhat )
{
    for( const std::string& problem : aProblems )
        BOOST_TEST_MESSAGE( problem );

    BOOST_CHECK_MESSAGE( aProblems.empty(), aProblems.size() << " problems: " << aWhat );
}


inline std::vector<std::string> ToStrings( const std::vector<wxString>& aMessages )
{
    std::vector<std::string> strings;

    for( const wxString& message : aMessages )
        strings.push_back( message.ToStdString() );

    return strings;
}


/// Every action in the tree is a registered one, and there are enough of them to make the check mean something.
inline std::vector<std::string> ActionProblems( const MENU_DEF& aBar, size_t aMinimumActions )
{
    std::vector<std::string> problems = ToStrings( aBar.UnresolvedActions( ACTION_MANAGER::GetActionList() ) );

    if( aBar.Actions().size() < aMinimumActions )
        problems.push_back( "the tree has " + std::to_string( aBar.Actions().size() ) + " actions, expected at least "
                            + std::to_string( aMinimumActions ) );

    return problems;
}


/// No two entries of one menu, at any level, share a mnemonic; every menu and submenu of the bar has one.
inline std::vector<std::string> MnemonicProblems( const MENU_DEF& aBar, size_t aMinimumMenus )
{
    std::vector<std::string> problems = ToStrings( aBar.DuplicateMnemonics() );

    if( aBar.m_children.size() < aMinimumMenus )
        problems.push_back( "the bar has " + std::to_string( aBar.m_children.size() ) + " menus, expected at least "
                            + std::to_string( aMinimumMenus ) );

    for( const MENU_DEF& menu : aBar.m_children )
    {
        if( !menu.Mnemonic() )
            problems.push_back( "menu \"" + menu.m_label.ToStdString() + "\" has no mnemonic" );

        for( const MENU_DEF& entry : menu.m_children )
        {
            if( entry.IsSubmenu() && !entry.Mnemonic() )
                problems.push_back( "submenu \"" + entry.m_label.ToStdString() + "\" of \"" + menu.m_label.ToStdString()
                                    + "\" has no mnemonic" );
        }
    }

    return problems;
}


/**
 * Every row of the note's table marked "=" has its action in the tree, or is on the list of deliberate omissions, so
 * nothing disappears without a decision. Rows that name a path of mnemonics also resolve to their action.
 */
inline std::vector<std::string> DisappearanceProblems( const MENU_DEF&                   aBar,
                                                       const std::vector<MENU_OMISSION>& aOmissions,
                                                       const std::vector<MENU_TREE_ROW>& aRows )
{
    std::vector<std::string> problems;

    if( aRows.size() < 20 )
        problems.push_back( "the table has " + std::to_string( aRows.size() ) + " rows" );

    if( aOmissions.empty() )
        problems.push_back( "the list of omissions is empty" );

    for( const MENU_TREE_ROW& row : aRows )
    {
        const TOOL_ACTION* action = FindRegistered( row.m_actionName );

        if( !action )
        {
            problems.push_back( std::string( "row " ) + row.m_sequence + " names " + row.m_actionName
                                + ", which no program registers" );
            continue;
        }

        bool isOmitted = std::any_of( aOmissions.begin(), aOmissions.end(),
                                      [&]( const MENU_OMISSION& aOmission )
                                      {
                                          return aOmission.m_actionName
                                                 && std::string( aOmission.m_actionName ) == row.m_actionName;
                                      } );

        if( aBar.Contains( *action ) && isOmitted )
            problems.push_back( std::string( row.m_actionName ) + " is in the tree and on the list of omissions" );
        else if( !aBar.Contains( *action ) && !isOmitted )
            problems.push_back( std::string( "row " ) + row.m_sequence + " (" + row.m_actionName
                                + ") is neither in the menu tree nor on the list of omissions" );

        if( row.m_isMenuPath )
        {
            const MENU_DEF* node = ResolveSequence( aBar, row.m_sequence );

            if( !node || node->KeyAction() != action )
                problems.push_back( std::string( "the keys " ) + row.m_sequence + " do not select " + row.m_actionName
                                    + " in the tree, they select "
                                    + ( node ? "\"" + node->m_label.ToStdString() + "\"" : std::string( "nothing" ) ) );
        }
    }

    for( const MENU_OMISSION& omission : aOmissions )
    {
        if( !omission.m_reason || !*omission.m_reason )
            problems.push_back( std::string( "the omission " ) + omission.m_sequence + " has no reason" );

        // An omission that names an action must name one that exists, or the list has gone stale
        if( omission.m_actionName && !FindRegistered( omission.m_actionName ) )
            problems.push_back( std::string( "the omission " ) + omission.m_sequence + " names " + omission.m_actionName
                                + ", which no program registers" );
    }

    return problems;
}


using SEQUENCE_ROOTS = std::vector<std::pair<const TOOL_ACTION*, wxUniChar>>;


/**
 * Type each row's sequence into the engine as the program does (the root action first, then one key at a time) and
 * check that the action the note names is the one that runs, and that nothing else does on the way. A row that is not
 * a path of mnemonics is typed too: the engine is the only way to a sequence.
 */
inline std::vector<std::string> SequenceProblems( const MENU_DEF& aBar, const SEQUENCE_ROOTS& aRoots,
                                                  const std::vector<MENU_TREE_ROW>& aRows )
{
    std::vector<std::string> problems;

    if( aRows.size() < 20 )
        problems.push_back( "the table has " + std::to_string( aRows.size() ) + " rows" );

    for( const MENU_TREE_ROW& row : aRows )
    {
        std::vector<wxUniChar> keys = SequenceKeys( row.m_sequence );
        const TOOL_ACTION*     expected = FindRegistered( row.m_actionName );
        std::string            where = std::string( "the keys " ) + row.m_sequence + ": ";

        if( keys.size() < 2 || !expected )
        {
            problems.push_back( where + "the row has no second key or names an action nothing registers" );
            continue;
        }

        const TOOL_ACTION* root = nullptr;

        for( const auto& [action, letter] : aRoots )
        {
            if( letter == keys[0] )
                root = action;
        }

        if( !root )
        {
            problems.push_back( where + "no root action opens the first key's menu" );
            continue;
        }

        KEY_SEQUENCE engine;

        engine.SetTree( std::make_shared<const MENU_DEF>( aBar ) );

        if( engine.Enter( keys[0] ).m_outcome != KEY_SEQUENCE_RESULT::OUTCOME::CONSUMED )
        {
            problems.push_back( where + "the root does not open a menu of the tree" );
            continue;
        }

        const TOOL_ACTION* ran = nullptr;

        for( size_t i = 1; i < keys.size(); ++i )
        {
            using OUTCOME = KEY_SEQUENCE_RESULT::OUTCOME;

            bool                isLast = i + 1 == keys.size();
            KEY_SEQUENCE_RESULT result = engine.HandleKey( static_cast<int>( keys[i].GetValue() ), false );

            if( result.m_outcome == OUTCOME::RUN_ACTION )
            {
                ran = result.m_action;

                if( !isLast )
                    problems.push_back( where + "an action ran before the last key" );

                break;
            }

            if( result.m_outcome != OUTCOME::CONSUMED )
            {
                problems.push_back( where + "the engine let a key through that the sequence needs" );
                break;
            }
        }

        if( ran != expected )
            problems.push_back( where + "ran " + ( ran ? ran->GetName() : std::string( "nothing" ) )
                                + ", the note says " + row.m_actionName );

        if( engine.IsPending() )
            problems.push_back( where + "the engine is still waiting for a key" );
    }

    return problems;
}


/**
 * The roots: every root action is registered, its menu is in the tree, and the profile's table binds it to the letter
 * it opens (so the key the user types is the one the tree reads). Every letter that starts a row has a root.
 */
inline std::vector<std::string> RootProblems( const MENU_DEF& aBar, const SEQUENCE_ROOTS& aRoots,
                                              const PROFILE_HOTKEY_LAYER&       aTable,
                                              const std::vector<MENU_TREE_ROW>& aRows )
{
    std::vector<std::string> problems;

    if( aRoots.size() < 10 )
        problems.push_back( "the editor has " + std::to_string( aRoots.size() ) + " roots" );

    for( const auto& [action, letter] : aRoots )
    {
        std::string name = action ? action->GetName() : std::string( "(null)" );

        if( !action || !FindRegistered( name ) )
        {
            problems.push_back( "root " + name + " is not registered" );
            continue;
        }

        const MENU_DEF* menu = KEY_SEQUENCE::FindRoot( aBar, letter );

        if( !menu || !menu->IsSubmenu() )
            problems.push_back( "root " + name + " opens no menu of the tree" );

        auto binding = aTable.m_bindings.find( name );

        if( binding == aTable.m_bindings.end() )
            problems.push_back( "root " + name + " has no key in the profile's table" );
        else if( binding->second.first != static_cast<int>( letter.GetValue() ) )
            problems.push_back( "root " + name + " is bound to a key other than "
                                + std::string( 1, (char) letter.GetValue() ) );
    }

    for( const MENU_TREE_ROW& row : aRows )
    {
        wxUniChar first = SequenceKeys( row.m_sequence ).at( 0 );
        bool      hasRoot = std::any_of( aRoots.begin(), aRoots.end(),
                                         [&]( const auto& aRoot )
                                         {
                                        return aRoot.second == first;
                                         } );

        if( !hasRoot )
            problems.push_back( std::string( "the row " ) + row.m_sequence + " starts with a letter no root has" );
    }

    return problems;
}

} // namespace MENU_TREE_CHECKS

#endif // MENU_TREE_CHECKS_H

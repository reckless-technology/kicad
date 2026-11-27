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
#include <boost/test/unit_test.hpp>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <hotkeys_basic.h>
#include <interaction_profile.h>
#include <tool/action_manager.h>
#include <tool/tool_action.h>

// Every action a profile's table names is an action this program registers (interaction-profile note, section 8). This
// file is built into each QA executable that links actions, with the action name prefixes that executable owns in
// INTERACTION_PROFILE_QA_PREFIXES: a table also names the other editors' actions, and they are checked where those are
// linked.

#ifndef INTERACTION_PROFILE_QA_PREFIXES
#error "INTERACTION_PROFILE_QA_PREFIXES: the action name prefixes this executable links"
#endif

BOOST_AUTO_TEST_SUITE( InteractionProfileActions )

BOOST_AUTO_TEST_CASE( EveryActionInEachProfileTableIsRegistered )
{
    std::vector<TOOL_ACTION*> registered( ACTION_MANAGER::GetActionList().begin(),
                                          ACTION_MANAGER::GetActionList().end() );

    for( INTERACTION_PROFILE_ID profile : INTERACTION_PROFILE::AllProfiles() )
    {
        PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( profile );

        // A profile other than KiCad's must bring a table, or this test passes on nothing.
        if( layer.m_isActive )
            BOOST_CHECK_MESSAGE( !layer.m_bindings.empty(), "an active profile's hotkey table is empty" );

        std::vector<std::string> unregistered =
                INTERACTION_PROFILE::UnregisteredActionNames( layer, registered, { INTERACTION_PROFILE_QA_PREFIXES } );

        // Each name is a failure of its own, so a typo is named
        // The PCB editor registers magneticSnapToggle under common.; only an executable that links it can check it
        const std::string pcbOwnedName = "common.Control.magneticSnapToggle";
        bool              hasPcbActions = false;

        for( const std::string& prefix : std::vector<std::string>( { INTERACTION_PROFILE_QA_PREFIXES } ) )
            hasPcbActions = hasPcbActions || prefix == "pcbnew.";

        if( !hasPcbActions )
            unregistered.erase( std::remove( unregistered.begin(), unregistered.end(), pcbOwnedName ),
                                unregistered.end() );

        for( const std::string& name : unregistered )
            BOOST_ERROR( "profile table names an action nothing registers: " << name );

        // ... and the check is on something: the table has names under this executable's prefixes
        if( layer.m_isActive )
        {
            size_t checked = 0;

            for( const auto& [name, keys] : layer.m_bindings )
            {
                for( const std::string& prefix : std::vector<std::string>( { INTERACTION_PROFILE_QA_PREFIXES } ) )
                    checked += name.compare( 0, prefix.size(), prefix ) == 0 ? 1 : 0;
            }

            BOOST_CHECK_GE( checked, 10u );
            BOOST_TEST_MESSAGE( checked << " names of the profile's table checked against the registered actions" );
        }
    }
}


BOOST_AUTO_TEST_CASE( EveryKeyOfTheAltiumTableIsSpelledTheWayTheHotkeyFileSpellsIt )
{
    std::string table = INTERACTION_PROFILE::HotKeyTableText( INTERACTION_PROFILE_ID::ALTIUM );
    size_t      keysChecked = 0;
    size_t      lineStart = 0;

    while( lineStart < table.size() )
    {
        size_t lineEnd = table.find( '\n', lineStart );

        if( lineEnd == std::string::npos )
            lineEnd = table.size();

        std::string line = table.substr( lineStart, lineEnd - lineStart );

        lineStart = lineEnd + 1;

        if( line.empty() || line[0] == '#' )
            continue;

        std::vector<std::string> fields;
        size_t                   fieldStart = 0;

        while( fieldStart <= line.size() )
        {
            size_t fieldEnd = line.find( '\t', fieldStart );

            if( fieldEnd == std::string::npos )
                fieldEnd = line.size();

            fields.push_back( line.substr( fieldStart, fieldEnd - fieldStart ) );
            fieldStart = fieldEnd + 1;
        }

        BOOST_CHECK_MESSAGE( fields.size() >= 2 && fields.size() <= 3,
                             "a line of the table has " << fields.size() << " fields: " << line );

        for( size_t i = 1; i < fields.size(); ++i )
        {
            if( fields[i].empty() )
                continue;

            int code = KeyCodeFromKeyName( wxString::FromUTF8( fields[i] ) );

            BOOST_CHECK_MESSAGE( code != 0, "the key \"" << fields[i] << "\" of " << fields[0] << " is not a key" );
            BOOST_CHECK_MESSAGE( KeyNameFromKeyCode( code ).ToStdString() == fields[i],
                                 "the key \"" << fields[i] << "\" of " << fields[0] << " is spelled "
                                              << KeyNameFromKeyCode( code ).ToStdString() << " by the hotkey file" );
            ++keysChecked;
        }
    }

    BOOST_CHECK_GE( keysChecked, 40u );
}


BOOST_AUTO_TEST_CASE( NoTwoActionsInOneScopeAndToolShareAKeyUnderTheAltiumProfile )
{
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );
    size_t               keysResolved = 0;

    // Each editor this executable links is a scope, with the actions of common beside its own: a key may be one thing in
    // the schematic editor and another in the PCB editor, but within one a tool's actions must not share it
    for( const std::string& scope : std::vector<std::string>( { INTERACTION_PROFILE_QA_PREFIXES } ) )
    {
        std::map<std::pair<std::string, int>, std::vector<std::string>> actionsByToolAndKey;

        for( const TOOL_ACTION* action : ACTION_MANAGER::GetActionList() )
        {
            std::string actionScope = INTERACTION_PROFILE::ScopeOf( action->GetName() );

            if( actionScope != "common." && actionScope != scope )
                continue;

            std::pair<int, int> keys = ACTION_MANAGER::ResolveHotKey( *action, std::map<std::string, int>(), layer,
                                                                      PROFILE_HOTKEY_MAP(), scope );

            for( int key : { keys.first, keys.second } )
            {
                if( key > 0 )
                {
                    actionsByToolAndKey[{ action->GetToolName(), key }].push_back( action->GetName() );
                    ++keysResolved;
                }
            }
        }

        for( const auto& [toolAndKey, names] : actionsByToolAndKey )
        {
            // A clash between two actions the profile never touched is the stock program's, and not the table's
            bool isTableAction = std::any_of( names.begin(), names.end(),
                                              [&]( const std::string& aName )
                                              {
                                                  return layer.m_bindings.count( aName ) > 0;
                                              } );

            if( names.size() < 2 || !isTableAction )
                continue;

            std::string list;

            for( const std::string& name : names )
                list += " " + name;

            BOOST_ERROR( scope << " tool " << toolAndKey.first << ": " << KeyNameFromKeyCode( toolAndKey.second )
                               << " is the key of" << list );
        }
    }

    BOOST_CHECK_GE( keysResolved, 50u );
}


BOOST_AUTO_TEST_CASE( TheKiCadProfileResolvesEveryLinkedActionToItsCompiledDefault )
{
    BOOST_REQUIRE( !ACTION_MANAGER::GetActionList().empty() );

    PROFILE_HOTKEY_LAYER kicadLayer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::STOCK );

    for( const TOOL_ACTION* action : ACTION_MANAGER::GetActionList() )
    {
        std::pair<int, int> keys = ACTION_MANAGER::ResolveHotKey( *action, std::map<std::string, int>(), kicadLayer,
                                                                  PROFILE_HOTKEY_MAP() );

        BOOST_CHECK_MESSAGE( keys == std::make_pair( action->GetDefaultHotKey(), action->GetDefaultHotKeyAlt() ),
                             action->GetName() << " is not its compiled default under the KiCad profile" );
    }
}

BOOST_AUTO_TEST_SUITE_END()

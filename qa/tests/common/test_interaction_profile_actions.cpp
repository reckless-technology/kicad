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

        std::vector<std::string> unregistered =
                INTERACTION_PROFILE::UnregisteredActionNames( layer, registered, { INTERACTION_PROFILE_QA_PREFIXES } );

        for( const std::string& name : unregistered )
            BOOST_ERROR( "profile table names an action nothing registers: " << name );
    }
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

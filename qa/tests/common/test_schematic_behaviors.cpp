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

/**
 * @file test_schematic_behaviors.cpp
 * The schematic behaviors of the Altium-style interaction profile (design note, 5.9) as the decisions they come down
 * to: move or drag under Ctrl, what a box selection adds, whether the next label repeats the last, and the nudge keys.
 */

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <hotkeys_basic.h>
#include <interaction_profile.h>
#include <math/vector2d.h>
#include <tool/action_manager.h>
#include <tool/actions.h>
#include <tool/tool_action.h>
#include <wx/string.h>

namespace
{
const INTERACTION_PROFILE_ID STOCK = INTERACTION_PROFILE_ID::STOCK;
const INTERACTION_PROFILE_ID ALTIUM = INTERACTION_PROFILE_ID::ALTIUM;


struct NUDGE_ENTRY
{
    TOOL_ACTION*               m_action;
    std::string                m_name;
    ACTIONS::CURSOR_EVENT_TYPE m_direction;
    int                        m_altiumKey;
};


std::vector<NUDGE_ENTRY> nudgeActions()
{
    const int ctrl = MD_CTRL;
    const int ctrlShift = MD_CTRL + MD_SHIFT;

    return { { &ACTIONS::nudgeUp, "common.Interactive.nudgeUp", ACTIONS::CURSOR_UP, ctrl + WXK_UP },
             { &ACTIONS::nudgeDown, "common.Interactive.nudgeDown", ACTIONS::CURSOR_DOWN, ctrl + WXK_DOWN },
             { &ACTIONS::nudgeLeft, "common.Interactive.nudgeLeft", ACTIONS::CURSOR_LEFT, ctrl + WXK_LEFT },
             { &ACTIONS::nudgeRight, "common.Interactive.nudgeRight", ACTIONS::CURSOR_RIGHT, ctrl + WXK_RIGHT },
             { &ACTIONS::nudgeUpFast, "common.Interactive.nudgeUpFast", ACTIONS::CURSOR_UP_FAST, ctrlShift + WXK_UP },
             { &ACTIONS::nudgeDownFast, "common.Interactive.nudgeDownFast", ACTIONS::CURSOR_DOWN_FAST,
               ctrlShift + WXK_DOWN },
             { &ACTIONS::nudgeLeftFast, "common.Interactive.nudgeLeftFast", ACTIONS::CURSOR_LEFT_FAST,
               ctrlShift + WXK_LEFT },
             { &ACTIONS::nudgeRightFast, "common.Interactive.nudgeRightFast", ACTIONS::CURSOR_RIGHT_FAST,
               ctrlShift + WXK_RIGHT } };
}


std::string nextLabelText( const wxString& aPreviousText, int aIncrement )
{
    return INTERACTION_PROFILE::NextRepeatedLabelText( aPreviousText, aIncrement ).ToStdString();
}


int keyUnder( INTERACTION_PROFILE_ID aProfile, const TOOL_ACTION& aAction )
{
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( aProfile );

    return INTERACTION_PROFILE::ApplyHotKeyLayer( layer, aAction.GetName(),
                                                  { aAction.GetHotKey(), aAction.GetHotKeyAlt() } )
            .first;
}
} // namespace


BOOST_AUTO_TEST_SUITE( SchematicBehaviors )


BOOST_AUTO_TEST_CASE( CtrlDragGivesTheOtherOfMoveAndDragOnlyUnderTheAltiumProfile )
{
    // A plain drag keeps the wires unless drag_is_move is on, in both profiles
    BOOST_CHECK( INTERACTION_PROFILE::ShouldDragWithWires( STOCK, false, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::ShouldDragWithWires( STOCK, true, false ) );
    BOOST_CHECK( INTERACTION_PROFILE::ShouldDragWithWires( ALTIUM, false, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::ShouldDragWithWires( ALTIUM, true, false ) );

    // Ctrl is the other one under the Altium-style profile, and means nothing here under stock
    BOOST_CHECK( INTERACTION_PROFILE::ShouldDragWithWires( STOCK, false, true ) );
    BOOST_CHECK( !INTERACTION_PROFILE::ShouldDragWithWires( STOCK, true, true ) );
    BOOST_CHECK( !INTERACTION_PROFILE::ShouldDragWithWires( ALTIUM, false, true ) );
    BOOST_CHECK( INTERACTION_PROFILE::ShouldDragWithWires( ALTIUM, true, true ) );
}


BOOST_AUTO_TEST_CASE( CtrlDragIsAnInversionOnlyForCtrlWithoutShiftUnderTheAltiumProfile )
{
    BOOST_CHECK( INTERACTION_PROFILE::IsCtrlDragInversion( ALTIUM, true, false ) );

    // Ctrl+Shift stays the subtractive box selection, and no Ctrl is no inversion
    BOOST_CHECK( !INTERACTION_PROFILE::IsCtrlDragInversion( ALTIUM, true, true ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsCtrlDragInversion( ALTIUM, false, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsCtrlDragInversion( ALTIUM, false, true ) );

    // Stock KiCad keeps Ctrl as a selection modifier
    BOOST_CHECK( !INTERACTION_PROFILE::IsCtrlDragInversion( STOCK, true, false ) );
}


BOOST_AUTO_TEST_CASE( ABoxSelectionAddsWithShiftAndCtrlInStockAndWithShiftAloneUnderAltium )
{
    // Stock: Ctrl or Shift adds, Alt cancels
    BOOST_CHECK( !INTERACTION_PROFILE::IsBoxSelectionAdditive( STOCK, false, false, false ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsBoxSelectionAdditive( STOCK, true, false, false ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsBoxSelectionAdditive( STOCK, false, true, false ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsBoxSelectionAdditive( STOCK, true, true, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsBoxSelectionAdditive( STOCK, true, true, true ) );

    // Altium: Shift adds, Ctrl alone no longer does, Alt cancels
    BOOST_CHECK( !INTERACTION_PROFILE::IsBoxSelectionAdditive( ALTIUM, false, false, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsBoxSelectionAdditive( ALTIUM, true, false, false ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsBoxSelectionAdditive( ALTIUM, false, true, false ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsBoxSelectionAdditive( ALTIUM, true, true, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsBoxSelectionAdditive( ALTIUM, false, true, true ) );
}


BOOST_AUTO_TEST_CASE( PlacingRepeatedCopiesStartsCheckedOnlyUnderTheAltiumProfile )
{
    BOOST_CHECK( INTERACTION_PROFILE::IsRepeatedPlacementDefault( ALTIUM ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsRepeatedPlacementDefault( STOCK ) );
}


BOOST_AUTO_TEST_CASE( ALabelRepeatsOnlyUnderTheAltiumProfileAndOnlyAfterTheFirst )
{
    // The first label of a session opens the dialog, as in stock
    BOOST_CHECK( !INTERACTION_PROFILE::IsLabelRepeated( ALTIUM, false ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsLabelRepeated( ALTIUM, true ) );

    // Stock always opens it
    BOOST_CHECK( !INTERACTION_PROFILE::IsLabelRepeated( STOCK, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsLabelRepeated( STOCK, true ) );
}


BOOST_AUTO_TEST_CASE( TheRepeatedLabelTextMovesItsTrailingNumberByTheIncrement )
{
    BOOST_CHECK_EQUAL( nextLabelText( wxS( "DATA1" ), 1 ), "DATA2" );
    BOOST_CHECK_EQUAL( nextLabelText( wxS( "DATA9" ), 1 ), "DATA10" );
    BOOST_CHECK_EQUAL( nextLabelText( wxS( "A7" ), 2 ), "A9" );
    BOOST_CHECK_EQUAL( nextLabelText( wxS( "D10" ), -1 ), "D09" ); // the width of the number is kept

    // Digits before a suffix, as Repeat Last Item treats them
    BOOST_CHECK_EQUAL( nextLabelText( wxS( "ADC3_IN" ), 1 ), "ADC4_IN" );

    // Text with no number is repeated as it is
    BOOST_CHECK_EQUAL( nextLabelText( wxS( "CLK" ), 1 ), "CLK" );
    BOOST_CHECK_EQUAL( nextLabelText( wxString(), 1 ), "" );
}


BOOST_AUTO_TEST_CASE( ANudgeMovesOneGridStepOrTenAlongItsAxisAndSwapsSidesWhenMirrored )
{
    const VECTOR2D grid( 1270000, 635000 );

    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_UP, grid, false ) == VECTOR2D( 0, -635000 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_DOWN, grid, false ) == VECTOR2D( 0, 635000 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_LEFT, grid, false ) == VECTOR2D( -1270000, 0 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_RIGHT, grid, false ) == VECTOR2D( 1270000, 0 ) );

    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_UP_FAST, grid, false ) == VECTOR2D( 0, -6350000 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_DOWN_FAST, grid, false ) == VECTOR2D( 0, 6350000 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_LEFT_FAST, grid, false ) == VECTOR2D( -12700000, 0 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_RIGHT_FAST, grid, false ) == VECTOR2D( 12700000, 0 ) );

    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_LEFT, grid, true ) == VECTOR2D( 1270000, 0 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_RIGHT_FAST, grid, true ) == VECTOR2D( -12700000, 0 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_UP, grid, true ) == VECTOR2D( 0, -635000 ) );

    // Anything that is not an arrow moves nothing
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_CLICK, grid, false ) == VECTOR2D( 0, 0 ) );
    BOOST_CHECK( ACTIONS::NudgeOffset( ACTIONS::CURSOR_NONE, grid, false ) == VECTOR2D( 0, 0 ) );
}


BOOST_AUTO_TEST_CASE( TheNudgeActionsAreRegisteredAndCarryTheirDirection )
{
    const std::list<TOOL_ACTION*>& registered = ACTION_MANAGER::GetActionList();

    for( const NUDGE_ENTRY& entry : nudgeActions() )
    {
        BOOST_CHECK_EQUAL( entry.m_action->GetName(), entry.m_name );
        BOOST_CHECK_MESSAGE( std::find( registered.begin(), registered.end(), entry.m_action ) != registered.end(),
                             entry.m_name << " is not registered" );

        // The direction rides on the event the tools read it from
        TOOL_EVENT event = entry.m_action->MakeEvent();
        BOOST_CHECK( event.Parameter<ACTIONS::CURSOR_EVENT_TYPE>() == entry.m_direction );
    }
}


BOOST_AUTO_TEST_CASE( TheNudgeActionsAreBoundToCtrlArrowsUnderAltiumAndUnboundInStock )
{
    for( const NUDGE_ENTRY& entry : nudgeActions() )
    {
        BOOST_CHECK_EQUAL( entry.m_action->GetHotKey(), 0 );
        BOOST_CHECK_EQUAL( entry.m_action->GetHotKeyAlt(), 0 );

        BOOST_CHECK_MESSAGE( keyUnder( ALTIUM, *entry.m_action ) == entry.m_altiumKey,
                             entry.m_name << " is not on its Altium-style key" );
        BOOST_CHECK_MESSAGE( keyUnder( STOCK, *entry.m_action ) == 0, entry.m_name << " is bound in stock" );
    }
}


BOOST_AUTO_TEST_CASE( TheCursorFastKeysMoveToShiftArrowsAndStockKeepsCtrlArrows )
{
    // The nudge takes Ctrl+arrows, so the cursor-fast actions are on Shift+arrows under the profile ...
    BOOST_CHECK_EQUAL( keyUnder( ALTIUM, ACTIONS::cursorUpFast ), MD_SHIFT + static_cast<int>( WXK_UP ) );
    BOOST_CHECK_EQUAL( keyUnder( ALTIUM, ACTIONS::cursorLeftFast ), MD_SHIFT + static_cast<int>( WXK_LEFT ) );

    // ... and nothing else is on a nudge key
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( ALTIUM );

    for( const NUDGE_ENTRY& entry : nudgeActions() )
    {
        for( const auto& [name, keys] : layer.m_bindings )
        {
            if( name == entry.m_name )
                continue;

            BOOST_CHECK_MESSAGE( keys.first != entry.m_altiumKey && keys.second != entry.m_altiumKey,
                                 name << " shares a key with " << entry.m_name );
        }
    }

    // Stock keeps them where they were
    BOOST_CHECK_EQUAL( keyUnder( STOCK, ACTIONS::cursorUpFast ), MD_CTRL + static_cast<int>( WXK_UP ) );
    BOOST_CHECK_EQUAL( keyUnder( STOCK, ACTIONS::cursorRightFast ), MD_CTRL + static_cast<int>( WXK_RIGHT ) );
}

BOOST_AUTO_TEST_SUITE_END()

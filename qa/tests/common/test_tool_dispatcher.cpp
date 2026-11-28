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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <boost/test/unit_test.hpp>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <settings/common_settings.h>
#include <tool/actions.h>
#include <tool/tool_dispatcher.h>
#include <tool/tool_manager.h>
#include <tool/tool_interactive.h>
#include <tool/action_menu.h>

#include <wx/display.h>
#include <wx/evtloop.h>

/**
 * Tests for TOOL_DISPATCHER::ShouldDropAutoRepeat, the pure decision that discards backlogged
 * OS key auto-repeat events delivered after a hotkey was released (issue 24540).
 */

BOOST_AUTO_TEST_SUITE( ToolDispatcherAutoRepeat )

// A single key press whose key has already been released (a quick tap) must always run.
BOOST_AUTO_TEST_CASE( LeadingEdgeAlwaysRuns )
{
    int        lastKey = 0;
    wxLongLong lastTime = 0;

    bool drop = TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', /*now*/ 1000, /*keyIsDown*/ false,
                                                       lastKey, lastTime );

    BOOST_CHECK( !drop );
    BOOST_CHECK_EQUAL( lastKey, 'R' );
    BOOST_CHECK( lastTime == 1000 );
}

// Repeats that arrive within the burst window while the key is still held keep running.
BOOST_AUTO_TEST_CASE( HeldKeyRepeatsRun )
{
    int        lastKey = 0;
    wxLongLong lastTime = 0;

    BOOST_CHECK( !TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', 1000, true, lastKey, lastTime ) );
    BOOST_CHECK( !TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', 1030, true, lastKey, lastTime ) );
    BOOST_CHECK( !TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', 1060, true, lastKey, lastTime ) );
}

// The defect: repeats from the same burst that arrive after release must be dropped.
BOOST_AUTO_TEST_CASE( StaleRepeatAfterReleaseDropped )
{
    int        lastKey = 0;
    wxLongLong lastTime = 0;

    BOOST_CHECK( !TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', 1000, true, lastKey, lastTime ) );

    // Backlogged events still in the queue after the key came up.
    BOOST_CHECK( TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', 1030, false, lastKey, lastTime ) );
    BOOST_CHECK( TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', 1060, false, lastKey, lastTime ) );
}

// A second deliberate tap after a human-scale gap is a new burst and must run again.
BOOST_AUTO_TEST_CASE( SecondDeliberateTapRuns )
{
    int        lastKey = 0;
    wxLongLong lastTime = 0;

    BOOST_CHECK( !TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', 1000, false, lastKey, lastTime ) );

    // Well beyond the auto-repeat window, so this is treated as a fresh press, not stale backlog.
    wxLongLong later = 1000 + TOOL_DISPATCHER::AutoRepeatWindowMs + 1;
    BOOST_CHECK( !TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', later, false, lastKey, lastTime ) );
}

// Switching to a different key starts a new burst whose leading edge runs.
BOOST_AUTO_TEST_CASE( DifferentKeyIsNewBurst )
{
    int        lastKey = 0;
    wxLongLong lastTime = 0;

    BOOST_CHECK( !TOOL_DISPATCHER::ShouldDropAutoRepeat( 'R', 1000, true, lastKey, lastTime ) );

    // A different hotkey arriving immediately is not part of the R burst.
    BOOST_CHECK( !TOOL_DISPATCHER::ShouldDropAutoRepeat( 'F', 1010, false, lastKey, lastTime ) );
}

BOOST_AUTO_TEST_SUITE_END()


namespace
{
// TOOL_EVENT's constructor reads ACTIONS::cancelInteractive, a static in another translation
// unit, so the event has to be initialized on first use rather than at file scope
const TOOL_EVENT& armMenuEvent()
{
    static const TOOL_EVENT evt( TC_COMMAND, TA_ACTION, std::string( "test.armContextMenu" ) );

    return evt;
}


class YIELDING_EVENT_LOOP : public wxEventLoop
{
public:
    bool IsYielding() const override { return true; }
};


class CONTEXT_MENU_TEST_TOOL : public TOOL_INTERACTIVE
{
public:
    CONTEXT_MENU_TEST_TOOL() :
            TOOL_INTERACTIVE( "test.contextMenuTool" ),
            m_menuRan( false ),
            m_menu( true )
    {
    }

    void Reset( RESET_REASON aReason [[maybe_unused]] ) override {}

    int ArmMenu( const TOOL_EVENT& aEvent [[maybe_unused]] )
    {
        Activate();
        SetContextMenu( &m_menu, CMENU_NOW );
        Wait( TOOL_EVENT( TC_COMMAND, TA_CHOICE_MENU_CLOSED ) );
        m_menuRan = true;

        return 0;
    }

    bool m_menuRan;

private:
    void setTransitions() override { Go( &CONTEXT_MENU_TEST_TOOL::ArmMenu, armMenuEvent() ); }

    ACTION_MENU m_menu;
};
} // namespace


BOOST_AUTO_TEST_SUITE( ToolManagerContextMenu )

BOOST_AUTO_TEST_CASE( RightClickOpensMenuWhileEventLoopYields )
{
    if( !KI_TEST::CanDoDisplayTests() )
    {
        BOOST_TEST_MESSAGE( "Skipping test - no display available" );
        return;
    }

    TOOL_MANAGER            mgr;
    CONTEXT_MENU_TEST_TOOL* tool = new CONTEXT_MENU_TEST_TOOL();

    mgr.SetEnvironment( nullptr, nullptr, nullptr, nullptr, nullptr );
    mgr.RegisterTool( tool );
    mgr.ResetTools( TOOL_BASE::RUN );

    YIELDING_EVENT_LOOP  loop;
    wxEventLoopActivator activate( &loop );

    mgr.ProcessEvent( armMenuEvent() );
    BOOST_CHECK( !tool->m_menuRan );

    mgr.ProcessEvent( TOOL_EVENT( TC_MOUSE, TA_MOUSE_CLICK, BUT_RIGHT ) );
    BOOST_CHECK( tool->m_menuRan );
}

BOOST_AUTO_TEST_SUITE_END()

namespace
{
TOOL_EVENT armButtonMenuEvent()
{
    return TOOL_EVENT( TC_COMMAND, TA_ACTION, std::string( "test.buttonMenuTool.arm" ), AS_GLOBAL );
}


/// A tool like a drawing command: its menu opens on a right-click, and stays armed for the next one
class BUTTON_MENU_TEST_TOOL : public TOOL_INTERACTIVE
{
public:
    BUTTON_MENU_TEST_TOOL() :
            TOOL_INTERACTIVE( "test.buttonMenuTool" ),
            m_menuClosedCount( 0 ),
            m_menu( true )
    {
    }

    void Reset( RESET_REASON aReason [[maybe_unused]] ) override {}

    int Arm( const TOOL_EVENT& aEvent [[maybe_unused]] )
    {
        Activate();
        SetContextMenu( &m_menu, CMENU_BUTTON );

        while( TOOL_EVENT* evt = Wait() )
        {
            if( evt->Category() == TC_COMMAND && evt->Action() == TA_CHOICE_MENU_CLOSED )
                ++m_menuClosedCount;
        }

        return 0;
    }

    int m_menuClosedCount;

private:
    void setTransitions() override { Go( &BUTTON_MENU_TEST_TOOL::Arm, armButtonMenuEvent() ); }

    ACTION_MENU m_menu;
};
} // namespace


BOOST_AUTO_TEST_SUITE( RightClickCancels )

BOOST_AUTO_TEST_CASE( ARightClickCancelsOnlyWithTheSettingOnAndACommandRunning )
{
    // setting, selection tool is the current tool, cancels
    BOOST_CHECK( TOOL_DISPATCHER::DoesRightClickCancel( true, false ) );
    BOOST_CHECK( !TOOL_DISPATCHER::DoesRightClickCancel( true, true ) );
    BOOST_CHECK( !TOOL_DISPATCHER::DoesRightClickCancel( false, false ) );
    BOOST_CHECK( !TOOL_DISPATCHER::DoesRightClickCancel( false, true ) );
}

BOOST_AUTO_TEST_CASE( TheCancelIsCancelInteractiveAndNotEsc )
{
    TOOL_EVENT cancel = TOOL_DISPATCHER::MakeRightClickEvent( true, BUT_RIGHT );

    BOOST_CHECK( cancel.IsCancel() );
    BOOST_CHECK( cancel.IsCancelInteractive() );
    BOOST_CHECK( cancel.IsCancelFromRightClick() );
    BOOST_CHECK( !cancel.IsClick( BUT_RIGHT ) );

    // The Esc key's event is the same kind of cancel, told apart by what made it
    TOOL_EVENT escape( TC_COMMAND, TA_CANCEL_TOOL, WXK_ESCAPE );

    BOOST_CHECK( escape.IsCancelInteractive() );
    BOOST_CHECK( !escape.IsCancelFromRightClick() );
}

BOOST_AUTO_TEST_CASE( WithTheSettingOffTheRightClickIsTheClickItAlwaysWas )
{
    TOOL_EVENT click = TOOL_DISPATCHER::MakeRightClickEvent( false, BUT_RIGHT );

    BOOST_CHECK( click.IsClick( BUT_RIGHT ) );
    BOOST_CHECK( !click.IsCancel() );
    BOOST_CHECK( !click.IsCancelInteractive() );
    BOOST_CHECK( !click.IsCancelFromRightClick() );
}

BOOST_AUTO_TEST_CASE( TheSettingFollowsTheBuildsProfile )
{
    COMMON_SETTINGS settings;

    settings.Load();

#ifdef KICAD_HOUSE_INTERACTION
    BOOST_CHECK( settings.m_Input.right_click_cancels );
#else
    BOOST_CHECK( !settings.m_Input.right_click_cancels );
#endif

    // Round trip, and the name the note gives it
    settings.m_Input.right_click_cancels = !settings.m_Input.right_click_cancels;

    bool expected = settings.m_Input.right_click_cancels;

    settings.Store();
    settings.m_Input.right_click_cancels = !expected;
    settings.Load();
    BOOST_CHECK_EQUAL( settings.m_Input.right_click_cancels, expected );
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE( ShowToolContextMenu )

BOOST_AUTO_TEST_CASE( TheRequestOpensTheMenuOfTheRunningToolAndOnlyThatEvent )
{
    if( !KI_TEST::CanDoDisplayTests() )
    {
        BOOST_TEST_MESSAGE( "Skipping test - no display available" );
        return;
    }

    TOOL_MANAGER           mgr;
    BUTTON_MENU_TEST_TOOL* tool = new BUTTON_MENU_TEST_TOOL();

    mgr.SetEnvironment( nullptr, nullptr, nullptr, nullptr, nullptr );
    mgr.RegisterTool( tool );
    mgr.ResetTools( TOOL_BASE::RUN );

    mgr.ProcessEvent( armButtonMenuEvent() );
    BOOST_CHECK_EQUAL( tool->m_menuClosedCount, 0 );

    // Any other event leaves a menu that waits for the right button alone
    mgr.ProcessEvent( TOOL_EVENT( TC_MOUSE, TA_MOUSE_MOTION, 0 ) );
    BOOST_CHECK_EQUAL( tool->m_menuClosedCount, 0 );

    // The key's action opens it now, whatever the trigger
    mgr.ProcessEvent( ACTIONS::showToolContextMenu.MakeEvent() );
    BOOST_CHECK_EQUAL( tool->m_menuClosedCount, 1 );

    // ... and leaves it as it was: a right-click opens it again, and the request does too
    mgr.ProcessEvent( TOOL_EVENT( TC_MOUSE, TA_MOUSE_CLICK, BUT_RIGHT ) );
    BOOST_CHECK_EQUAL( tool->m_menuClosedCount, 2 );

    mgr.ProcessEvent( ACTIONS::showToolContextMenu.MakeEvent() );
    BOOST_CHECK_EQUAL( tool->m_menuClosedCount, 3 );
}

BOOST_AUTO_TEST_SUITE_END()

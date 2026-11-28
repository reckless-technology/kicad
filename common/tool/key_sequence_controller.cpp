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

#include <tool/key_sequence_controller.h>

#include <kiplatform/ui.h>
#include <pgm_base.h>
#include <settings/common_settings.h>
#include <tool/action_manager.h>
#include <tool/action_menu.h>
#include <tool/tool_action.h>
#include <tool/tool_event.h>
#include <tool/tool_interactive.h>
#include <tool/tool_manager.h>
#include <tool/tools_holder.h>
#include <trace_helpers.h>

#include <memory>

#include <menu_def.h>

#include <wx/log.h>
#include <wx/menu.h>
#include <wx/window.h>


KEY_SEQUENCE_CONTROLLER::KEY_SEQUENCE_CONTROLLER( TOOL_MANAGER* aToolManager ) :
        m_toolManager( aToolManager )
{
    m_timer.SetOwner( this );
    Bind( wxEVT_TIMER, &KEY_SEQUENCE_CONTROLLER::onTimer, this );
}


KEY_SEQUENCE_CONTROLLER::~KEY_SEQUENCE_CONTROLLER()
{
    m_timer.Stop();
}


int KEY_SEQUENCE_CONTROLLER::HintDelayMilliseconds()
{
    COMMON_SETTINGS* settings = Pgm().GetCommonSettings();

    return settings ? settings->m_Input.key_sequence_hint_delay_ms : 300;
}


bool KEY_SEQUENCE_CONTROLLER::Begin( const TOOL_ACTION& aRoot )
{
    TOOLS_HOLDER*     holder = m_toolManager->GetToolHolder();
    KEY_SEQUENCE_SPEC spec;

    if( !holder || !holder->GetKeySequenceSpec( spec ) )
        return false;

    wxUniChar mnemonic = 0;

    for( const auto& [action, letter] : spec.m_roots )
    {
        if( action == &aRoot )
            mnemonic = letter;
    }

    // The root's letter is the one of the menu it opens
    m_engine.SetTree( spec.m_tree );

    if( !mnemonic || m_engine.Enter( mnemonic ).m_outcome != KEY_SEQUENCE_RESULT::OUTCOME::CONSUMED )
    {
        afterEngineChange();
        return false;
    }

    wxLogTrace( kicadTraceKeySequence, wxS( "root %s opened %s" ), aRoot.GetName(), m_engine.KeysTyped() );
    afterEngineChange();
    return true;
}


void KEY_SEQUENCE_CONTROLLER::Cancel()
{
    m_engine.Clear();
    afterEngineChange();
}


bool KEY_SEQUENCE_CONTROLLER::Handle( const TOOL_EVENT& aEvent )
{
    using OUTCOME = KEY_SEQUENCE_RESULT::OUTCOME;

    if( !m_engine.IsPending() )
        return false;

    KEY_SEQUENCE_RESULT result;

    if( aEvent.IsKeyPressed() )
    {
        bool hasCommandModifier = aEvent.Modifier( MD_CTRL | MD_ALT | MD_SUPER | MD_META | MD_ALTGR ) != 0;

        result = m_engine.HandleKey( aEvent.KeyCode(), hasCommandModifier );
    }
    else if( aEvent.IsCancel() && aEvent.GetCommandId() && *aEvent.GetCommandId() == WXK_ESCAPE ) // the Esc key
    {
        result = m_engine.HandleKey( WXK_ESCAPE, false );
    }
    else if( aEvent.IsCancelFromRightClick() )
    {
        // A right-click that cancels abandons the sequence, as Esc does, and cancels nothing else
        m_engine.Clear();
        afterEngineChange();
        return true;
    }
    else if( aEvent.Category() == TC_MOUSE && aEvent.Action() != TA_MOUSE_MOTION && aEvent.Action() != TA_MOUSE_WHEEL )
    {
        // A click abandons the sequence, and goes on to its usual meaning
        m_engine.Clear();
        afterEngineChange();
        return false;
    }
    else
    {
        return false;
    }

    // Leave the status bar as it was before the action runs: a tool the action starts shows its own name there
    afterEngineChange();

    switch( result.m_outcome )
    {
    case OUTCOME::RUN_ACTION:
        wxLogTrace( kicadTraceKeySequence, wxS( "key %d runs %s" ), aEvent.KeyCode(), result.m_action->GetName() );
        m_toolManager->GetActionManager()->RunActionAsHotKey( *result.m_action );
        return true;

    case OUTCOME::CONSUMED: return true;

    case OUTCOME::SHOW_NODE:
    case OUTCOME::NOT_MINE:
        wxLogTrace( kicadTraceKeySequence, wxS( "key %d is not part of the sequence" ), aEvent.KeyCode() );
        return false;
    }

    return false;
}


void KEY_SEQUENCE_CONTROLLER::afterEngineChange()
{
    m_timer.Stop();

    if( !m_engine.IsPending() )
    {
        restoreStatus();
        return;
    }

    showStatus();

    int delay = HintDelayMilliseconds();

    m_timerToken = m_engine.Token();

    if( delay <= 0 )
    {
        // At once, but not from inside the key's own dispatch: the menu is a modal loop
        unsigned token = m_timerToken;

        CallAfter(
                [this, token]()
                {
                    onHintDue( token );
                } );
    }
    else
    {
        m_timer.StartOnce( delay );
    }
}


void KEY_SEQUENCE_CONTROLLER::onTimer( wxTimerEvent& )
{
    onHintDue( m_timerToken );
}


void KEY_SEQUENCE_CONTROLLER::onHintDue( unsigned aToken )
{
    KEY_SEQUENCE_RESULT result = m_engine.HintTimerFired( aToken );

    if( result.m_outcome != KEY_SEQUENCE_RESULT::OUTCOME::SHOW_NODE )
        return;

    wxLogTrace( kicadTraceKeySequence, wxS( "hint shown for %s" ), result.m_node->m_label );
    restoreStatus();
    showHint( *result.m_node );
}


void KEY_SEQUENCE_CONTROLLER::showStatus()
{
    if( TOOLS_HOLDER* holder = m_toolManager->GetToolHolder() )
    {
        holder->DisplayToolMsg( m_engine.KeysTyped() );
        m_isStatusShown = true;
    }
}


void KEY_SEQUENCE_CONTROLLER::restoreStatus()
{
    if( !m_isStatusShown )
        return;

    m_isStatusShown = false;

    if( TOOLS_HOLDER* holder = m_toolManager->GetToolHolder() )
        holder->RestoreToolMsg();
}


void KEY_SEQUENCE_CONTROLLER::showHint( const MENU_DEF& aNode )
{
    TOOLS_HOLDER*     holder = m_toolManager->GetToolHolder();
    wxWindow*         frame = dynamic_cast<wxWindow*>( holder );
    TOOL_INTERACTIVE* tool = dynamic_cast<TOOL_INTERACTIVE*>( m_toolManager->FindTool( "common.Control" ) );

    if( !frame || !tool )
        return;

    // The same builder as the menu bar's makes the menu, so its mnemonics are the sequence's keys. On macOS the letters
    // are shown beside the entries, because its popups have no mnemonics.
    MENU_DEF hinted = aNode;

#ifdef __WXMAC__
    hinted = MENU_DEF::WithLetterHints( aNode );
#endif

    std::unique_ptr<ACTION_MENU> menu( BuildActionMenu( hinted, tool ) );

    frame->PopupMenu( menu.get(), frame->ScreenToClient( KIPLATFORM::UI::GetMousePosition() ) );
}

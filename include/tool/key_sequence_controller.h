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

#ifndef KEY_SEQUENCE_CONTROLLER_H
#define KEY_SEQUENCE_CONTROLLER_H

#include <key_sequence.h>

#include <wx/event.h>
#include <wx/timer.h>

class TOOL_ACTION;
class TOOL_EVENT;
class TOOL_MANAGER;


/**
 * Runs the key sequences of one tool manager (interaction-profile note, 5.3): the state machine, the hint timer, the
 * keys shown in the status bar and the hint popup.
 *
 * The tool manager makes one the first time a root action runs, so with the stock profile (no root has a key) none
 * exists and key dispatch is the stock path.
 */
class KEY_SEQUENCE_CONTROLLER : public wxEvtHandler
{
public:
    explicit KEY_SEQUENCE_CONTROLLER( TOOL_MANAGER* aToolManager );
    ~KEY_SEQUENCE_CONTROLLER() override;

    /// A root action ran. False when the frame has no sequence for it, so the key it came from is not consumed.
    bool Begin( const TOOL_ACTION& aRoot );

    bool IsPending() const { return m_engine.IsPending(); }

    /**
     * Offer an event to a pending sequence: a key press, `Esc`, or a mouse button (which abandons the sequence).
     *
     * @return true when the event is consumed.
     */
    bool Handle( const TOOL_EVENT& aEvent );

    /// Abandon a pending sequence.
    void Cancel();

    /// The engine, for the tests of the whole path
    KEY_SEQUENCE& Engine() { return m_engine; }

    /// The hint delay in force, in milliseconds: the setting, which is 0 for "at once".
    static int HintDelayMilliseconds();

private:
    void onTimer( wxTimerEvent& aEvent );
    void onHintDue( unsigned aToken );
    void afterEngineChange();
    void showStatus();
    void restoreStatus();
    void showHint( const MENU_DEF& aNode );

    TOOL_MANAGER* m_toolManager;
    KEY_SEQUENCE  m_engine;
    wxTimer       m_timer;
    unsigned      m_timerToken = 0;
    bool          m_isStatusShown = false;
};

#endif // KEY_SEQUENCE_CONTROLLER_H

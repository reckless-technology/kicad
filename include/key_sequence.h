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

#ifndef KEY_SEQUENCE_H
#define KEY_SEQUENCE_H

#include <memory>
#include <utility>
#include <vector>

#include <wx/string.h>

#include <menu_def.h>

class TOOL_ACTION;


/**
 * The result of giving the key-sequence engine a key or a timer.
 */
struct KEY_SEQUENCE_RESULT
{
    enum class OUTCOME
    {
        NOT_MINE,   ///< not the engine's: dispatch the key as usual (the engine is idle again)
        CONSUMED,   ///< the engine took it; nothing to run yet
        RUN_ACTION, ///< a sequence is complete: run m_action (the engine is idle again)
        SHOW_NODE   ///< the hint timer fired: show m_node's menu (the engine is idle again, the menu takes over)
    };

    OUTCOME            m_outcome = OUTCOME::NOT_MINE;
    const TOOL_ACTION* m_action = nullptr; ///< RUN_ACTION
    const MENU_DEF*    m_node = nullptr;   ///< SHOW_NODE: valid until the next call that changes the tree
};


/**
 * A frame's key sequences: the tree they are read from and which action opens which menu of it.
 *
 * The tree is the frame's Altium-style menu bar (key-only entries included), so the menu bar and the sequences cannot
 * drift apart: a sequence is a path of mnemonics through the tree.
 */
struct KEY_SEQUENCE_SPEC
{
    std::shared_ptr<const MENU_DEF> m_tree;

    /// Each root action and the mnemonic of the menu it opens, resolved by KEY_SEQUENCE::FindRoot()
    std::vector<std::pair<const TOOL_ACTION*, wxUniChar>> m_roots;
};


/**
 * The state machine behind the Altium-style key sequences (interaction-profile note, 5.3).
 *
 * A root action enters the pending state at a menu of the tree. The next key selects a child of that menu by its
 * mnemonic: a leaf's action runs and the state clears, a submenu becomes the new menu. Escape clears the state, and any
 * other key clears it and is passed on, so a mistyped sequence costs nothing. Modifier-only keys are ignored. Because
 * the keys are looked up here before hotkeys, a key that is both a child's mnemonic and somebody's hotkey goes to the
 * child while a sequence is pending.
 *
 * The engine knows no wx widgets and no timer: the owner starts a timer for the token it reports after each call that
 * leaves a sequence pending, and gives it back to HintTimerFired(). A fast typist completes the sequence first, which
 * changes the token, so the late timer shows nothing. It is tested without a GUI.
 */
class KEY_SEQUENCE
{
public:
    using RESULT = KEY_SEQUENCE_RESULT;

    /**
     * The menu a sequence's first key opens: a top-level menu with that mnemonic, else the first submenu of a top-level
     * menu that has it (Edit's Move, Select and Align are roots of their own, as in Altium).
     *
     * @return null when there is none.
     */
    static const MENU_DEF* FindRoot( const MENU_DEF& aBar, wxUniChar aMnemonic );

    /// The available child of \a aMenu that \a aMnemonic selects, or null. Separators and slots have none.
    static const MENU_DEF* FindChild( const MENU_DEF& aMenu, wxUniChar aMnemonic );

    /// The entry a whole sequence selects, one mnemonic per key, or null (for tests and for tools that list sequences).
    static const MENU_DEF* Resolve( const MENU_DEF& aBar, const std::vector<wxUniChar>& aKeys );

    /// Whether \a aKeyCode (a wx key code) is a modifier by itself
    static bool IsModifierOnly( int aKeyCode );

    /// Use \a aTree for the sequences that follow. Clears a pending one.
    void SetTree( std::shared_ptr<const MENU_DEF> aTree );

    /// A root action ran: enter the pending state at the menu that \a aRootMnemonic opens. NOT_MINE if it opens none.
    RESULT Enter( wxUniChar aRootMnemonic );

    /**
     * The next key of a sequence.
     *
     * @param aKeyCode a wx key code; letters in either case.
     * @param aHasCommandModifier whether Ctrl, Alt or Meta is down: such a key is never a mnemonic. Shift is allowed,
     *        because a typist whose Shift is still down must not lose the sequence.
     */
    RESULT HandleKey( int aKeyCode, bool aHasCommandModifier );

    /// The hint timer for \a aToken fired: SHOW_NODE if that is still the state, else NOT_MINE and nothing happens.
    RESULT HintTimerFired( unsigned aToken );

    bool IsPending() const { return m_node != nullptr; }

    /// Changes at every key that moves a pending sequence on, so that a timer started earlier can be told apart.
    unsigned Token() const { return m_token; }

    /// The keys typed so far, as the status bar shows them: "P V ›". Empty when nothing is pending.
    wxString KeysTyped() const;

    /// Forget a pending sequence.
    void Clear();

private:
    std::shared_ptr<const MENU_DEF> m_tree;
    const MENU_DEF*                 m_node = nullptr;
    std::vector<wxUniChar>          m_typed;
    unsigned                        m_token = 0;
};

#endif // KEY_SEQUENCE_H

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

#ifndef MENU_DEF_H
#define MENU_DEF_H

#include <functional>
#include <initializer_list>
#include <list>
#include <string>
#include <vector>

// wxArrayString is a std::vector<wxString> exported from the wx DLL: without this, MSVC instantiates a second copy
#include <wx/arrstr.h>
#include <wx/string.h>

#include <tool/selection_conditions.h>

class ACTION_MENU;
class CONDITIONAL_MENU;
class TOOL_ACTION;
class TOOL_INTERACTIVE;
class WX_MENUBAR;


/**
 * One node of a declarative menu tree (interaction-profile note, 5.4).
 *
 * A node is a leaf (an action), a submenu (children), a separator, or a slot: a place where the frame puts what only
 * it can make at run time (the recent files, the language list). The tree holds no wx widgets, so the menu bar and
 * the key-sequence engine can both read it, and a test can walk it without a window.
 *
 * The label carries the `&` mnemonic: the letter after it is the key that selects the entry while the menu is open,
 * and the second key of a sequence in the key-sequence engine. Mnemonics are unique within a menu.
 */
struct MENU_DEF
{
    wxString              m_label;                                   ///< translated, with the `&` mnemonic
    const TOOL_ACTION*    m_action = nullptr;                        ///< a leaf's action
    std::vector<MENU_DEF> m_children;                                ///< a submenu's entries
    SELECTION_CONDITION   m_show = SELECTION_CONDITIONS::ShowAlways; ///< when the entry is shown (context menus)
    bool                  m_isSeparator = false;
    bool                  m_isCheck = false; ///< a leaf shown with a check mark
    std::string           m_slot;            ///< a slot's name: the frame fills it in when the menu is built

    /// Empty for an entry that is always there. Otherwise it is asked when the menu is built, and a false answer leaves
    /// the entry out (an advanced setting that is off, a program that runs on its own). The tree still holds it.
    std::function<bool()> m_isAvailable;

    /// A leaf that runs \a aAction.
    static MENU_DEF Item( const wxString& aLabel, const TOOL_ACTION& aAction );

    /// A leaf that runs \a aAction and is shown with a check mark.
    static MENU_DEF CheckItem( const wxString& aLabel, const TOOL_ACTION& aAction );

    /// A leaf shown only while \a aCondition holds.
    static MENU_DEF Item( const wxString& aLabel, const TOOL_ACTION& aAction, const SELECTION_CONDITION& aCondition );

    /// A submenu of \a aChildren.
    static MENU_DEF Submenu( const wxString& aLabel, std::initializer_list<MENU_DEF> aChildren );

    /// A submenu shown only while \a aCondition holds.
    static MENU_DEF Submenu( const wxString& aLabel, const SELECTION_CONDITION& aCondition,
                             std::initializer_list<MENU_DEF> aChildren );

    static MENU_DEF Separator();

    /// A named place the frame fills in. \a aLabel is the title of the submenu a frame may put there.
    static MENU_DEF Slot( const wxString& aLabel, const std::string& aSlot );

    /// A copy of this node that is built only when \a aPredicate answers true.
    MENU_DEF When( const std::function<bool()>& aPredicate ) const;

    bool IsAvailable() const { return !m_isAvailable || m_isAvailable(); }
    bool IsSeparator() const { return m_isSeparator; }
    bool IsSubmenu() const { return !m_isSeparator && !m_action && m_slot.empty(); }
    bool IsSlot() const { return !m_slot.empty(); }
    bool IsAction() const { return m_action != nullptr; }

    /**
     * The mnemonic of the label, upper case: the letter after the first `&` that is not part of `&&`.
     *
     * @return 0 when the label has none.
     */
    wxUniChar Mnemonic() const;

    /// Visit every node, parents before children, with the labels of the submenus above it, outermost first.
    void Walk( const std::function<void( const MENU_DEF& aNode, const std::vector<wxString>& aPath )>& aVisitor ) const;

    /// Every action in the tree, in menu order. An action in two places is listed twice.
    std::vector<const TOOL_ACTION*> Actions() const;

    /// Whether the tree has an entry for \a aAction.
    bool Contains( const TOOL_ACTION& aAction ) const;

    /// Every slot name in the tree.
    std::vector<std::string> Slots() const;

    /**
     * One message for each pair of entries that share a mnemonic within a menu, at every level.
     *
     * The message names the menu and both entries. A menu bar's top-level entries are one menu.
     */
    std::vector<wxString> DuplicateMnemonics() const;

    /// One message for each entry that is not a separator, a slot or a submenu and has no mnemonic.
    std::vector<wxString> EntriesWithoutMnemonic() const;

    /**
     * One message for each leaf whose action is null, has no name, or is not in \a aRegistered, and for each empty
     * submenu. A message names the action.
     */
    std::vector<wxString> UnresolvedActions( const std::list<TOOL_ACTION*>& aRegistered ) const;
};


/// A tree entry left out on purpose, so a test can tell a decision from a loss.
struct MENU_OMISSION
{
    const char* m_sequence;   ///< the interaction-profile note's key sequence, or what the entry is
    const char* m_actionName; ///< the registered name of an action that exists but has no entry, else nullptr
    const char* m_reason;
};


/**
 * Fills a slot: adds what the frame makes at run time to \a aParent, in the place the slot has in the tree.
 *
 * It may add nothing.
 */
using MENU_SLOT_FILLER = std::function<void( const std::string& aSlot, const MENU_DEF& aNode, ACTION_MENU& aParent )>;


/**
 * Build the menu for a submenu node: its label is the title, its children the entries. Conditions are ignored, because
 * a menu bar has no selection to test (BuildConditionalMenu() keeps them). The caller owns the menu.
 */
ACTION_MENU* BuildActionMenu( const MENU_DEF& aSubmenu, TOOL_INTERACTIVE* aTool,
                              const MENU_SLOT_FILLER& aFillSlot = MENU_SLOT_FILLER() );

/**
 * Build a menu whose entries come and go with the selection, for a submenu node: each node's condition is the
 * entry's condition (CONDITIONAL_MENU::Evaluate() applies them). Its entries carry their actions' own labels, as a
 * CONDITIONAL_MENU's always do, and its submenus the labels of the tree. The caller owns the menu.
 */
CONDITIONAL_MENU* BuildConditionalMenu( const MENU_DEF& aSubmenu, TOOL_INTERACTIVE* aTool );

/**
 * Build a frame's menu bar: each child of \a aBar is a menu, titled with its label. The frame appends its help menu.
 * The caller owns the bar and gives it to the frame.
 */
WX_MENUBAR* BuildMenuBar( const MENU_DEF& aBar, TOOL_INTERACTIVE* aTool,
                          const MENU_SLOT_FILLER& aFillSlot = MENU_SLOT_FILLER() );

#endif // MENU_DEF_H

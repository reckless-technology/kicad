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

#ifndef HOTKEY_STORE__H
#define HOTKEY_STORE__H

#include <hotkeys_basic.h>
#include <tool/tool_action.h>
#include <vector>

class TOOL_MANAGER;


struct HOTKEY
{
    std::vector<TOOL_ACTION*> m_Actions;
    int                       m_EditKeycode;
    int                       m_EditKeycodeAlt;

    HOTKEY() :
            m_EditKeycode( 0 ),
            m_EditKeycodeAlt( 0 )
    { }

    HOTKEY( TOOL_ACTION* aAction ) :
            m_EditKeycode( aAction->GetHotKey() ),
            m_EditKeycodeAlt( aAction->GetHotKeyAlt() )
    {
        m_Actions.push_back( aAction );
    }
};


struct HOTKEY_SECTION
{
    wxString            m_SectionName;   // The displayed, translated, name of the section
    std::vector<HOTKEY> m_HotKeys;
};


/**
 * A class that contains a set of hotkeys, arranged into "sections"
 * and provides some book-keeping functions for them.
 */
class HOTKEY_STORE
{
public:

    /**
     * Construct a HOTKEY_STORE from a list of hotkey sections
     */
    HOTKEY_STORE();

    void Init( std::vector<TOOL_ACTION*> aActionsList, bool aIncludeReadOnlyCmds );

    /**
     * Make a profile the one the store edits: every hotkey becomes the profile's default, then what \a aUserKeys
     * (that profile's user hotkey file) says. Only the store changes, not the actions.
     */
    void SetProfile( INTERACTION_PROFILE_ID aProfile, const PROFILE_HOTKEY_MAP& aUserKeys );

    INTERACTION_PROFILE_ID GetProfile() const { return m_profile; }

    static wxString GetAppName( TOOL_ACTION* aAction );
    static wxString GetSectionName( TOOL_ACTION* aAction );

    /**
     * Get the list of sections managed by this store
     */
    std::vector<HOTKEY_SECTION>& GetSections();

    /**
     * Persist all changes to hotkeys in the store to the underlying
     * data structures.
     */
    void SaveAllHotkeys();

    /**
     * Reset every hotkey in the store to the default values of the store's profile (the active one unless
     * SetProfile() chose another).
     */
    void ResetAllHotkeysToDefault();

    /**
     * Reset every hotkey in the store to the default values of a profile: the compiled defaults under the
     * profile's table, and make it the store's profile. Conflict checks then run on this resolved set.
     */
    void ResetAllHotkeysToDefault( INTERACTION_PROFILE_ID aProfile );

    /**
     * Resets every hotkey to the original values.
     */
    void ResetAllHotkeysToOriginal();

    /**
     * Check whether the given key conflicts with anything in this store.
     *
     * @param aAction - the action the key is proposed to be assigned to.  Only conflicts
     *                  within the same section will be flagged.
     * @param aKey - key to check
     * @param aConflict - outparam getting the section this one conflicts with
     */
    bool CheckKeyConflicts( TOOL_ACTION* aAction, long aKey, HOTKEY** aConflict );

private:
    std::vector<TOOL_MANAGER*>  m_toolManagers;
    std::vector<HOTKEY_SECTION> m_hk_sections;
    INTERACTION_PROFILE_ID      m_profile = INTERACTION_PROFILE_ID::STOCK;
};

#endif // HOTKEY_STORE__H

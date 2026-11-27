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

#ifndef INTERACTION_PROFILE_H
#define INTERACTION_PROFILE_H

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <wx/string.h>

class TOOL_ACTION;

/**
 * The interaction profiles: named ways of reaching KiCad's functions.
 *
 * STOCK is stock KiCad (setting value `kicad`) and changes nothing. ALTIUM maps the keys (and, in later steps, menus and views) of an Altium
 * user's finger memory onto KiCad's functions. The values are stored as numbers in `input.interaction_profile`, so
 * new profiles are appended.
 */
enum class INTERACTION_PROFILE_ID
{
    STOCK = 0, ///< named STOCK because KICAD is a macro in some programs
    ALTIUM = 1
};


/// Action name to primary and alternate key, the shape of a hotkey file.
using PROFILE_HOTKEY_MAP = std::map<std::string, std::pair<int, int>>;


/**
 * The profile's contribution to hotkey resolution, built once per update from a profile's table.
 *
 * An inactive layer (the KICAD profile) changes nothing, so resolution is the stock one.
 */
struct PROFILE_HOTKEY_LAYER
{
    bool               m_isActive = false;
    PROFILE_HOTKEY_MAP m_bindings;  ///< what the profile table binds, by action name
    std::set<int>      m_takenKeys; ///< every key the table binds, in any editor

    /// The keys the table binds, by the scope of the action that binds them: the action name up to and including its
    /// first dot ("common.", "eeschema.", "pcbnew."). A `common.` action is in every editor, so its key is taken there;
    /// an editor's own action takes its key in that editor only.
    std::map<std::string, std::set<int>> m_takenKeysByScope;
};


/**
 * The profile switch and the profile layer of hotkey resolution (interaction-profile note, 5.1 and 5.2).
 *
 * The active profile lives in COMMON_SETTINGS, reached through Pgm(). This class is compiled into the static
 * `common` library, so it keeps no state of its own: a static here would be one copy per module (each kiface and
 * executable), and the copies would disagree about the profile.
 */
class INTERACTION_PROFILE
{
public:
    /// The profile in force: `input.interaction_profile` of the common settings, KICAD when there are none yet.
    static INTERACTION_PROFILE_ID Active();

    /**
     * Select a profile in the common settings. Nothing is saved or announced: the caller saves the settings and
     * sends KIWAY::CommonSettingsChanged( HOTKEYS_CHANGED | INTERACTION_PROFILE_CHANGED ) so every open frame
     * re-applies it.
     */
    static void SetActive( INTERACTION_PROFILE_ID aProfile );

    /// The profile's name as shown in Preferences.
    static wxString DisplayName( INTERACTION_PROFILE_ID aProfile );

    /// Every profile, in the order the selector lists them.
    static std::vector<INTERACTION_PROFILE_ID> AllProfiles();

    /// The user's hotkey file without its directory: `user.hotkeys` for KICAD, `user.altium.hotkeys` for ALTIUM.
    static wxString UserHotKeyFileName( INTERACTION_PROFILE_ID aProfile );

    /// The user's hotkey file for a profile, in \a aDirectory or, when that is empty, the user settings directory.
    static wxString UserHotKeyFilePath( INTERACTION_PROFILE_ID aProfile, const wxString& aDirectory = wxEmptyString );

    /// The profile's embedded hotkey table, in the `user.hotkeys` format. Empty for KICAD.
    static std::string HotKeyTableText( INTERACTION_PROFILE_ID aProfile );

    /// Read a table in the `user.hotkeys` format, ignoring blank lines and lines that start with `#`.
    static void ParseHotKeyTable( const std::string& aText, PROFILE_HOTKEY_MAP& aBindings );

    /// A profile table's key names in this platform's spelling. The tables say `Ctrl+` and `Alt+`; on macOS the hotkey
    /// file says `Cmd+` and `Option+`, and KeyCodeFromKeyName() reads no `Alt+` there.
    static std::string PlatformKeyNames( const std::string& aTableText );

    /// A profile table's key names as macOS spells them (`Ctrl+` as `Cmd+`, `Alt+` as `Option+`), on any platform.
    static std::string MacKeyNames( const std::string& aTableText );

    /// The layer of a profile, from its embedded table.
    static PROFILE_HOTKEY_LAYER BuildHotKeyLayer( INTERACTION_PROFILE_ID aProfile );

    /// The layer made from a table given as text, for tests and for tools that read a table from elsewhere.
    static PROFILE_HOTKEY_LAYER BuildHotKeyLayerFromText( const std::string& aTableText );

    /**
     * The profile's defaults for one action: the layering that follows the compiled (or legacy) keys.
     *
     * The table's binding wins. An action the table does not name keeps \a aBaseKeys, except that a key the table
     * binds elsewhere in its scope is removed from it (0 in that slot), so the profile's keys never fight a stock
     * default. The scopes of a key are the action's: a key bound by a `common.` action is taken in every editor, and a
     * key bound by an `eeschema.` or `pcbnew.` action in that editor only. An action of an editor loses the keys of
     * `common.` and of its own editor; a `common.` action, which is in every editor, loses the keys of `common.` and of
     * the editor it is resolved for (\a aEditorScope, as "eeschema."), or those of every editor when that is empty
     * (a hotkey editor that lists the actions of several).
     *
     * An action of the active tool (\a aIsContextAction) keeps its key where a root takes it, because inside its command
     * it wins: Altium's rule that in-command keys win, which is why the roots are real actions (note, 5.3). Only a table
     * entry for the action itself changes it.
     *
     * An inactive layer returns \a aBaseKeys.
     */
    static std::pair<int, int> ApplyHotKeyLayer( const PROFILE_HOTKEY_LAYER& aLayer, const std::string& aActionName,
                                                 std::pair<int, int> aBaseKeys,
                                                 const std::string&  aEditorScope = std::string(),
                                                 bool                aIsContextAction = false );

    /// The scope of an action name: up to and including its first dot ("common."), or empty for a name with none.
    static std::string ScopeOf( const std::string& aActionName );

    /**
     * The action names of a layer's table that are in none of \a aActions.
     *
     * Only names that begin with one of \a aNamePrefixes are checked ("common.", "eeschema."), because each QA
     * executable links only its own editor's actions.
     */
    static std::vector<std::string> UnregisteredActionNames( const PROFILE_HOTKEY_LAYER&      aLayer,
                                                             const std::vector<TOOL_ACTION*>& aActions,
                                                             const std::vector<std::string>&  aNamePrefixes );
};

#endif // INTERACTION_PROFILE_H

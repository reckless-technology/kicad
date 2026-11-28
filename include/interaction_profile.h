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
class APP_SETTINGS_BASE;
class COMMON_SETTINGS;

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


/// What a mouse drag in the 3D viewer does to the camera.
enum class CAMERA_DRAG_MODE
{
    NONE,  ///< the drag does nothing
    ORBIT, ///< the drag rotates the board
    PAN    ///< the drag moves the board in the view
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

    /**
     * Apply the Altium-style profile's mouse preset (interaction-profile note, 5.6) to \a aSettings the first time the
     * profile is in effect, and never again: the wheel scrolls vertically, Shift+wheel horizontally, Ctrl+wheel zooms
     * (KiCad's trackpad defaults), right-drag pans and middle-drag zooms. What the user changes afterwards is theirs.
     *
     * The caller says so to the user and saves the settings.
     *
     * @return true when the preset was applied by this call; false in the stock profile, and when it was applied before.
     */
    static bool ApplyMousePresetOnce( COMMON_SETTINGS& aSettings );

    /**
     * Make the Altium-style profile's color theme (Altium classic) the theme of \a aSettings (one application's) the
     * first time the profile is in effect, and never again: what the user picks afterwards is theirs, and a switch to
     * the stock profile and back does not bring it back.
     *
     * The caller saves the settings and reloads the colors.
     *
     * @return true when the theme was set by this call; false in the stock profile, and when it was set before.
     */
    static bool ApplyColorThemeOnce( APP_SETTINGS_BASE& aSettings );

    /// How far one press of the mask level keys moves `appearance.hicontrast_dimming_factor`.
    static constexpr double DIMMING_FACTOR_STEP = 0.1;

    /// \a aDimmingFactor moved by \a aSteps of DIMMING_FACTOR_STEP, held between 0 and 1 and rounded to the step.
    static double SteppedDimmingFactor( double aDimmingFactor, int aSteps );

    /**
     * Whether a right-click cancels the running command, from `input.right_click_cancels` (-1 follows the profile, 0 is
     * off, 1 is on) and the profile in force.
     */
    static bool DoesRightClickCancel( int aSetting, INTERACTION_PROFILE_ID aProfile );

    /**
     * Whether a mouse drag in the schematic keeps the attached wires (the Drag command) rather than moving the items
     * alone (the Move command). \a aDragIsMove is the `input.drag_is_move` setting, which makes a plain drag a move.
     * Under the Altium-style profile holding Ctrl gives the other of the two; the stock profile ignores Ctrl here.
     */
    static bool ShouldDragWithWires( INTERACTION_PROFILE_ID aProfile, bool aDragIsMove, bool aIsCtrlHeld );

    /**
     * Whether a drag that starts on empty space or on an unselected item, with these modifiers, draws a selection
     * box that adds to the selection instead of replacing it. Stock KiCad adds with Ctrl or Shift; the Altium-style
     * profile gives Ctrl to Ctrl+drag (the other of move and drag) and adds with Shift alone. Alt never adds.
     */
    static bool IsBoxSelectionAdditive( INTERACTION_PROFILE_ID aProfile, bool aIsCtrlHeld, bool aIsShiftHeld,
                                        bool aIsAltHeld );

    /**
     * Whether Ctrl+drag, with Shift not held, is the profile's inverted move or drag rather than a selection
     * modifier. Only the Altium-style profile; stock KiCad treats Ctrl as an exclusive-or selection modifier.
     */
    static bool IsCtrlDragInversion( INTERACTION_PROFILE_ID aProfile, bool aIsCtrlHeld, bool aIsShiftHeld );

    /**
     * Whether the PCB editor refreshes an open 3D viewer as the board is edited. \a aSetting is
     * `pcb_display.live_3d_refresh_mode` (-1 follows the profile, 0 is off, 1 is on). While it follows the profile, the
     * Altium-style profile refreshes and the stock profile keeps what the older `pcb_display.live_3d_refresh` switch
     * says, \a aIsLegacySwitchOn, so a stock user's saved choice survives.
     */
    static bool IsLive3DRefreshOn( int aSetting, bool aIsLegacySwitchOn, INTERACTION_PROFILE_ID aProfile );

    /**
     * Whether closing the 3D viewer hides it, so that its camera is still there when it is shown again, rather than
     * destroying it. The Altium-style profile hides it when the user closes it (\a aCanVeto, the close event allows
     * a veto); the PCB editor closing, which forces the close, always destroys it, and so does the stock profile.
     */
    static bool ShouldHide3DViewerOnClose( INTERACTION_PROFILE_ID aProfile, bool aCanVeto );

    /**
     * Whether a key that the Altium-style profile gives to an idle editor (`3`, the 3D viewer) may run now. Under that
     * profile it runs only when no command is running (\a aIsToolStackEmpty), so a command that owns the key, or the
     * router while it has none of its own, is not interrupted. The stock profile has no such limit.
     */
    static bool MayRunIdleKey( INTERACTION_PROFILE_ID aProfile, bool aIsToolStackEmpty );

    /**
     * What a drag with these buttons held does to the 3D viewer's camera. The left button always orbits. The Altium-style
     * profile also orbits on Shift+right-drag; the right button without Shift, and the middle button, pan when their
     * setting (\a aDoesRightDragPan, \a aDoesMiddleDragPan) says so, as in the stock profile, which has no orbit on the
     * right button. Defined here, not in the static `common` library, because the 3D canvas that asks is in kicommon.
     */
    static CAMERA_DRAG_MODE Camera3DDragMode( INTERACTION_PROFILE_ID aProfile, bool aIsLeftDown, bool aIsMiddleDown,
                                              bool aIsRightDown, bool aIsShiftHeld, bool aDoesMiddleDragPan,
                                              bool aDoesRightDragPan )
    {
        if( aIsLeftDown )
            return CAMERA_DRAG_MODE::ORBIT;

        if( aProfile == INTERACTION_PROFILE_ID::ALTIUM && aIsRightDown && aIsShiftHeld )
            return CAMERA_DRAG_MODE::ORBIT;

        if( ( aIsMiddleDown && aDoesMiddleDragPan ) || ( aIsRightDown && aDoesRightDragPan ) )
            return CAMERA_DRAG_MODE::PAN;

        return CAMERA_DRAG_MODE::NONE;
    }

    /**
     * Whether the symbol chooser's "Place repeated copies" starts checked: under the Altium-style profile it does, so
     * that a part stays on the cursor for the next click.
     */
    static bool IsRepeatedPlacementDefault( INTERACTION_PROFILE_ID aProfile );

    /**
     * Whether a net label, global label or hierarchical label is attached without its dialog, as a copy of the
     * previous one of its kind with the text incremented: the Altium-style profile does that once there is a previous
     * label. The first label of a session, and every label under the stock profile, opens the dialog.
     */
    static bool IsLabelRepeated( INTERACTION_PROFILE_ID aProfile, bool aHasPreviousLabel );

    /**
     * The text of the next repeated label: \a aPreviousText with its trailing number moved by \a aIncrement
     * (`drawing.repeat_label_increment`), by the same rule as the Repeat Last Item command. Text with no number to
     * increment is repeated unchanged.
     */
    static wxString NextRepeatedLabelText( const wxString& aPreviousText, int aIncrement );

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

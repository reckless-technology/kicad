/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

#ifndef  HOTKEYS_BASIC_H
#define  HOTKEYS_BASIC_H

#include <interaction_profile.h>
#include <wx/string.h>
#include <map>
#include <vector>

#define EESCHEMA_HOTKEY_NAME wxT( "Eeschema" )
#define PCBNEW_HOTKEY_NAME wxT( "PcbNew" )

// A define to allow translation of Hot Key message Info in hotkey help menu
// We do not want to use the _( x ) usual macro from wxWidgets, which calls wxGetTranslation(),
// because the English string is used in key file configuration
// The translated string is used only when displaying the help window.
// Therefore translation tools have to use the "_" and the "_HKI" prefix to extract
// strings to translate
#include <i18n_utility.h>       // _HKI definition

class TOOL_ACTION;
class TOOL_MANAGER;
class EDA_BASE_FRAME;


/*
 * Keep these out of the ASCII range, and out of the WXK range
 */
#define PSEUDO_WXK_CLICK    400
#define PSEUDO_WXK_DBLCLICK 401
#define PSEUDO_WXK_WHEEL    402

/**
 * Return the key code from its user-friendly key name (ie: "Ctrl+M").
 */
int KeyCodeFromKeyName( const wxString& keyname );

/**
 * Return the key name from the key code.
 *
 * Only some wxWidgets key values are handled for function key ( see hotkeyNameList[] )
 *
 * @param aKeycode key code (ASCII value, or wxWidgets value for function keys).
 * @param aIsFound a pointer to a bool to return true if found, or false. an be nullptr default).
 * @return the key name in a wxString.
 */
wxString KeyNameFromKeyCode( int aKeycode, bool* aIsFound = nullptr );

/**
 * In menus we can add a hot key, or an accelerator, or sometimes just a comment.
 *
 * Hot keys can perform actions using the current mouse cursor position and accelerators perform
 * the same action as the associated menu.
 *
 * A comment is used in tool tips for some tools (zoom ..) to show the hot key that performs
 * this action
 */
enum HOTKEY_ACTION_TYPE
{
    IS_HOTKEY,
    IS_COMMENT
};

/**
 * @param aText the base text on which to append the hotkey.
 * @param aHotKey the hotkey keycode.
 * @param aStyle #IS_HOTKEY to add \<tab\>\<keyname\> (shortcuts in menus, same as hotkeys).
 *               #IS_COMMENT to add \<spaces\>\<(keyname)\> mainly in tool tips.
 */
wxString AddHotkeyName( const wxString& aText, int aHotKey, HOTKEY_ACTION_TYPE aStyle = IS_HOTKEY );

/**
 * Display the current hotkey list.
 *
 * @param aFrame current active frame.
 */
void DisplayHotkeyList( EDA_BASE_FRAME* aFrame );

/**
 * Read a hotkey config file into a map.
 *
 * If \a aFileName is empty it will read in the default hotkeys file.
 */
void ReadHotKeyConfig( const wxString&                             aFileName,
                       std::map<std::string, std::pair<int, int>>& aHotKeys );

/**
 * Read a hotkey config file into a map from text in the same format.
 */
void ParseHotKeyConfig( const wxString& aText, std::map<std::string, std::pair<int, int>>& aHotKeys );

/**
 * Read a hotkey config file into a list of actions.
 *
 * If \a aFileName is empty it will read in the active interaction profile's user hotkeys file.
 */
void ReadHotKeyConfigIntoActions( const wxString& aFileName, std::vector<TOOL_ACTION*>& aActions,
                                  const std::string& aEditorScope = std::string() );

/**
 * Set a list of actions to a profile's hotkeys: its defaults, then its user file (or \a aFileName when given).
 */
void ReadHotKeyConfigIntoActions( INTERACTION_PROFILE_ID aProfile, const wxString& aFileName,
                                  std::vector<TOOL_ACTION*>& aActions,
                                  const std::string&         aEditorScope = std::string() );

/**
 * Update the hotkeys config file with the hotkeys from the given actions map.
 *
 * If \a aFileName is empty it will write the active interaction profile's user hotkeys file.
 */
int WriteHotKeyConfig( const std::vector<TOOL_ACTION*>& aActions, const wxString& aFileName = wxEmptyString );

/// The legacy hotkey files' keys by legacy name, one map for each editor whose file holds them.
struct LEGACY_HOTKEY_MAPS
{
    std::map<std::string, int> m_eeschema;
    std::map<std::string, int> m_pcbnew;
};

/**
 * Read the legacy hotkey files of the schematic and PCB editors.
 */
LEGACY_HOTKEY_MAPS ReadLegacyHotkeyMaps();

/**
 * The defaults of an action under a profile: its compiled default, then the legacy file of its editor (chosen by the
 * action name's prefix), then the profile's table. The user's file is not applied.
 *
 * @param aEditorScope is the editor the keys are for, as INTERACTION_PROFILE::ApplyHotKeyLayer() takes it.
 */
std::pair<int, int> ResolveProfileDefaultKeys( const TOOL_ACTION& aAction, const PROFILE_HOTKEY_LAYER& aLayer,
                                               const LEGACY_HOTKEY_MAPS& aLegacyMaps,
                                               const std::string&        aEditorScope = std::string() );

/**
 * Write the user's hotkey file of a profile.
 *
 * For the stock profile (an inactive \a aLayer) this is WriteHotKeyConfig(): the whole set. For any other profile only
 * the actions whose keys differ from the profile's defaults are written, so a later change to the profile's table
 * reaches every action the user never changed. An action the user unbound is written with empty keys; one set back to
 * its default is removed from the file.
 *
 * @param aFileName is the file to write; when empty it is the user file of the active profile.
 */
int WriteProfileHotKeyConfig( const std::vector<TOOL_ACTION*>& aActions, const PROFILE_HOTKEY_LAYER& aLayer,
                              const LEGACY_HOTKEY_MAPS& aLegacyMaps, const wxString& aFileName = wxEmptyString,
                              const std::string& aEditorScope = std::string() );

/**
 * Read hotkey configuration for a given app.
 *
 * @param aFilename the filename to save the hotkeys as.
 * @param aMap The list of keycodes mapped by legacy property names.
 * @return 1 on success, 0 on failure.
*/
int ReadLegacyHotkeyConfigFile( const wxString& aFilename, std::map<std::string, int>& aMap );

/**
 * Read configuration data and fill the current hotkey list with hotkeys.
 *
 * @param aAppname the value of the app's m_FrameName.
 * @param aMap The list of keycodes mapped by legacy property names.
 */
int ReadLegacyHotkeyConfig( const wxString& aAppname, std::map<std::string, int>& aMap );

#endif // HOTKEYS_BASIC_H

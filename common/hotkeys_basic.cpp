/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2010 Wayne Stambaugh <stambaughw@gmail.com>
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

#include <kiface_base.h>
#include <hotkeys_basic.h>
#include <interaction_profile.h>
#include <id.h>
#include <string_utils.h>
#include <eda_base_frame.h>
#include <eda_draw_frame.h>
#include <wildcards_and_files_ext.h>
#include <paths.h>

#include <tool/action_manager.h>
#include <tool/tool_manager.h>
#include "dialogs/dialog_hotkey_list.h"
#include <wx/apptrait.h>
#include <wx/stdpaths.h>
#include <wx/tokenzr.h>
#include <wx/txtstrm.h>
#include <wx/wfstream.h>
#include <tool/tool_action.h>
#include <tool/tool_event.h>


/*
 * class to handle the printable name and the keycode
 */
struct hotkey_name_descr
{
    const wxChar* m_Name;
    int           m_KeyCode;
};


/* table giving the hotkey name from the hotkey code, for special keys
 * Note : when modifiers (ATL, SHIFT, CTRL) do not modify
 * the code of the key, do need to enter the modified key code
 * For instance wxT( "F1" ), WXK_F1 handle F1, AltF1, CtrlF1 ...
 * Key names are:
 *        "Space","Ctrl+Space","Alt+Space" or
 *        "Alt+A","Ctrl+F1", ...
 */
#define KEY_NON_FOUND -1
static struct hotkey_name_descr hotkeyNameList[] =
{
    { wxT( "F1" ),            WXK_F1                 },
    { wxT( "F2" ),            WXK_F2                 },
    { wxT( "F3" ),            WXK_F3                 },
    { wxT( "F4" ),            WXK_F4                 },
    { wxT( "F5" ),            WXK_F5                 },
    { wxT( "F6" ),            WXK_F6                 },
    { wxT( "F7" ),            WXK_F7                 },
    { wxT( "F8" ),            WXK_F8                 },
    { wxT( "F9" ),            WXK_F9                 },
    { wxT( "F10" ),           WXK_F10                },
    { wxT( "F11" ),           WXK_F11                },
    { wxT( "F12" ),           WXK_F12                },
    { wxT( "F13" ),           WXK_F13                },
    { wxT( "F14" ),           WXK_F14                },
    { wxT( "F15" ),           WXK_F15                },
    { wxT( "F16" ),           WXK_F16                },
    { wxT( "F17" ),           WXK_F17                },
    { wxT( "F18" ),           WXK_F18                },
    { wxT( "F19" ),           WXK_F19                },
    { wxT( "F20" ),           WXK_F20                },
    { wxT( "F21" ),           WXK_F21                },
    { wxT( "F22" ),           WXK_F22                },
    { wxT( "F23" ),           WXK_F23                },
    { wxT( "F24" ),           WXK_F24                },

    { wxT( "Esc" ),           WXK_ESCAPE             },
    { wxT( "Del" ),           WXK_DELETE             },
    { wxT( "Tab" ),           WXK_TAB                },
    { wxT( "Back" ),          WXK_BACK               },
    { wxT( "Ins" ),           WXK_INSERT             },

    { wxT( "Home" ),          WXK_HOME               },
    { wxT( "End" ),           WXK_END                },
    { wxT( "PgUp" ),          WXK_PAGEUP             },
    { wxT( "PgDn" ),          WXK_PAGEDOWN           },

    { wxT( "Up" ),            WXK_UP                 },
    { wxT( "Down" ),          WXK_DOWN               },
    { wxT( "Left" ),          WXK_LEFT               },
    { wxT( "Right" ),         WXK_RIGHT              },

    { wxT( "Return" ),        WXK_RETURN             },

    { wxT( "Space" ),         WXK_SPACE              },

    { wxT( "Num Pad 0" ),     WXK_NUMPAD0            },
    { wxT( "Num Pad 1" ),     WXK_NUMPAD1            },
    { wxT( "Num Pad 2" ),     WXK_NUMPAD2            },
    { wxT( "Num Pad 3" ),     WXK_NUMPAD3            },
    { wxT( "Num Pad 4" ),     WXK_NUMPAD4            },
    { wxT( "Num Pad 5" ),     WXK_NUMPAD5            },
    { wxT( "Num Pad 6" ),     WXK_NUMPAD6            },
    { wxT( "Num Pad 7" ),     WXK_NUMPAD7            },
    { wxT( "Num Pad 8" ),     WXK_NUMPAD8            },
    { wxT( "Num Pad 9" ),     WXK_NUMPAD9            },
    { wxT( "Num Pad +" ),     WXK_NUMPAD_ADD         },
    { wxT( "Num Pad -" ),     WXK_NUMPAD_SUBTRACT    },
    { wxT( "Num Pad *" ),     WXK_NUMPAD_MULTIPLY    },
    { wxT( "Num Pad /" ),     WXK_NUMPAD_DIVIDE      },
    { wxT( "Num Pad ." ),     WXK_NUMPAD_SEPARATOR   },
    { wxT( "Num Pad Enter" ), WXK_NUMPAD_ENTER       },
    { wxT( "Num Pad F1"),     WXK_NUMPAD_F1          },
    { wxT( "Num Pad F2"),     WXK_NUMPAD_F2          },
    { wxT( "Num Pad F3"),     WXK_NUMPAD_F3          },
    { wxT( "Num Pad F4"),     WXK_NUMPAD_F4          },

    { wxT( "" ),              0                      },

    { wxT( "Click" ),         PSEUDO_WXK_CLICK       },
    { wxT( "DblClick" ),      PSEUDO_WXK_DBLCLICK    },
    { wxT( "Wheel" ),         PSEUDO_WXK_WHEEL       },

    // Do not change this line: end of list
    { wxT( "" ),              KEY_NON_FOUND          }
};


// name of modifier keys.
// Note: the Ctrl key is Cmd key on Mac OS X.
// However, in wxWidgets defs, the key WXK_CONTROL is the Cmd key,
// so the code using WXK_CONTROL should be ok on any system.
// (on Mac OS X the actual Ctrl key code is WXK_RAW_CONTROL)
#ifdef __WXMAC__
#define USING_MAC_CMD
#endif

#ifdef USING_MAC_CMD
#define MODIFIER_CTRL       wxT( "Cmd+" )
#define MODIFIER_ALT        wxT( "Option+" )
#else
#define MODIFIER_CTRL       wxT( "Ctrl+" )
#define MODIFIER_ALT        wxT( "Alt+" )
#endif
#define MODIFIER_CMD_MAC    wxT( "Cmd+" )
#define MODIFIER_CTRL_BASE  wxT( "Ctrl+" )
#define MODIFIER_SHIFT      wxT( "Shift+" )
#define MODIFIER_META       wxT( "Meta+" )
#define MODIFIER_WIN        wxT( "Win+" )
#define MODIFIER_SUPER      wxT( "Super+" )
#define MODIFIER_ALTGR      wxT( "AltGr+" )


wxString KeyNameFromKeyCode( int aKeycode, bool* aIsFound )
{
    wxString keyname, modifier, fullkeyname;
    int      ii;
    bool     found = false;

    if( aKeycode == WXK_CONTROL )
        return wxString( MODIFIER_CTRL ).BeforeFirst( '+' );
    else if( aKeycode == WXK_RAW_CONTROL )
        return wxString( MODIFIER_CTRL_BASE ).BeforeFirst( '+' );
    else if( aKeycode == WXK_SHIFT )
        return wxString( MODIFIER_SHIFT ).BeforeFirst( '+' );
    else if( aKeycode == WXK_ALT )
        return wxString( MODIFIER_ALT ).BeforeFirst( '+' );
#ifdef WXK_WINDOWS_LEFT
    else if( aKeycode == WXK_WINDOWS_LEFT || aKeycode == WXK_WINDOWS_RIGHT )
        return wxString( MODIFIER_WIN ).BeforeFirst( '+' );
#endif
#ifdef WXK_META
    else if( aKeycode == WXK_META )
        return wxString( MODIFIER_META ).BeforeFirst( '+' );
#endif

    // Assume keycode of 0 is "unassigned"
    if( (aKeycode & MD_CTRL) != 0 )
        modifier << MODIFIER_CTRL;

    if( (aKeycode & MD_ALT) != 0 )
        modifier << MODIFIER_ALT;

    if( (aKeycode & MD_SHIFT) != 0 )
        modifier << MODIFIER_SHIFT;

    if( (aKeycode & MD_META) != 0 )
        modifier << MODIFIER_META;

    if( (aKeycode & MD_SUPER) != 0 )
        modifier << MODIFIER_WIN;

    if( (aKeycode & MD_ALTGR) != 0 )
        modifier << MODIFIER_ALTGR;

    aKeycode &= ~MD_MODIFIER_MASK;

    if( (aKeycode > ' ') && (aKeycode < 0x7F ) )
    {
        found   = true;
        keyname.Append( (wxChar)aKeycode );
    }
    else
    {
        for( ii = 0; ; ii++ )
        {
            if( hotkeyNameList[ii].m_KeyCode == KEY_NON_FOUND ) // End of list
            {
                keyname = wxT( "<unknown>" );
                break;
            }

            if( hotkeyNameList[ii].m_KeyCode == aKeycode )
            {
                keyname = hotkeyNameList[ii].m_Name;
                found   = true;
                break;
            }
        }
    }

    if( aIsFound )
        *aIsFound = found;

    fullkeyname = modifier + keyname;
    return fullkeyname;
}


wxString AddHotkeyName( const wxString& aText, int aHotKey, HOTKEY_ACTION_TYPE aStyle )
{
    wxString msg = aText;
    wxString keyname = KeyNameFromKeyCode( aHotKey );

    if( !keyname.IsEmpty() )
    {
        switch( aStyle )
        {
        case IS_HOTKEY:
        {
            // Don't add a suffix for unassigned hotkeys:
            // WX spews debug from wxAcceleratorEntry::ParseAccel if it doesn't
            // recognize the keyname, which is the case for <unassigned>.
            if( aHotKey != 0 )
            {
                msg << wxT( "\t" ) << keyname;
            }
            break;
        }
        case IS_COMMENT:
        {
            msg << wxT( " (" ) << keyname << wxT( ")" );
            break;
        }
        }
    }

#ifdef USING_MAC_CMD
    // On OSX, the modifier equivalent to the Ctrl key of PCs
    // is the Cmd key, but in code we should use Ctrl as prefix in menus
    msg.Replace( MODIFIER_CMD_MAC, MODIFIER_CTRL_BASE );
#endif

    return msg;
}


int KeyCodeFromKeyName( const wxString& keyname )
{
    int ii, keycode = KEY_NON_FOUND;

    // Search for modifiers: Ctrl+ Alt+ Shift+ and others
    // Note: on Mac OSX, the Cmd key is equiv here to Ctrl
    wxString key = keyname;
    wxString prefix;
    int modifier = 0;

    while( true )
    {
        prefix.Empty();

        if( key.StartsWith( MODIFIER_CTRL_BASE ) )
        {
            modifier |= MD_CTRL;
            prefix = MODIFIER_CTRL_BASE;
        }
        else if( key.StartsWith( MODIFIER_CMD_MAC ) )
        {
            modifier |= MD_CTRL;
            prefix = MODIFIER_CMD_MAC;
        }
        else if( key.StartsWith( MODIFIER_ALT ) )
        {
            modifier |= MD_ALT;
            prefix = MODIFIER_ALT;
        }
        else if( key.StartsWith( MODIFIER_SHIFT ) )
        {
            modifier |= MD_SHIFT;
            prefix = MODIFIER_SHIFT;
        }
        else if( key.StartsWith( MODIFIER_META ) )
        {
            modifier |= MD_META;
            prefix = MODIFIER_META;
        }
        else if( key.StartsWith( MODIFIER_WIN ) )
        {
            modifier |= MD_SUPER;
            prefix = MODIFIER_WIN;
        }
        else if( key.StartsWith( MODIFIER_SUPER ) )
        {
            modifier |= MD_SUPER;
            prefix = MODIFIER_SUPER;
        }
        else if( key.StartsWith( MODIFIER_ALTGR ) )
        {
            modifier |= MD_ALTGR;
            prefix = MODIFIER_ALTGR;
        }
        else
        {
            break;
        }

        if( !prefix.IsEmpty() )
            key.Remove( 0, prefix.Len() );
    }

    if( (key.length() == 1) && (key[0] > ' ') && (key[0] < 0x7F) )
    {
        keycode = key[0];
        keycode += modifier;
        return keycode;
    }

    for( ii = 0; hotkeyNameList[ii].m_KeyCode != KEY_NON_FOUND; ii++ )
    {
        if( key.CmpNoCase( hotkeyNameList[ii].m_Name ) == 0 )
        {
            keycode = hotkeyNameList[ii].m_KeyCode + modifier;
            break;
        }
    }

    return keycode;
}


void DisplayHotkeyList( EDA_BASE_FRAME* aParent )
{
    DIALOG_LIST_HOTKEYS dlg( aParent );
    dlg.ShowModal();
}


void ParseHotKeyConfig( const wxString& aText, std::map<std::string, std::pair<int, int>>& aHotKeys )
{
    wxString input = aText;
    input.Replace( "\r\n", "\n" );  // Convert Windows files to Unix line-ends
    wxStringTokenizer fileTokenizer( input, "\n", wxTOKEN_STRTOK );

    while( fileTokenizer.HasMoreTokens() )
    {
        wxStringTokenizer lineTokenizer( fileTokenizer.GetNextToken(), "\t" );

        wxString cmdName   = lineTokenizer.GetNextToken();
        wxString primary   = lineTokenizer.GetNextToken();
        wxString secondary = lineTokenizer.GetNextToken();

        if( !cmdName.IsEmpty() )
        {
            aHotKeys[cmdName.ToStdString()] = std::pair<int, int>( KeyCodeFromKeyName( primary ),
                                                                   KeyCodeFromKeyName( secondary ) );
        }
    }
}


void ReadHotKeyConfig( const wxString& aFileName, std::map<std::string, std::pair<int, int>>& aHotKeys )
{
    wxString fileName = aFileName;

    // The default file is the active interaction profile's: user.hotkeys for the KiCad profile
    if( fileName.IsEmpty() )
        fileName = INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE::Active() );

    if( !wxFile::Exists( fileName ) )
        return;

    wxFFile file( fileName, "rb" );

    if( !file.IsOpened() ) // There is a problem to open file
        return;

    wxString input;
    file.ReadAll( &input );
    ParseHotKeyConfig( input, aHotKeys );
}


void ReadHotKeyConfigIntoActions( const wxString& aFileName, std::vector<TOOL_ACTION*>& aActions,
                                  const std::string& aEditorScope )
{
    ReadHotKeyConfigIntoActions( INTERACTION_PROFILE::Active(), aFileName, aActions, aEditorScope );
}


void ReadHotKeyConfigIntoActions( INTERACTION_PROFILE_ID aProfile, const wxString& aFileName,
                                  std::vector<TOOL_ACTION*>& aActions, const std::string& aEditorScope )
{
    std::map<std::string, std::pair<int, int>> hotkeys;

    // Read the existing config (all hotkeys): the file named, or the profile's own user file
    ReadHotKeyConfig( aFileName.IsEmpty() ? INTERACTION_PROFILE::UserHotKeyFilePath( aProfile ) : aFileName, hotkeys );

    // A profile other than KiCad's starts every action from the profile's defaults, not the compiled ones
    PROFILE_HOTKEY_LAYER profileLayer = INTERACTION_PROFILE::BuildHotKeyLayer( aProfile );
    LEGACY_HOTKEY_MAPS   legacyMaps;

    if( profileLayer.m_isActive )
        legacyMaps = ReadLegacyHotkeyMaps();

    // Set each tool action hotkey to the config file hotkey if present
    for( TOOL_ACTION* action : aActions )
    {
        if( profileLayer.m_isActive )
        {
            std::pair<int, int> defaults = ResolveProfileDefaultKeys( *action, profileLayer, legacyMaps, aEditorScope );
            action->SetHotKey( defaults.first, defaults.second );
        }

        if( hotkeys.find( action->GetName() ) != hotkeys.end() )
        {
            std::pair<int, int> keys = hotkeys[action->GetName()];
            action->SetHotKey( keys.first, keys.second );
        }
    }
}


LEGACY_HOTKEY_MAPS ReadLegacyHotkeyMaps()
{
    LEGACY_HOTKEY_MAPS maps;

    ReadLegacyHotkeyConfig( SCH_EDIT_FRAME_NAME, maps.m_eeschema );
    ReadLegacyHotkeyConfig( PCB_EDIT_FRAME_NAME, maps.m_pcbnew );

    return maps;
}


std::pair<int, int> ResolveProfileDefaultKeys( const TOOL_ACTION& aAction, const PROFILE_HOTKEY_LAYER& aLayer,
                                               const LEGACY_HOTKEY_MAPS& aLegacyMaps, const std::string& aEditorScope )
{
    static const std::map<std::string, int> noLegacyKeys;

    const std::string&                name = aAction.GetName();
    const std::map<std::string, int>* legacyKeys = &noLegacyKeys;

    // Only the schematic and PCB editors' legacy files are read here; the other actions' legacy keys are not
    if( name.compare( 0, 9, "eeschema." ) == 0 )
        legacyKeys = &aLegacyMaps.m_eeschema;
    else if( name.compare( 0, 8, "pcbnew." ) == 0 )
        legacyKeys = &aLegacyMaps.m_pcbnew;

    return ACTION_MANAGER::ResolveHotKey( aAction, *legacyKeys, aLayer, std::map<std::string, std::pair<int, int>>(),
                                          aEditorScope );
}


int WriteProfileHotKeyConfig( const std::vector<TOOL_ACTION*>& aActions, const PROFILE_HOTKEY_LAYER& aLayer,
                              const LEGACY_HOTKEY_MAPS& aLegacyMaps, const wxString& aFileName,
                              const std::string& aEditorScope )
{
    // KiCad's own profile keeps upstream's way: the whole set
    if( !aLayer.m_isActive )
        return WriteHotKeyConfig( aActions, aFileName );

    std::map<std::string, std::pair<int, int>> hotkeys;
    wxFileName fn( aFileName.IsEmpty() ? INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE::Active() )
                                       : aFileName );

    ReadHotKeyConfig( fn.GetFullPath(), hotkeys );

    for( const TOOL_ACTION* action : aActions )
    {
        std::pair<int, int> keys( action->GetHotKey(), action->GetHotKeyAlt() );

        // Only a difference from the profile's defaults is the user's: an unbound action is written with empty keys,
        // and one set back to its default drops out of the file
        if( keys == ResolveProfileDefaultKeys( *action, aLayer, aLegacyMaps, aEditorScope ) )
            hotkeys.erase( action->GetName() );
        else
            hotkeys[action->GetName()] = keys;
    }

    wxFFileOutputStream outStream( fn.GetFullPath() );
    wxTextOutputStream  txtStream( outStream, wxEOL_UNIX );

    for( const std::pair<const std::string, std::pair<int, int>>& entry : hotkeys )
    {
        txtStream << entry.first << "\t" << KeyNameFromKeyCode( entry.second.first ) << "\t"
                  << KeyNameFromKeyCode( entry.second.second ) << endl;
    }

    txtStream.Flush();
    outStream.Close();

    return 1;
}


int WriteHotKeyConfig( const std::vector<TOOL_ACTION*>& aActions, const wxString& aFileName )
{
    std::map<std::string, std::pair<int, int>> hotkeys;
    wxFileName fn( aFileName.IsEmpty() ? INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE::Active() )
                                       : aFileName );

    // Read the existing config (all hotkeys)
    ReadHotKeyConfig( fn.GetFullPath(), hotkeys );

    // Overlay the current app's hotkey definitions onto the map
    for( const TOOL_ACTION* action : aActions )
        hotkeys[ action->GetName() ] = std::pair<int, int>( action->GetHotKey(), action->GetHotKeyAlt() );

    // Write entire hotkey set
    wxFFileOutputStream outStream( fn.GetFullPath() );
    wxTextOutputStream  txtStream( outStream, wxEOL_UNIX );

    for( const std::pair<const std::string, std::pair<int, int>>& entry : hotkeys )
    {
        txtStream << entry.first
            << "\t" << KeyNameFromKeyCode( entry.second.first )
            << "\t" << KeyNameFromKeyCode( entry.second.second ) << endl;
    }

    txtStream.Flush();
    outStream.Close();

    return 1;
}


int ReadLegacyHotkeyConfig( const wxString& aAppname, std::map<std::string, int>& aMap )
{
    // For Eeschema and Pcbnew frames, we read the new combined file.
    // For other kifaces, we read the frame-based file
    if( aAppname == LIB_EDIT_FRAME_NAME || aAppname == SCH_EDIT_FRAME_NAME )
    {
        return ReadLegacyHotkeyConfigFile( EESCHEMA_HOTKEY_NAME, aMap );
    }
    else if( aAppname == PCB_EDIT_FRAME_NAME || aAppname == FOOTPRINT_EDIT_FRAME_NAME )
    {
        return ReadLegacyHotkeyConfigFile( PCBNEW_HOTKEY_NAME, aMap );
    }

    return ReadLegacyHotkeyConfigFile( aAppname, aMap );
}


int ReadLegacyHotkeyConfigFile( const wxString& aFilename, std::map<std::string, int>& aMap )
{
    wxFileName fn( aFilename );

    fn.SetExt( FILEEXT::HotkeyFileExtension );
    fn.SetPath( PATHS::GetUserSettingsPath() );

    if( !wxFile::Exists( fn.GetFullPath() ) )
        return 0;

    wxFFile cfgfile( fn.GetFullPath(), "rb" );

    if( !cfgfile.IsOpened() )       // There is a problem to open file
        return 0;

    // get length
    wxFileOffset size = cfgfile.Length();

    // read data
    std::vector<char> buffer( size );
    cfgfile.Read( buffer.data(), size );
    wxString data( buffer.data(), wxConvUTF8, size );

    // Is this the wxConfig format? If so, remove "Keys=" and parse the newlines.
    if( data.StartsWith( wxT("Keys="), &data ) )
        data.Replace( "\\n", "\n", true );

    // parse
    wxStringTokenizer tokenizer( data, "\r\n", wxTOKEN_STRTOK );

    while( tokenizer.HasMoreTokens() )
    {
        wxString          line = tokenizer.GetNextToken();
        wxStringTokenizer lineTokenizer( line, " \t\r\n" );

        wxString          line_type = lineTokenizer.GetNextToken();

        if( line_type[0]  == '#' ) // comment
            continue;

        if( line_type[0]  == '[' ) // tags ignored reading legacy hotkeys
            continue;

        if( line_type == wxT( "$Endlist" ) )
            break;

        if( line_type != wxT( "shortcut" ) )
            continue;

        // Get the key name
        lineTokenizer.SetString( lineTokenizer.GetString(), L"\"\r\n\t ", wxTOKEN_STRTOK );
        wxString keyname = lineTokenizer.GetNextToken();

        wxString remainder = lineTokenizer.GetString();

        // Get the command name
        wxString fctname = remainder.AfterFirst( '\"' ).BeforeFirst( '\"' );

        // Add the pair to the map
        aMap[ fctname.ToStdString() ] = KeyCodeFromKeyName( keyname );
    }

    // cleanup
    cfgfile.Close();
    return 1;
}



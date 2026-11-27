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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <boost/test/unit_test.hpp>

#include <boost/test/unit_test.hpp>

#include <hotkey_store.h>
#include <hotkeys_basic.h>
#include <interaction_profile.h>
#include <settings/common_settings.h>
#include <tool/action_manager.h>
#include <tool/tool_action.h>
#include <tool/tool_event.h>

#include <wx/dir.h>
#include <wx/ffile.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

// The interaction profile (docs/reckless/interaction-profile.md, 5.1 and 5.2): the setting, the profile layer of hotkey
// resolution, and a user hotkey file for each profile. The tests use the Altium-style table as checked in, which for
// step 1 binds Q to toggleUnits, Home to zoomCenter and Shift+S to highContrastModeCycle.

namespace
{
/// Actions with the names and compiled keys of real ones the table binds or takes keys from, and one it never touches.
struct PROFILE_TEST_ACTIONS
{
    TOOL_ACTION toggleUnits = TOOL_ACTION( TOOL_ACTION_ARGS()
                                                   .Name( "common.Control.toggleUnits" )
                                                   .FriendlyName( "Switch units" )
                                                   .Scope( AS_GLOBAL )
                                                   .DefaultHotkey( MD_CTRL + 'U' ) );
    TOOL_ACTION zoomCenter = TOOL_ACTION( TOOL_ACTION_ARGS()
                                                  .Name( "common.Control.zoomCenter" )
                                                  .FriendlyName( "Center on Cursor" )
                                                  .Scope( AS_GLOBAL )
                                                  .DefaultHotkey( WXK_F4 ) );
    TOOL_ACTION highContrastModeCycle = TOOL_ACTION( TOOL_ACTION_ARGS()
                                                             .Name( "common.Control.highContrastModeCycle" )
                                                             .FriendlyName( "Inactive Layer View Mode" )
                                                             .Scope( AS_GLOBAL )
                                                             .DefaultHotkey( 'H' ) );
    /// Compiled with Q, which the profile takes for toggleUnits
    TOOL_ACTION customTrackViaSize = TOOL_ACTION( TOOL_ACTION_ARGS()
                                                          .Name( "pcbnew.InteractiveRouter.CustomTrackViaSize" )
                                                          .FriendlyName( "Custom Track/Via Size" )
                                                          .Scope( AS_GLOBAL )
                                                          .DefaultHotkey( 'Q' ) );
    /// Compiled with Shift+S, which the profile takes for highContrastModeCycle
    TOOL_ACTION magneticSnapToggle = TOOL_ACTION( TOOL_ACTION_ARGS()
                                                          .Name( "common.test.snap" )
                                                          .FriendlyName( "Snap" )
                                                          .Scope( AS_GLOBAL )
                                                          .DefaultHotkey( MD_SHIFT + 'S' ) );
    /// Compiled with Home as its alternate, which the profile takes for zoomCenter
    TOOL_ACTION zoomFitScreen = TOOL_ACTION( TOOL_ACTION_ARGS()
                                                     .Name( "common.Control.zoomFitScreen" )
                                                     .FriendlyName( "Zoom to Fit" )
                                                     .Scope( AS_GLOBAL )
                                                     .DefaultHotkey( int( MD_CTRL ) + int( WXK_HOME ) )
                                                     .DefaultHotkeyAlt( WXK_HOME ) );
    TOOL_ACTION untouched = TOOL_ACTION( TOOL_ACTION_ARGS()
                                                 .Name( "common.test.untouched" )
                                                 .FriendlyName( "Untouched" )
                                                 .Scope( AS_GLOBAL )
                                                 .DefaultHotkey( 'K' )
                                                 .DefaultHotkeyAlt( 'B' ) );

    std::vector<TOOL_ACTION*> List()
    {
        return { &toggleUnits,   &zoomCenter, &highContrastModeCycle, &customTrackViaSize, &magneticSnapToggle,
                 &zoomFitScreen, &untouched };
    }
};

using KEYS = std::pair<int, int>;

/// What the keys of an action resolve to with a profile and a user file, as ACTION_MANAGER::UpdateHotKeys does.
KEYS resolve( const TOOL_ACTION& aAction, INTERACTION_PROFILE_ID aProfile,
              const PROFILE_HOTKEY_MAP& aUserKeys = PROFILE_HOTKEY_MAP() )
{
    return ACTION_MANAGER::ResolveHotKey( aAction, std::map<std::string, int>(),
                                          INTERACTION_PROFILE::BuildHotKeyLayer( aProfile ), aUserKeys );
}

/// A new directory for the user files of a test, removed with it
struct TEMP_DIRECTORY
{
    TEMP_DIRECTORY()
    {
        wxFileName fileName( wxFileName::CreateTempFileName( wxS( "interaction_profile" ) ) );
        wxRemoveFile( fileName.GetFullPath() );
        m_path = fileName.GetFullPath();
        wxFileName::Mkdir( m_path );
    }

    ~TEMP_DIRECTORY() { wxFileName::Rmdir( m_path, wxPATH_RMDIR_RECURSIVE ); }

    wxString m_path;
};

wxString readWholeFile( const wxString& aPath )
{
    wxString content;
    wxFFile  file( aPath, wxS( "rb" ) );

    if( file.IsOpened() )
        file.ReadAll( &content );

    return content;
}
} // namespace


BOOST_AUTO_TEST_SUITE( InteractionProfile )

BOOST_AUTO_TEST_CASE( SettingDefaultsAndRoundTrip )
{
    COMMON_SETTINGS settings;

    // A parameter missing from the file loads its default, so an existing configuration needs no migration.
    settings.Load();

#ifdef KICAD_HOUSE_INTERACTION
    BOOST_CHECK( settings.m_Input.interaction_profile == INTERACTION_PROFILE_ID::ALTIUM );
#else
    BOOST_CHECK( settings.m_Input.interaction_profile == INTERACTION_PROFILE_ID::STOCK );
#endif

    settings.m_Input.interaction_profile = INTERACTION_PROFILE_ID::ALTIUM;
    settings.Store();

    settings.m_Input.interaction_profile = INTERACTION_PROFILE_ID::STOCK;
    settings.Load();
    BOOST_CHECK( settings.m_Input.interaction_profile == INTERACTION_PROFILE_ID::ALTIUM );
}


BOOST_AUTO_TEST_CASE( TableParsing )
{
    PROFILE_HOTKEY_MAP bindings;

    INTERACTION_PROFILE::ParseHotKeyTable( "# a comment\n\ncommon.a\tQ\tShift+W\r\ncommon.b\tHome\n", bindings );

    BOOST_CHECK_EQUAL( bindings.size(), 2u );
    BOOST_CHECK( bindings["common.a"] == KEYS( 'Q', MD_SHIFT + 'W' ) );
    BOOST_CHECK( bindings["common.b"] == KEYS( WXK_HOME, 0 ) );

    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayerFromText( "common.a\tQ\tShift+W\n" );
    BOOST_CHECK( layer.m_isActive );
    BOOST_CHECK( layer.m_takenKeys == std::set<int>( { 'Q', MD_SHIFT + 'W' } ) );
}


BOOST_AUTO_TEST_CASE( TableKeysTakeTheMacSpelling )
{
    // macOS's hotkey file says Cmd and Option, and its parser reads no Alt: the tables are rewritten before parsing
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::MacKeyNames( "pcbnew.A\tCtrl+Alt+G\npcbnew.B\tAlt+F\tCtrl+Shift+G\n" ),
                       "pcbnew.A\tCmd+Option+G\npcbnew.B\tOption+F\tCmd+Shift+G\n" );
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::MacKeyNames( "common.C\tShift+S\tAltGr+X\n" ),
                       "common.C\tShift+S\tAltGr+X\n" );

#ifdef __WXMAC__
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::PlatformKeyNames( "Ctrl+Alt+G" ), "Cmd+Option+G" );
    BOOST_CHECK_GT( KeyCodeFromKeyName( wxS( "Cmd+Option+G" ) ), 0 );
#else
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::PlatformKeyNames( "Ctrl+Alt+G" ), "Ctrl+Alt+G" );
#endif
}


BOOST_AUTO_TEST_CASE( EmbeddedTables )
{
    BOOST_CHECK( INTERACTION_PROFILE::HotKeyTableText( INTERACTION_PROFILE_ID::STOCK ).empty() );
    BOOST_CHECK( !INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::STOCK ).m_isActive );

    PROFILE_HOTKEY_LAYER altium = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    BOOST_CHECK( altium.m_isActive );
    BOOST_CHECK( altium.m_bindings.count( "common.Control.toggleUnits" ) );
    BOOST_CHECK( altium.m_takenKeys.count( 'Q' ) );
}


BOOST_AUTO_TEST_CASE( SelectingTheProfileChangesTheBindingsAndBack )
{
    PROFILE_TEST_ACTIONS actions;

    // KiCad: the compiled defaults
    BOOST_CHECK( resolve( actions.toggleUnits, INTERACTION_PROFILE_ID::STOCK ) == KEYS( MD_CTRL + 'U', 0 ) );
    BOOST_CHECK( resolve( actions.zoomCenter, INTERACTION_PROFILE_ID::STOCK ) == KEYS( WXK_F4, 0 ) );
    BOOST_CHECK( resolve( actions.highContrastModeCycle, INTERACTION_PROFILE_ID::STOCK ) == KEYS( 'H', 0 ) );

    // Altium-style: the table's
    BOOST_CHECK( resolve( actions.toggleUnits, INTERACTION_PROFILE_ID::ALTIUM ) == KEYS( 'Q', 0 ) );
    BOOST_CHECK( resolve( actions.zoomCenter, INTERACTION_PROFILE_ID::ALTIUM ) == KEYS( WXK_HOME, 0 ) );
    BOOST_CHECK( resolve( actions.highContrastModeCycle, INTERACTION_PROFILE_ID::ALTIUM )
                 == KEYS( MD_SHIFT + 'S', 0 ) );

    // And KiCad again restores them
    BOOST_CHECK( resolve( actions.toggleUnits, INTERACTION_PROFILE_ID::STOCK ) == KEYS( MD_CTRL + 'U', 0 ) );
    BOOST_CHECK( resolve( actions.zoomCenter, INTERACTION_PROFILE_ID::STOCK ) == KEYS( WXK_F4, 0 ) );
    BOOST_CHECK( resolve( actions.highContrastModeCycle, INTERACTION_PROFILE_ID::STOCK ) == KEYS( 'H', 0 ) );
}


BOOST_AUTO_TEST_CASE( KiCadProfileIsExactlyTheCompiledDefaults )
{
    PROFILE_TEST_ACTIONS      actions;
    std::vector<TOOL_ACTION*> everyAction = actions.List();

    // The whole set the program has linked in as well as the test actions
    for( TOOL_ACTION* action : ACTION_MANAGER::GetActionList() )
        everyAction.push_back( action );

    BOOST_REQUIRE( everyAction.size() > actions.List().size() );

    for( const TOOL_ACTION* action : everyAction )
    {
        BOOST_CHECK_MESSAGE( resolve( *action, INTERACTION_PROFILE_ID::STOCK )
                                     == KEYS( action->GetDefaultHotKey(), action->GetDefaultHotKeyAlt() ),
                             action->GetName() << " changed under the KiCad profile" );
    }
}


BOOST_AUTO_TEST_CASE( AKeyTheProfileTakesDropsOutOfTheCompiledDefaults )
{
    PROFILE_TEST_ACTIONS actions;

    // Q was CustomTrackViaSize's; the profile binds it to toggleUnits, so CustomTrackViaSize has none
    BOOST_CHECK( resolve( actions.customTrackViaSize, INTERACTION_PROFILE_ID::STOCK ) == KEYS( 'Q', 0 ) );
    BOOST_CHECK( resolve( actions.customTrackViaSize, INTERACTION_PROFILE_ID::ALTIUM ) == KEYS( 0, 0 ) );

    // Shift+S likewise
    BOOST_CHECK( resolve( actions.magneticSnapToggle, INTERACTION_PROFILE_ID::ALTIUM ) == KEYS( 0, 0 ) );

    // A taken key is removed from the slot that held it only: the primary stays, the alternate goes
    BOOST_CHECK( resolve( actions.zoomFitScreen, INTERACTION_PROFILE_ID::ALTIUM )
                 == KEYS( int( MD_CTRL ) + int( WXK_HOME ), 0 ) );

    // An action no key of which is taken keeps its keys, alternate included
    BOOST_CHECK( resolve( actions.untouched, INTERACTION_PROFILE_ID::ALTIUM ) == KEYS( 'K', 'B' ) );
}


BOOST_AUTO_TEST_CASE( TheUserFileWinsOverTheProfile )
{
    PROFILE_TEST_ACTIONS actions;
    PROFILE_HOTKEY_MAP   userKeys;

    userKeys["common.Control.toggleUnits"] = KEYS( 'X', 0 );
    userKeys["pcbnew.InteractiveRouter.CustomTrackViaSize"] = KEYS( 'Q', 0 );

    BOOST_CHECK( resolve( actions.toggleUnits, INTERACTION_PROFILE_ID::ALTIUM, userKeys ) == KEYS( 'X', 0 ) );

    // The user may bind a key the profile took, for an action the profile left alone
    BOOST_CHECK( resolve( actions.customTrackViaSize, INTERACTION_PROFILE_ID::ALTIUM, userKeys ) == KEYS( 'Q', 0 ) );
}


BOOST_AUTO_TEST_CASE( EachProfileHasItsOwnUserFile )
{
    TEMP_DIRECTORY directory;

    wxString kicadFile = INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE_ID::STOCK, directory.m_path );
    wxString altiumFile = INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE_ID::ALTIUM, directory.m_path );

    BOOST_CHECK_EQUAL( wxFileName( kicadFile ).GetFullName(), "user.hotkeys" );
    BOOST_CHECK_EQUAL( wxFileName( altiumFile ).GetFullName(), "user.altium.hotkeys" );

    PROFILE_TEST_ACTIONS      actions;
    std::vector<TOOL_ACTION*> list = actions.List();

    // The user customizes the KiCad profile
    actions.untouched.SetHotKey( 'K', 0 );
    WriteHotKeyConfig( list, kicadFile );
    wxString kicadContent = readWholeFile( kicadFile );

    // Then the Altium-style one
    actions.untouched.SetHotKey( 'A', 'B' );
    WriteHotKeyConfig( list, altiumFile );

    // Writing the second left the first alone, and each reads back its own keys
    BOOST_CHECK_EQUAL( readWholeFile( kicadFile ), kicadContent );

    PROFILE_HOTKEY_MAP kicadKeys;
    PROFILE_HOTKEY_MAP altiumKeys;
    ReadHotKeyConfig( kicadFile, kicadKeys );
    ReadHotKeyConfig( altiumFile, altiumKeys );

    BOOST_CHECK( kicadKeys["common.test.untouched"] == KEYS( 'K', 0 ) );
    BOOST_CHECK( altiumKeys["common.test.untouched"] == KEYS( 'A', 'B' ) );

    // Loading a profile's keys into actions reads that profile's file over the profile's defaults
    ReadHotKeyConfigIntoActions( INTERACTION_PROFILE_ID::STOCK, kicadFile, list );
    BOOST_CHECK_EQUAL( actions.untouched.GetHotKey(), 'K' );
    BOOST_CHECK_EQUAL( actions.toggleUnits.GetHotKey(), MD_CTRL + 'U' );

    ReadHotKeyConfigIntoActions( INTERACTION_PROFILE_ID::ALTIUM, altiumFile, list );
    BOOST_CHECK_EQUAL( actions.untouched.GetHotKey(), 'A' );
}


/// Set every action to the profile's defaults, as the editor does when it loads a profile
void setToProfileDefaults( PROFILE_TEST_ACTIONS& aActions, const PROFILE_HOTKEY_LAYER& aLayer )
{
    for( TOOL_ACTION* action : aActions.List() )
    {
        KEYS keys = ResolveProfileDefaultKeys( *action, aLayer, LEGACY_HOTKEY_MAPS() );
        action->SetHotKey( keys.first, keys.second );
    }
}


BOOST_AUTO_TEST_CASE( AProfileFileHoldsOnlyWhatTheUserChanged )
{
    TEMP_DIRECTORY directory;
    wxString       file = INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE_ID::ALTIUM, directory.m_path );
    PROFILE_TEST_ACTIONS actions;
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    // Saved with nothing changed, the file holds nothing
    setToProfileDefaults( actions, layer );
    WriteProfileHotKeyConfig( actions.List(), layer, LEGACY_HOTKEY_MAPS(), file );

    PROFILE_HOTKEY_MAP written;
    ReadHotKeyConfig( file, written );
    BOOST_CHECK( written.empty() );

    // One action changed: only it is written
    actions.untouched.SetHotKey( 'K', 0 );
    WriteProfileHotKeyConfig( actions.List(), layer, LEGACY_HOTKEY_MAPS(), file );

    ReadHotKeyConfig( file, written );
    BOOST_CHECK_EQUAL( written.size(), 1u );
    BOOST_CHECK( written["common.test.untouched"] == KEYS( 'K', 0 ) );
}


BOOST_AUTO_TEST_CASE( ADeliberatelyUnboundActionIsWrittenWithEmptyKeys )
{
    TEMP_DIRECTORY directory;
    wxString       file = INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE_ID::ALTIUM, directory.m_path );
    PROFILE_TEST_ACTIONS actions;
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    setToProfileDefaults( actions, layer );
    actions.toggleUnits.SetHotKey( 0, 0 );
    WriteProfileHotKeyConfig( actions.List(), layer, LEGACY_HOTKEY_MAPS(), file );

    BOOST_CHECK( readWholeFile( file ).Contains( wxS( "common.Control.toggleUnits\t\t" ) ) );

    // Read back, it overrides the profile's binding: the action stays unbound
    PROFILE_HOTKEY_MAP written;
    ReadHotKeyConfig( file, written );
    BOOST_REQUIRE( written.count( "common.Control.toggleUnits" ) );
    BOOST_CHECK( resolve( actions.toggleUnits, INTERACTION_PROFILE_ID::ALTIUM, written ) == KEYS( 0, 0 ) );
}


BOOST_AUTO_TEST_CASE( AnActionSetBackToTheProfileDefaultDropsOutOfTheFile )
{
    TEMP_DIRECTORY directory;
    wxString       file = INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE_ID::ALTIUM, directory.m_path );
    PROFILE_TEST_ACTIONS actions;
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    setToProfileDefaults( actions, layer );
    actions.toggleUnits.SetHotKey( 'X', 0 );
    actions.untouched.SetHotKey( 'K', 0 );
    WriteProfileHotKeyConfig( actions.List(), layer, LEGACY_HOTKEY_MAPS(), file );

    PROFILE_HOTKEY_MAP written;
    ReadHotKeyConfig( file, written );
    BOOST_CHECK_EQUAL( written.size(), 2u );

    // toggleUnits goes back to the profile's Q
    actions.toggleUnits.SetHotKey( 'Q', 0 );
    WriteProfileHotKeyConfig( actions.List(), layer, LEGACY_HOTKEY_MAPS(), file );

    written.clear();
    ReadHotKeyConfig( file, written );
    BOOST_CHECK_EQUAL( written.size(), 1u );
    BOOST_CHECK( !written.count( "common.Control.toggleUnits" ) );
    BOOST_CHECK( written.count( "common.test.untouched" ) );
}


BOOST_AUTO_TEST_CASE( ChangingTheTableAfterASaveReachesWhatTheUserNeverTouched )
{
    TEMP_DIRECTORY directory;
    wxString       file = INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE_ID::ALTIUM, directory.m_path );
    PROFILE_TEST_ACTIONS actions;

    // A first version of the profile's table, and a save in which the user changed one action only
    PROFILE_HOTKEY_LAYER firstTable =
            INTERACTION_PROFILE::BuildHotKeyLayerFromText( "common.Control.toggleUnits\tQ\n" );

    setToProfileDefaults( actions, firstTable );
    actions.untouched.SetHotKey( 'K', 0 );
    WriteProfileHotKeyConfig( actions.List(), firstTable, LEGACY_HOTKEY_MAPS(), file );

    // The table then changes, as it does when step 3 fills it in
    PROFILE_HOTKEY_LAYER laterTable =
            INTERACTION_PROFILE::BuildHotKeyLayerFromText( "common.Control.toggleUnits\tT\n" );

    PROFILE_HOTKEY_MAP userKeys;
    ReadHotKeyConfig( file, userKeys );

    auto resolveWith = [&]( const TOOL_ACTION& aAction, const PROFILE_HOTKEY_LAYER& aLayer )
    {
        return ACTION_MANAGER::ResolveHotKey( aAction, std::map<std::string, int>(), aLayer, userKeys );
    };

    BOOST_CHECK( resolveWith( actions.toggleUnits, firstTable ) == KEYS( 'Q', 0 ) );
    BOOST_CHECK( resolveWith( actions.toggleUnits, laterTable ) == KEYS( 'T', 0 ) );
    BOOST_CHECK( resolveWith( actions.untouched, laterTable ) == KEYS( 'K', 0 ) );
}


BOOST_AUTO_TEST_CASE( ResetGivesTheProfilesDefaults )
{
    PROFILE_TEST_ACTIONS actions;

    HOTKEY_STORE store;
    store.Init( actions.List(), false );

    auto keysOf = [&]( const TOOL_ACTION& aAction ) -> KEYS
    {
        for( HOTKEY_SECTION& section : store.GetSections() )
        {
            for( HOTKEY& hotkey : section.m_HotKeys )
            {
                if( hotkey.m_Actions[0] == &aAction )
                    return KEYS( hotkey.m_EditKeycode, hotkey.m_EditKeycodeAlt );
            }
        }

        return KEYS( -1, -1 );
    };

    store.ResetAllHotkeysToDefault( INTERACTION_PROFILE_ID::ALTIUM );
    BOOST_CHECK( store.GetProfile() == INTERACTION_PROFILE_ID::ALTIUM );
    BOOST_CHECK( keysOf( actions.toggleUnits ) == KEYS( 'Q', 0 ) );
    BOOST_CHECK( keysOf( actions.customTrackViaSize ) == KEYS( 0, 0 ) );
    BOOST_CHECK( keysOf( actions.untouched ) == KEYS( 'K', 'B' ) );

    // The no-argument reset is the store's profile's
    store.GetSections()[0].m_HotKeys[0].m_EditKeycode = 'J';
    store.ResetAllHotkeysToDefault();
    BOOST_CHECK( keysOf( actions.toggleUnits ) == KEYS( 'Q', 0 ) );
    BOOST_CHECK( keysOf( actions.zoomCenter ) == KEYS( WXK_HOME, 0 ) );

    store.ResetAllHotkeysToDefault( INTERACTION_PROFILE_ID::STOCK );
    BOOST_CHECK( keysOf( actions.toggleUnits ) == KEYS( MD_CTRL + 'U', 0 ) );
    BOOST_CHECK( keysOf( actions.customTrackViaSize ) == KEYS( 'Q', 0 ) );

    // Switching the store to a profile applies that profile's user keys over its defaults
    PROFILE_HOTKEY_MAP userKeys;
    userKeys["common.test.untouched"] = KEYS( 'K', 0 );
    store.SetProfile( INTERACTION_PROFILE_ID::ALTIUM, userKeys );
    BOOST_CHECK( keysOf( actions.untouched ) == KEYS( 'K', 0 ) );
    BOOST_CHECK( keysOf( actions.toggleUnits ) == KEYS( 'Q', 0 ) );
}


BOOST_AUTO_TEST_CASE( ConflictChecksRunOnTheResolvedSet )
{
    PROFILE_TEST_ACTIONS actions;

    HOTKEY_STORE store;
    store.Init( actions.List(), false );
    HOTKEY* conflict = nullptr;

    // In the KiCad set, H is highContrastModeCycle's, so another common action cannot take it; Q is the router's own
    store.ResetAllHotkeysToDefault( INTERACTION_PROFILE_ID::STOCK );
    BOOST_CHECK( store.CheckKeyConflicts( &actions.untouched, 'H', &conflict ) );
    BOOST_CHECK( !store.CheckKeyConflicts( &actions.customTrackViaSize, 'Q', &conflict ) );

    // In the Altium-style set H is free, and Q is toggleUnits', which the router's action can no longer take
    store.ResetAllHotkeysToDefault( INTERACTION_PROFILE_ID::ALTIUM );
    BOOST_CHECK( !store.CheckKeyConflicts( &actions.untouched, 'H', &conflict ) );
    BOOST_CHECK( store.CheckKeyConflicts( &actions.customTrackViaSize, 'Q', &conflict ) );
    BOOST_CHECK( conflict && conflict->m_Actions[0] == &actions.toggleUnits );
}


BOOST_AUTO_TEST_CASE( ActionNamesInATableAreChecked )
{
    PROFILE_TEST_ACTIONS actions;
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayerFromText(
            "common.Control.toggleUnits\tQ\ncommon.Control.noSuchAction\tW\npcbnew.other.noSuchAction\tE\n" );

    std::vector<std::string> unregistered =
            INTERACTION_PROFILE::UnregisteredActionNames( layer, actions.List(), { "common." } );

    // Only the prefix asked for is checked, and only a name nothing registers is reported
    BOOST_CHECK( unregistered == std::vector<std::string>( { "common.Control.noSuchAction" } ) );
}


BOOST_AUTO_TEST_CASE( AKeyIsTakenInTheScopeOfTheActionThatBindsIt )
{
    // common binds Q, the schematic editor W, the PCB editor W and E
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayerFromText(
            "common.test.a\tQ\neeschema.test.b\tW\npcbnew.test.c\tW\npcbnew.test.d\tE\n" );

    BOOST_CHECK( layer.m_takenKeysByScope.at( "common." ) == std::set<int>( { 'Q' } ) );
    BOOST_CHECK( layer.m_takenKeysByScope.at( "eeschema." ) == std::set<int>( { 'W' } ) );
    BOOST_CHECK( layer.m_takenKeysByScope.at( "pcbnew." ) == std::set<int>( { 'W', 'E' } ) );
    BOOST_CHECK( layer.m_takenKeys == std::set<int>( { 'Q', 'W', 'E' } ) );

    auto keyOf = [&]( const char* aName, int aKey, const char* aEditor )
    {
        return INTERACTION_PROFILE::ApplyHotKeyLayer( layer, aName, KEYS( aKey, 0 ), aEditor ).first;
    };

    // A key a common action binds is taken in every editor, from every action
    BOOST_CHECK_EQUAL( keyOf( "eeschema.test.x", 'Q', "eeschema." ), 0 );
    BOOST_CHECK_EQUAL( keyOf( "pcbnew.test.x", 'Q', "pcbnew." ), 0 );
    BOOST_CHECK_EQUAL( keyOf( "common.test.x", 'Q', "eeschema." ), 0 );
    BOOST_CHECK_EQUAL( keyOf( "common.test.x", 'Q', "pcbnew." ), 0 );
    BOOST_CHECK_EQUAL( keyOf( "common.test.x", 'Q', "" ), 0 );

    // A key an editor's action binds is taken in that editor only
    BOOST_CHECK_EQUAL( keyOf( "eeschema.test.x", 'W', "eeschema." ), 0 );
    BOOST_CHECK_EQUAL( keyOf( "pcbnew.test.x", 'W', "pcbnew." ), 0 );
    BOOST_CHECK_EQUAL( keyOf( "eeschema.test.x", 'E', "eeschema." ), 'E' ); // only the PCB editor binds E
    BOOST_CHECK_EQUAL( keyOf( "pcbnew.test.x", 'E', "pcbnew." ), 0 );

    // An action that is in every editor loses the keys of the editor it is resolved for
    BOOST_CHECK_EQUAL( keyOf( "common.test.x", 'E', "eeschema." ), 'E' );
    BOOST_CHECK_EQUAL( keyOf( "common.test.x", 'E', "pcbnew." ), 0 );
    BOOST_CHECK_EQUAL( keyOf( "common.test.x", 'W', "eeschema." ), 0 );

    // ... and the keys of every editor when the editor is not known (a hotkey editor that lists several)
    BOOST_CHECK_EQUAL( keyOf( "common.test.x", 'E', "" ), 0 );

    // An action of a program with no table of its own loses only what common takes
    BOOST_CHECK_EQUAL( keyOf( "kicad.test.x", 'W', "" ), 'W' );
    BOOST_CHECK_EQUAL( keyOf( "kicad.test.x", 'Q', "" ), 0 );

    // The table's own binding wins over all of it, and a key nobody binds is untouched
    BOOST_CHECK( INTERACTION_PROFILE::ApplyHotKeyLayer( layer, "pcbnew.test.c", KEYS( 'Q', 0 ), "pcbnew." )
                 == KEYS( 'W', 0 ) );
    BOOST_CHECK_EQUAL( keyOf( "pcbnew.test.x", 'K', "pcbnew." ), 'K' );

    // The alternate key is scoped the same way
    BOOST_CHECK( INTERACTION_PROFILE::ApplyHotKeyLayer( layer, "eeschema.test.x", KEYS( 'W', 'E' ), "eeschema." )
                 == KEYS( 0, 'E' ) );
    BOOST_CHECK( INTERACTION_PROFILE::ApplyHotKeyLayer( layer, "pcbnew.test.x", KEYS( 'E', 'W' ), "pcbnew." )
                 == KEYS( 0, 0 ) );

    // The scope of a name
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::ScopeOf( "eeschema.Sequence.zoom" ), "eeschema." );
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::ScopeOf( "nodots" ), "" );

    // An inactive layer changes nothing in any scope
    BOOST_CHECK( INTERACTION_PROFILE::ApplyHotKeyLayer( PROFILE_HOTKEY_LAYER(), "pcbnew.test.x", KEYS( 'E', 'W' ),
                                                        "pcbnew." )
                 == KEYS( 'E', 'W' ) );
}


BOOST_AUTO_TEST_CASE( TheEditorsOwnKeysAreResolvedPerEditorByTheActionManager )
{
    // The same key means a different action in each editor, because two editor-prefixed actions bind it
    PROFILE_HOTKEY_LAYER layer =
            INTERACTION_PROFILE::BuildHotKeyLayerFromText( "eeschema.test.wire\tCtrl+W\npcbnew.test.track\tCtrl+W\n" );

    TOOL_ACTION stockWire = TOOL_ACTION( TOOL_ACTION_ARGS()
                                                 .Name( "common.test.closeWindow" )
                                                 .FriendlyName( "Close" )
                                                 .Scope( AS_GLOBAL )
                                                 .DefaultHotkey( MD_CTRL + 'W' ) );
    TOOL_ACTION schWire = TOOL_ACTION( TOOL_ACTION_ARGS().Name( "eeschema.test.wire" ).Scope( AS_GLOBAL ) );
    TOOL_ACTION pcbTrack = TOOL_ACTION( TOOL_ACTION_ARGS().Name( "pcbnew.test.track" ).Scope( AS_GLOBAL ) );

    auto keys = [&]( const TOOL_ACTION& aAction, const char* aEditor )
    {
        return ACTION_MANAGER::ResolveHotKey( aAction, std::map<std::string, int>(), layer, PROFILE_HOTKEY_MAP(),
                                              aEditor );
    };

    BOOST_CHECK( keys( schWire, "eeschema." ) == KEYS( MD_CTRL + 'W', 0 ) );
    BOOST_CHECK( keys( pcbTrack, "pcbnew." ) == KEYS( MD_CTRL + 'W', 0 ) );

    // A common action that has Ctrl+W loses it in each editor, because each binds it
    BOOST_CHECK( keys( stockWire, "eeschema." ) == KEYS( 0, 0 ) );
    BOOST_CHECK( keys( stockWire, "pcbnew." ) == KEYS( 0, 0 ) );

    // In an editor that does not bind it, it keeps it
    BOOST_CHECK( keys( stockWire, "gerbview." ) == KEYS( MD_CTRL + 'W', 0 ) );
    BOOST_CHECK( ACTION_MANAGER::EditorScopeOf( KICAD_MAIN_FRAME_T ).empty() );
    BOOST_CHECK_EQUAL( ACTION_MANAGER::EditorScopeOf( FRAME_SCH ), "eeschema." );
    BOOST_CHECK_EQUAL( ACTION_MANAGER::EditorScopeOf( FRAME_PCB_EDITOR ), "pcbnew." );
}


BOOST_AUTO_TEST_CASE( TheKeySequenceHintDelayHasADefaultARangeAndOldFilesLoadTheDefault )
{
    COMMON_SETTINGS settings;

    settings.Load();
    BOOST_CHECK_EQUAL( settings.m_Input.key_sequence_hint_delay_ms, 300 );

    // Round trip, including 0 ("show the menu at once") and the ends of the range
    for( int delay : { 0, 1, 750, 5000 } )
    {
        settings.m_Input.key_sequence_hint_delay_ms = delay;
        settings.Store();
        settings.m_Input.key_sequence_hint_delay_ms = 17;
        settings.Load();
        BOOST_CHECK_EQUAL( settings.m_Input.key_sequence_hint_delay_ms, delay );
    }

    // A file from before the setting existed has no such entry, and a file with a value outside the range has a
    // bad one: both load the default, whatever was in memory
    TEMP_DIRECTORY directory;

    settings.m_Input.key_sequence_hint_delay_ms = 750;
    settings.Store();
    BOOST_REQUIRE( settings.SaveToFile( directory.m_path, true ) );

    wxFileName  fileName( directory.m_path, settings.GetFullFilename() );
    std::string written = readWholeFile( fileName.GetFullPath() ).ToStdString();

    BOOST_REQUIRE_MESSAGE( written.find( "\"key_sequence_hint_delay_ms\": 750" ) != std::string::npos,
                           "the setting is not in the file the settings wrote" );

    auto loadWith = [&]( const std::string& aReplacement )
    {
        std::string text = written;
        size_t      at = text.find( "\"key_sequence_hint_delay_ms\": 750," );

        BOOST_REQUIRE( at != std::string::npos );
        text.replace( at, std::string( "\"key_sequence_hint_delay_ms\": 750," ).size(), aReplacement );

        wxFFile file( fileName.GetFullPath(), wxS( "wb" ) );
        file.Write( text.data(), text.size() );
        file.Close();

        settings.m_Input.key_sequence_hint_delay_ms = 750;
        BOOST_REQUIRE( settings.LoadFromFile( directory.m_path ) );
        return settings.m_Input.key_sequence_hint_delay_ms;
    };

    BOOST_CHECK_EQUAL( loadWith( "" ), 300 );
    BOOST_CHECK_EQUAL( loadWith( "\"key_sequence_hint_delay_ms\": -5," ), 300 );
    BOOST_CHECK_EQUAL( loadWith( "\"key_sequence_hint_delay_ms\": 99999," ), 300 );
    BOOST_CHECK_EQUAL( loadWith( "\"key_sequence_hint_delay_ms\": 0," ), 0 );
    BOOST_CHECK_EQUAL( loadWith( "\"key_sequence_hint_delay_ms\": 750," ), 750 );
}

BOOST_AUTO_TEST_CASE( AContextActionKeepsItsKeyWhereARootTakesItAndAGlobalOneLosesIt )
{
    // F is the root of the File sequences. In a command it is still the router's "finish": a context action of the
    // active tool wins its letter, which is Altium's rule that in-command keys win (note, 5.3)
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayerFromText( "common.Sequence.file\tF\n" );

    TOOL_ACTION finish = TOOL_ACTION( TOOL_ACTION_ARGS()
                                              .Name( "pcbnew.InteractiveRouter.AttemptFinish" )
                                              .Scope( AS_CONTEXT )
                                              .DefaultHotkey( 'F' ) );
    TOOL_ACTION flip = TOOL_ACTION( TOOL_ACTION_ARGS()
                                            .Name( "pcbnew.InteractiveEdit.flip" )
                                            .FriendlyName( "Flip" )
                                            .Scope( AS_GLOBAL )
                                            .DefaultHotkey( 'F' ) );

    auto keys = [&]( const TOOL_ACTION& aAction )
    {
        return ACTION_MANAGER::ResolveHotKey( aAction, std::map<std::string, int>(), layer, PROFILE_HOTKEY_MAP(),
                                              "pcbnew." );
    };

    BOOST_CHECK( keys( finish ) == KEYS( 'F', 0 ) );
    BOOST_CHECK( keys( flip ) == KEYS( 0, 0 ) );

    // The user's own file still wins over both, and a context action the table names gets the table's key
    PROFILE_HOTKEY_LAYER named = INTERACTION_PROFILE::BuildHotKeyLayerFromText(
            "common.Sequence.file\tF\npcbnew.InteractiveRouter.AttemptFinish\tShift+F\n" );

    BOOST_CHECK( ACTION_MANAGER::ResolveHotKey( finish, std::map<std::string, int>(), named, PROFILE_HOTKEY_MAP(),
                                                "pcbnew." )
                 == KEYS( MD_SHIFT + 'F', 0 ) );
}

BOOST_AUTO_TEST_SUITE_END()

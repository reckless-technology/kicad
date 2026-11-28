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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <vector>

#include <wx/defs.h>

#include <hotkeys_basic.h>
#include <interaction_profile.h>
#include <router/pns_routing_settings.h>
#include <tool/action_manager.h>
#include <tool/actions.h>
#include <tool/tool_action.h>
#include <tool/tool_event.h>
#include <tools/pcb_actions.h>

// The routing keys of the Altium-style profile (docs/reckless/interaction-profile.md, 5.8, step 5): the actions that are
// new, the keys they and the router's and the length tuner's own actions have under each profile, which action a key
// reaches with the router or the tuner on the tool stack and which with none, and the look-ahead that is a session's
// choice and not a stored setting.

namespace
{
const char* const ROUTER = "pcbnew.InteractiveRouter";
const char* const DRAWING = "pcbnew.InteractiveDrawing";
const char* const SCOPE = "pcbnew.";

const TOOL_ACTION* findRegistered( const std::string& aName )
{
    const auto& actions = ACTION_MANAGER::GetActionList();
    auto        found = std::find_if( actions.begin(), actions.end(),
                                      [&]( const TOOL_ACTION* aAction )
                                      {
                                   return aAction->GetName() == aName;
                                      } );

    return found == actions.end() ? nullptr : *found;
}


/// The actions each key holds in the PCB editor under a profile's table: what the action manager builds when it
/// updates the hotkeys.
std::map<int, std::list<TOOL_ACTION*>> keysOfPcbEditor( const PROFILE_HOTKEY_LAYER& aLayer )
{
    std::map<int, std::list<TOOL_ACTION*>> byKey;

    for( TOOL_ACTION* action : ACTION_MANAGER::GetActionList() )
    {
        const std::string actionScope = INTERACTION_PROFILE::ScopeOf( action->GetName() );

        if( actionScope != "common." && actionScope != SCOPE )
            continue;

        const std::pair<int, int> keys = ACTION_MANAGER::ResolveHotKey( *action, std::map<std::string, int>(), aLayer,
                                                                        PROFILE_HOTKEY_MAP(), SCOPE );

        for( int key : { keys.first, keys.second } )
        {
            if( key > 0 )
                byKey[key].push_back( action );
        }
    }

    return byKey;
}


/// What a key does with these tools on the stack, lowest first: the action RunHotKey would run (the target of an alias),
/// else the names of the global actions that would be tried, in order.
struct KEY_RESULT
{
    std::string              contextAction; ///< empty when no tool on the stack has one
    std::vector<std::string> globalActions;
};

KEY_RESULT pressKey( const std::map<int, std::list<TOOL_ACTION*>>& aByKey, int aKey,
                     const std::vector<std::string>& aToolStack )
{
    KEY_RESULT result;
    auto       held = aByKey.find( aKey );

    if( held == aByKey.end() )
        return result;

    std::vector<const TOOL_ACTION*> global;

    const TOOL_ACTION* context = ACTION_MANAGER::ChooseContextAction(
            held->second,
            [&]( const TOOL_ACTION& aAction )
            {
                auto onStack = std::find( aToolStack.begin(), aToolStack.end(), aAction.GetToolName() );

                return onStack == aToolStack.end() ? -1 : static_cast<int>( onStack - aToolStack.begin() );
            },
            global );

    if( context )
        result.contextAction = ( context->GetForwardTarget() ? context->GetForwardTarget() : context )->GetName();

    for( const TOOL_ACTION* action : global )
        result.globalActions.push_back( action->GetName() );

    return result;
}

const int NUMPAD_PLUS = WXK_NUMPAD_ADD;
const int NUMPAD_MINUS = WXK_NUMPAD_SUBTRACT;
const int NUMPAD_STAR = WXK_NUMPAD_MULTIPLY;
const int SHIFT_SPACE = static_cast<int>( MD_SHIFT ) + static_cast<int>( WXK_SPACE );
} // namespace


BOOST_AUTO_TEST_SUITE( RoutingKeysProfile )


BOOST_AUTO_TEST_CASE( EveryNewActionIsRegisteredBoundUnderTheProfileAndUnboundInStock )
{
    PROFILE_HOTKEY_LAYER altium = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    struct EXPECTED
    {
        const TOOL_ACTION& action;
        int                key;
        int                alternate;
    };

    const std::vector<EXPECTED> expected = {
        { PCB_ACTIONS::layerNextVisible, NUMPAD_PLUS, '+' },
        { PCB_ACTIONS::layerPrevVisible, NUMPAD_MINUS, '-' },
        { PCB_ACTIONS::layerNextWhileRouting, NUMPAD_PLUS, '+' },
        { PCB_ACTIONS::layerPrevWhileRouting, NUMPAD_MINUS, '-' },
        { PCB_ACTIONS::cycleRouterModeWhileRouting, MD_SHIFT + 'R', 0 },
        { PCB_ACTIONS::trackWidthIncWhileRouting, '3', 0 },
        { PCB_ACTIONS::viaSizeIncWhileRouting, '4', 0 },
        { PCB_ACTIONS::routerToggleLookAhead, '1', 0 },
        { PCB_ACTIONS::routerToggleLoopRemoval, MD_SHIFT + 'D', 0 },
        { PCB_ACTIONS::routerShowTrackWidthMenu, MD_SHIFT + 'W', 0 },
        { PCB_ACTIONS::routerShowViaSizeMenu, MD_SHIFT + 'V', 0 },
        { PCB_ACTIONS::amplDecreaseWhileTuning, ',', 0 },
        { PCB_ACTIONS::amplIncreaseWhileTuning, '.', 0 },
        { PCB_ACTIONS::spacingDecreaseWhileTuning, '3', 0 },
        { PCB_ACTIONS::spacingIncreaseWhileTuning, '4', 0 },
    };

    for( const EXPECTED& entry : expected )
    {
        const std::string& name = entry.action.GetName();

        BOOST_TEST_CONTEXT( name )
        {
            BOOST_CHECK( findRegistered( name ) == &entry.action );

            const std::pair<int, int> profile = ACTION_MANAGER::ResolveHotKey(
                    entry.action, std::map<std::string, int>(), altium, PROFILE_HOTKEY_MAP(), SCOPE );
            const std::pair<int, int> stock = ACTION_MANAGER::ResolveHotKey(
                    entry.action, std::map<std::string, int>(), PROFILE_HOTKEY_LAYER(), PROFILE_HOTKEY_MAP(), SCOPE );

            BOOST_CHECK_EQUAL( profile.first, entry.key );
            BOOST_CHECK_EQUAL( profile.second, entry.alternate );
            BOOST_CHECK_EQUAL( stock.first, 0 );
            BOOST_CHECK_EQUAL( stock.second, 0 );
            BOOST_CHECK_EQUAL( entry.action.GetDefaultHotKey(), 0 );
        }
    }

    // The two router toggles and the two menus are the router's own, never global: idle, their keys mean what they meant
    for( const TOOL_ACTION* routerAction :
         { &PCB_ACTIONS::routerToggleLookAhead, &PCB_ACTIONS::routerToggleLoopRemoval,
           &PCB_ACTIONS::routerShowTrackWidthMenu, &PCB_ACTIONS::routerShowViaSizeMenu } )
    {
        BOOST_CHECK( routerAction->GetScope() == AS_CONTEXT );
        BOOST_CHECK_EQUAL( routerAction->GetToolName(), ROUTER );
    }
}


BOOST_AUTO_TEST_CASE( TheRoutersOwnActionsHaveTheirKeysUnderTheProfile )
{
    PROFILE_HOTKEY_LAYER altium = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    struct EXPECTED
    {
        const char* name;
        int         key;
    };

    // Space, Shift+Space, 2, 9 and Ctrl+L are the keys of actions the router already has, which are context actions;
    // Tab and Backspace and Enter are as they were
    for( const EXPECTED& entry : std::vector<EXPECTED>( {
                 { "pcbnew.InteractiveRouter.SwitchPosture", WXK_SPACE },
                 { "pcbnew.InteractiveRouter.SwitchRoundingToNext", SHIFT_SPACE },
                 { "pcbnew.InteractiveRouter.PlaceVia", '2' },
                 { "pcbnew.InteractiveRouter.ContinueFromEnd", '9' },
                 { "pcbnew.InteractiveRouter.SelLayerAndPlaceVia", MD_CTRL + 'L' },
                 { "pcbnew.InteractiveRouter.CustomTrackViaSize", WXK_TAB },
         } ) )
    {
        const TOOL_ACTION* action = findRegistered( entry.name );

        BOOST_REQUIRE_MESSAGE( action, entry.name );
        BOOST_CHECK( action->GetScope() == AS_CONTEXT );
        BOOST_CHECK_EQUAL( action->GetToolName(), ROUTER );
        BOOST_CHECK_EQUAL( ACTION_MANAGER::ResolveHotKey( *action, std::map<std::string, int>(), altium,
                                                          PROFILE_HOTKEY_MAP(), SCOPE )
                                   .first,
                           entry.key );
    }

    // Backspace undoes the segment and Enter clicks, which the table does not touch: the keys are what stock gives them
    const auto byKey = keysOfPcbEditor( altium );

    BOOST_CHECK_EQUAL( PCB_ACTIONS::routerUndoLastSegment.GetDefaultHotKey(), WXK_BACK );
    BOOST_CHECK_EQUAL( pressKey( byKey, WXK_BACK, { ROUTER } ).contextAction,
                       PCB_ACTIONS::routerUndoLastSegment.GetName() );
    BOOST_CHECK_EQUAL( ACTIONS::cursorClick.GetDefaultHotKey(), WXK_RETURN );

    const KEY_RESULT enter = pressKey( byKey, WXK_RETURN, { ROUTER } );

    BOOST_CHECK( enter.contextAction.empty() );
    BOOST_CHECK( std::find( enter.globalActions.begin(), enter.globalActions.end(), ACTIONS::cursorClick.GetName() )
                 != enter.globalActions.end() );
}


BOOST_AUTO_TEST_CASE( EachRoutingKeyReachesItsActionWhileTheRouterIsOnTheStack )
{
    const auto byKey = keysOfPcbEditor( INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM ) );

    struct EXPECTED
    {
        int         key;
        const char* action;
    };

    // The aliases run the global action they forward to; the router's own actions run themselves
    for( const EXPECTED& entry : std::vector<EXPECTED>( {
                 { WXK_SPACE, "pcbnew.InteractiveRouter.SwitchPosture" },
                 { SHIFT_SPACE, "pcbnew.InteractiveRouter.SwitchRoundingToNext" },
                 { MD_SHIFT + 'R', "pcbnew.InteractiveRouter.CycleRouterMode" },
                 { '3', "pcbnew.EditorControl.trackWidthInc" },
                 { '4', "pcbnew.EditorControl.viaSizeInc" },
                 { '1', "pcbnew.InteractiveRouter.ToggleLookAhead" },
                 { MD_SHIFT + 'D', "pcbnew.InteractiveRouter.ToggleLoopRemoval" },
                 { '9', "pcbnew.InteractiveRouter.ContinueFromEnd" },
                 { '2', "pcbnew.InteractiveRouter.PlaceVia" },
                 { MD_SHIFT + 'W', "pcbnew.InteractiveRouter.ShowTrackWidthMenu" },
                 { MD_SHIFT + 'V', "pcbnew.InteractiveRouter.ShowViaSizeMenu" },
                 { MD_CTRL + 'L', "pcbnew.InteractiveRouter.SelLayerAndPlaceVia" },
                 { NUMPAD_PLUS, "pcbnew.Control.layerNext" },
                 { '+', "pcbnew.Control.layerNext" },
                 { NUMPAD_MINUS, "pcbnew.Control.layerPrev" },
                 { '-', "pcbnew.Control.layerPrev" },
                 { WXK_TAB, "pcbnew.InteractiveRouter.CustomTrackViaSize" },
         } ) )
    {
        BOOST_TEST_CONTEXT( KeyNameFromKeyCode( entry.key ) )
        {
            const KEY_RESULT result = pressKey( byKey, entry.key, { ROUTER } );

            BOOST_CHECK_EQUAL( result.contextAction, entry.action );
        }
    }

    // The numpad star is the global layer-next action itself, which the router handles with a via
    const KEY_RESULT star = pressKey( byKey, NUMPAD_STAR, { ROUTER } );

    BOOST_CHECK( star.contextAction.empty() );
    BOOST_CHECK( std::find( star.globalActions.begin(), star.globalActions.end(), PCB_ACTIONS::layerNext.GetName() )
                 != star.globalActions.end() );
}


BOOST_AUTO_TEST_CASE( EachRoutingKeyIsWhatItWasIdleWithNoCommandRunning )
{
    const auto byKey = keysOfPcbEditor( INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM ) );

    auto idleGlobals = [&]( int aKey )
    {
        return pressKey( byKey, aKey, {} );
    };

    auto isAmong = []( const KEY_RESULT& aResult, const std::string& aName )
    {
        return std::find( aResult.globalActions.begin(), aResult.globalActions.end(), aName )
               != aResult.globalActions.end();
    };

    // 3 is the 3D viewer, and nothing else
    KEY_RESULT three = idleGlobals( '3' );

    BOOST_CHECK( three.contextAction.empty() );
    BOOST_CHECK_EQUAL( three.globalActions.size(), 1u );
    BOOST_CHECK( isAmong( three, ACTIONS::show3DViewer.GetName() ) );

    // The keys the router took keep what they did: rotate, the width and the layer pair, the tuner's settings and the
    // skew tuner; the tuner's own keys are gone from the global actions
    BOOST_CHECK( isAmong( idleGlobals( MD_SHIFT + 'R' ), PCB_ACTIONS::rotateCw.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( MD_SHIFT + 'W' ), PCB_ACTIONS::trackWidthDec.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( MD_SHIFT + 'V' ), PCB_ACTIONS::layerPairPresetsCycle.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( MD_CTRL + 'L' ), PCB_ACTIONS::lengthTunerSettings.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( '9' ), PCB_ACTIONS::tuneSkew.GetName() ) );

    for( int key : std::vector<int>{ '1', '2', '4', ',', '.', MD_SHIFT + 'D' } )
    {
        BOOST_TEST_CONTEXT( KeyNameFromKeyCode( key ) )
        {
            const KEY_RESULT result = idleGlobals( key );

            BOOST_CHECK( result.contextAction.empty() );
            BOOST_CHECK( result.globalActions.empty() );
        }
    }

    // Idle, plus and minus walk every visible layer; the star walks the copper layers
    BOOST_CHECK( isAmong( idleGlobals( NUMPAD_PLUS ), PCB_ACTIONS::layerNextVisible.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( '+' ), PCB_ACTIONS::layerNextVisible.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( NUMPAD_MINUS ), PCB_ACTIONS::layerPrevVisible.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( '-' ), PCB_ACTIONS::layerPrevVisible.GetName() ) );
    BOOST_CHECK( !isAmong( idleGlobals( NUMPAD_PLUS ), PCB_ACTIONS::layerNext.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( NUMPAD_STAR ), PCB_ACTIONS::layerNext.GetName() ) );
    BOOST_CHECK( isAmong( idleGlobals( MD_SHIFT + NUMPAD_STAR ), PCB_ACTIONS::layerPrev.GetName() ) );
}


BOOST_AUTO_TEST_CASE( TheLengthTunerKeysReachTheTunerWhileTheDrawingToolIsOnTheStack )
{
    const auto byKey = keysOfPcbEditor( INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM ) );

    struct EXPECTED
    {
        int         key;
        const char* action;
    };

    // Comma and period step the amplitude down and up, 3 and 4 the spacing down and up
    for( const EXPECTED& entry : std::vector<EXPECTED>( {
                 { ',', "pcbnew.lengthTuner.AmplDecrease" },
                 { '.', "pcbnew.lengthTuner.AmplIncrease" },
                 { '3', "pcbnew.lengthTuner.SpacingDecrease" },
                 { '4', "pcbnew.lengthTuner.SpacingIncrease" },
         } ) )
    {
        BOOST_TEST_CONTEXT( KeyNameFromKeyCode( entry.key ) )
        {
            BOOST_CHECK_EQUAL( pressKey( byKey, entry.key, { DRAWING } ).contextAction, entry.action );
        }
    }

    // The router above the drawing tool on the stack wins its own 3 and 4
    BOOST_CHECK_EQUAL( pressKey( byKey, '3', { DRAWING, ROUTER } ).contextAction,
                       PCB_ACTIONS::trackWidthInc.GetName() );
    BOOST_CHECK_EQUAL( pressKey( byKey, '3', { ROUTER, DRAWING } ).contextAction,
                       PCB_ACTIONS::spacingDecrease.GetName() );

    // The tuner's stock actions lose their keys to the table: they are reached by the aliases only
    for( const TOOL_ACTION* stockAction : { &PCB_ACTIONS::spacingIncrease, &PCB_ACTIONS::spacingDecrease,
                                            &PCB_ACTIONS::amplIncrease, &PCB_ACTIONS::amplDecrease } )
    {
        BOOST_CHECK_MESSAGE( stockAction->GetDefaultHotKey() > 0, stockAction->GetName() << " has a stock key" );

        const std::pair<int, int> keys = ACTION_MANAGER::ResolveHotKey(
                *stockAction, std::map<std::string, int>(),
                INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM ), PROFILE_HOTKEY_MAP(), SCOPE );

        BOOST_CHECK_MESSAGE( keys.first == 0 && keys.second == 0, stockAction->GetName() << " keeps a key" );
    }

    // KiCad's tuner has no corner or miter action, so Altium's 1 and 2 of the tuner have nothing to run
    for( const TOOL_ACTION* action : ACTION_MANAGER::GetActionList() )
    {
        const std::string& name = action->GetName();

        if( name.rfind( "pcbnew.lengthTuner.", 0 ) == 0 )
        {
            BOOST_CHECK_MESSAGE( name.find( "iter" ) == std::string::npos && name.find( "orner" ) == std::string::npos,
                                 name << " is a corner or miter action" );
        }
    }
}


BOOST_AUTO_TEST_CASE( TheStockProfileBindsNoRoutingKeyOfTheTable )
{
    const auto byKey = keysOfPcbEditor( PROFILE_HOTKEY_LAYER() );

    // With the router on the stack, stock KiCad's own router keys are all there is: none of the aliases and toggles is
    // reached by a key, and the tuner's stock keys are global
    for( const char* name :
         { "pcbnew.InteractiveRouter.cycleRouterModeWhileRouting", "pcbnew.InteractiveRouter.trackWidthIncWhileRouting",
           "pcbnew.InteractiveRouter.viaSizeIncWhileRouting", "pcbnew.InteractiveRouter.layerNextWhileRouting",
           "pcbnew.InteractiveRouter.layerPrevWhileRouting", "pcbnew.InteractiveRouter.ToggleLookAhead",
           "pcbnew.InteractiveRouter.ToggleLoopRemoval", "pcbnew.InteractiveRouter.ShowTrackWidthMenu",
           "pcbnew.InteractiveRouter.ShowViaSizeMenu", "pcbnew.InteractiveDrawing.amplDecreaseWhileTuning",
           "pcbnew.InteractiveDrawing.amplIncreaseWhileTuning", "pcbnew.InteractiveDrawing.spacingDecreaseWhileTuning",
           "pcbnew.InteractiveDrawing.spacingIncreaseWhileTuning", "pcbnew.Control.layerNextVisible",
           "pcbnew.Control.layerPrevVisible" } )
    {
        for( const auto& [key, actions] : byKey )
        {
            for( const TOOL_ACTION* action : actions )
                BOOST_CHECK_MESSAGE( action->GetName() != name, name << " has the key " << KeyNameFromKeyCode( key ) );
        }
    }

    // Stock still has its own: the tuner's four keys, global, and a plus that steps through copper only
    BOOST_CHECK_EQUAL( pressKey( byKey, '3', { ROUTER } ).globalActions.size(), 1u );
    BOOST_CHECK_EQUAL( pressKey( byKey, '3', { ROUTER } ).globalActions.front(), PCB_ACTIONS::amplIncrease.GetName() );
    BOOST_CHECK_EQUAL( pressKey( byKey, '+', {} ).globalActions.front(), PCB_ACTIONS::layerNext.GetName() );
}


BOOST_AUTO_TEST_CASE( LookAheadIsASessionsChoiceAndTheStoredSettingDoesNotMove )
{
    PNS::ROUTING_SETTINGS settings( nullptr, "" );

    // The stored default is the one the owner routes with: a click fixes everything up to the cursor
    BOOST_CHECK( settings.GetStoredFixAllSegments() );
    BOOST_CHECK( settings.GetFixAllSegments() );
    BOOST_CHECK( !settings.IsFixAllSegmentsSessionChoice() );

    const std::string before = settings.FormatAsString();

    settings.ToggleSessionFixAllSegments();

    BOOST_CHECK( !settings.GetFixAllSegments() );
    BOOST_CHECK( settings.IsFixAllSegmentsSessionChoice() );
    BOOST_CHECK( settings.GetStoredFixAllSegments() );

    // What would be written to the settings file is what it was
    BOOST_CHECK_EQUAL( settings.FormatAsString(), before );
    BOOST_CHECK( settings.FormatAsString().find( "\"fix_all_segments\": true" ) != std::string::npos );

    settings.ToggleSessionFixAllSegments();

    BOOST_CHECK( settings.GetFixAllSegments() );
    BOOST_CHECK( settings.GetStoredFixAllSegments() );

    // The stored setting, when it is changed on purpose, is what is in force again
    settings.ToggleSessionFixAllSegments();
    settings.SetFixAllSegments( false );

    BOOST_CHECK( !settings.GetFixAllSegments() );
    BOOST_CHECK( !settings.GetStoredFixAllSegments() );
    BOOST_CHECK( !settings.IsFixAllSegmentsSessionChoice() );
    BOOST_CHECK( settings.FormatAsString().find( "\"fix_all_segments\": false" ) != std::string::npos );

    // The loop removal is the stored setting itself
    BOOST_CHECK( settings.RemoveLoops() );
    settings.SetRemoveLoops( !settings.RemoveLoops() );
    BOOST_CHECK( !settings.RemoveLoops() );
}


BOOST_AUTO_TEST_SUITE_END()

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
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <3d_enums.h>
#include <3d_rendering/track_ball.h>
#include <3d_viewer/tools/eda_3d_actions.h>
#include <interaction_profile.h>
#include <pcbnew_settings.h>
#include <settings/json_settings_internals.h>
#include <tool/action_manager.h>
#include <tool/actions.h>
#include <tool/tool_action.h>

// The 3D viewer's part of the Altium-style profile (docs/reckless/interaction-profile.md, 5.10, step 10): its actions,
// their keys under each profile, the camera's two new presets, and the live refresh setting.

namespace
{
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


void expectNear( const glm::mat4& aActual, const glm::mat4& aExpected, float aTolerance )
{
    for( int column = 0; column < 4; ++column )
    {
        for( int row = 0; row < 4; ++row )
            BOOST_CHECK_SMALL( aActual[column][row] - aExpected[column][row], aTolerance );
    }
}


/// A camera that has been turned about, so that a preset is not tested from the pose it starts at.
TRACK_BALL makeTurnedCamera()
{
    TRACK_BALL camera( 2.0f );

    camera.RotateX( 0.4f );
    camera.RotateY( -0.3f );
    camera.RotateZ( 1.1f );

    return camera;
}


/// The rotation the camera ends with when a view command runs to its end.
glm::mat4 rotationAfter( TRACK_BALL& aCamera, VIEW3D_TYPE aView )
{
    aCamera.SetT0_and_T1_current_T();
    BOOST_REQUIRE( aCamera.ViewCommand_T1( aView ) );
    aCamera.Interpolate( 1.0f );

    return aCamera.GetRotationMatrix();
}
} // namespace


BOOST_AUTO_TEST_SUITE( Viewer3DProfile )


BOOST_AUTO_TEST_CASE( TheNewActionsAreRegisteredAndUnboundInStock )
{
    for( const TOOL_ACTION* action :
         { &EDA_3D_ACTIONS::showPcbEditor, &EDA_3D_ACTIONS::rotateZ90, &EDA_3D_ACTIONS::viewIsometric } )
    {
        BOOST_CHECK_MESSAGE( findRegistered( action->GetName() ) == action, action->GetName() << " is not registered" );

        // Stock KiCad gives them no key: they are reached from the menus, or by a key the user binds
        BOOST_CHECK_EQUAL( action->GetDefaultHotKey(), 0 );
        BOOST_CHECK_EQUAL( action->GetDefaultHotKeyAlt(), 0 );
    }

    // The keys that the profile binds belong to existing actions, which keep the stock keys they had
    BOOST_CHECK_EQUAL( ACTIONS::show3DViewer.GetDefaultHotKey(), MD_ALT + '3' );
    BOOST_CHECK_EQUAL( EDA_3D_ACTIONS::viewTop.GetDefaultHotKey(), 'Z' );
    BOOST_CHECK_EQUAL( EDA_3D_ACTIONS::flipView.GetDefaultHotKey(), 'F' );
}


BOOST_AUTO_TEST_CASE( TheProfileBindsThemAndTheStockProfileDoesNot )
{
    PROFILE_HOTKEY_LAYER altium = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    struct EXPECTATION
    {
        const TOOL_ACTION* action;
        int                key;
    };

    const EXPECTATION expectations[] = { { &ACTIONS::show3DViewer, '3' },
                                         { &EDA_3D_ACTIONS::showPcbEditor, '2' },
                                         { &EDA_3D_ACTIONS::viewTop, '0' },
                                         { &EDA_3D_ACTIONS::rotateZ90, '9' },
                                         { &EDA_3D_ACTIONS::viewIsometric, '8' },
                                         { &EDA_3D_ACTIONS::flipView, MD_CTRL + 'F' } };

    for( const EXPECTATION& expectation : expectations )
    {
        const std::string name = expectation.action->GetName();

        auto binding = altium.m_bindings.find( name );

        BOOST_REQUIRE_MESSAGE( binding != altium.m_bindings.end(), name );
        BOOST_CHECK_EQUAL( binding->second.first, expectation.key );

        // Resolved the way the action manager does, in the viewer (no editor scope) under the profile
        std::pair<int, int> resolved = INTERACTION_PROFILE::ApplyHotKeyLayer(
                altium, name, { expectation.action->GetDefaultHotKey(), 0 }, std::string(), false );

        BOOST_CHECK_EQUAL( resolved.first, expectation.key );

        // ... and under the stock profile it is the compiled key
        PROFILE_HOTKEY_LAYER stock = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::STOCK );

        resolved = INTERACTION_PROFILE::ApplyHotKeyLayer( stock, name, { expectation.action->GetDefaultHotKey(), 0 },
                                                          std::string(), false );

        BOOST_CHECK_EQUAL( resolved.first, expectation.action->GetDefaultHotKey() );
    }
}


BOOST_AUTO_TEST_CASE( PageUpAndPageDownZoomTheViewerUnderTheProfile )
{
    PROFILE_HOTKEY_LAYER altium = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    // The viewer runs the common zoom actions, which the table gives PgUp and PgDn in every editor
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::ApplyHotKeyLayer( altium, ACTIONS::zoomIn.GetName(),
                                                              { ACTIONS::zoomIn.GetDefaultHotKey(), 0 }, std::string(),
                                                              false )
                               .first,
                       static_cast<int>( WXK_PAGEUP ) );
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::ApplyHotKeyLayer( altium, ACTIONS::zoomOut.GetName(),
                                                              { ACTIONS::zoomOut.GetDefaultHotKey(), 0 },
                                                              std::string(), false )
                               .first,
                       static_cast<int>( WXK_PAGEDOWN ) );
}


BOOST_AUTO_TEST_CASE( ThreeLeavesTheRoutersAndTheTunersInCommandKeysToStepFive )
{
    PROFILE_HOTKEY_LAYER altium = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    // `3` is the show-3D-viewer key in every editor, so it is taken from an action of the PCB editor that holds it by
    // default; the only one is the length tuner's amplitude key, which is an in-command key and is step 5's. The key
    // is gated by MayRunIdleKey, so with a command running it runs nothing at all.
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::ApplyHotKeyLayer( altium, "pcbnew.lengthTuner.AmplIncrease", { '3', 0 },
                                                              "pcbnew.", false )
                               .first,
                       0 );

    // `2`, the viewer's key for the editor, is taken in the viewer's scope only: the tuner's `2` is untouched
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::ApplyHotKeyLayer( altium, "pcbnew.lengthTuner.SpacingDecrease", { '2', 0 },
                                                              "pcbnew.", false )
                               .first,
                       '2' );

    // A context alias is never stripped of its key: whatever step 5 binds to 3 for the router wins over the idle key
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::ApplyHotKeyLayer( altium, "pcbnew.InteractiveRouter.someAlias", { '3', 0 },
                                                              "pcbnew.", true )
                               .first,
                       '3' );
}


BOOST_AUTO_TEST_CASE( AQuarterTurnIsExactlyNinetyDegreesAboutZ )
{
    TRACK_BALL camera = makeTurnedCamera();

    const glm::mat4 before = camera.GetRotationMatrix();
    const glm::mat4 after = rotationAfter( camera, VIEW3D_TYPE::VIEW3D_ROTATE_Z_90 );

    // The board is turned about its own Z, not by the configured rotation increment
    expectNear( after, before * glm::rotate( glm::mat4( 1.0f ), -glm::half_pi<float>(), glm::vec3( 0, 0, 1 ) ), 1e-5f );
    BOOST_CHECK_CLOSE( CAMERA::QuarterTurnAboutZ(), -glm::half_pi<float>(), 1e-4 );

    // Four of them are a whole turn
    for( int i = 0; i < 3; ++i )
        rotationAfter( camera, VIEW3D_TYPE::VIEW3D_ROTATE_Z_90 );

    expectNear( camera.GetRotationMatrix(), before, 1e-4f );
}


BOOST_AUTO_TEST_CASE( TheIsometricViewForeshortensAllThreeAxesEqually )
{
    TRACK_BALL fresh( 2.0f );
    TRACK_BALL turned = makeTurnedCamera();

    const glm::mat4 fromFresh = rotationAfter( fresh, VIEW3D_TYPE::VIEW3D_ISOMETRIC );
    const glm::mat4 fromTurned = rotationAfter( turned, VIEW3D_TYPE::VIEW3D_ISOMETRIC );

    // The preset is a place to be, not a turn from where the camera was
    expectNear( fromTurned, fromFresh, 1e-4f );

    // Every board axis makes the same angle with the line of sight: a z of 1 over the root of three
    const float expectedDepth = 1.0f / std::sqrt( 3.0f );

    for( const glm::vec3& axis : { glm::vec3( 1, 0, 0 ), glm::vec3( 0, 1, 0 ), glm::vec3( 0, 0, 1 ) } )
    {
        const glm::vec4 viewed = fromFresh * glm::vec4( axis, 0.0f );

        BOOST_CHECK_SMALL( std::abs( viewed.z ) - expectedDepth, 1e-4f );
    }

    // Seen from above: the board's top faces the viewer
    BOOST_CHECK_GT( ( fromFresh * glm::vec4( 0, 0, 1, 0 ) ).z, 0.0f );

    // And the preset sits between the front view and the right view, 45 degrees from each about Z
    const SFVEC3F isometric = CAMERA::IsometricRotation();

    BOOST_CHECK_CLOSE( isometric.z, -glm::quarter_pi<float>(), 1e-4 );
    BOOST_CHECK_SMALL( isometric.y, 1e-6f );
    BOOST_CHECK_CLOSE( isometric.x, -( glm::half_pi<float>() - std::atan( 1.0f / std::sqrt( 2.0f ) ) ), 1e-4 );
}


BOOST_AUTO_TEST_CASE( TheLiveRefreshSettingStartsFollowingTheProfile )
{
    PCBNEW_SETTINGS settings;

    // An int that follows the profile and not a bool whose default would have to
    BOOST_CHECK_EQUAL( settings.m_Display.m_Live3DRefreshMode, -1 );

    // It is stored under its own key, beside the older switch, which is still written for the stock profile
    settings.Store();

    BOOST_CHECK_EQUAL( settings.Internals()->Get<int>( "pcb_display.live_3d_refresh_mode" ), -1 );
    BOOST_CHECK_EQUAL( settings.Internals()->Get<bool>( "pcb_display.live_3d_refresh" ), false );

    settings.m_Display.m_Live3DRefreshMode = 0;
    settings.Store();

    BOOST_CHECK_EQUAL( settings.Internals()->Get<int>( "pcb_display.live_3d_refresh_mode" ), 0 );
}


BOOST_AUTO_TEST_SUITE_END()

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

#include <map>

#include <wx/defs.h>

#include <interaction_profile.h>
#include <tool/tool_event.h>

// The profile's decisions about the 3D viewer (docs/reckless/interaction-profile.md, 5.10, step 10), as functions of
// the profile and the inputs: what the live refresh does, whether a close hides the viewer, which key may run while a
// command runs, and what a mouse drag does to the camera.

BOOST_AUTO_TEST_SUITE( InteractionProfile3DViewer )

BOOST_AUTO_TEST_CASE( LiveRefreshFollowsTheProfileUntilItIsSet )
{
    // -1 follows the profile: on in the Altium-style one; in the stock one it is what the older switch says
    BOOST_CHECK( INTERACTION_PROFILE::IsLive3DRefreshOn( -1, false, INTERACTION_PROFILE_ID::ALTIUM ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsLive3DRefreshOn( -1, true, INTERACTION_PROFILE_ID::ALTIUM ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsLive3DRefreshOn( -1, false, INTERACTION_PROFILE_ID::STOCK ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsLive3DRefreshOn( -1, true, INTERACTION_PROFILE_ID::STOCK ) );

    // 0 and 1 are the user's, in either profile, whatever the older switch says
    for( INTERACTION_PROFILE_ID profile : { INTERACTION_PROFILE_ID::STOCK, INTERACTION_PROFILE_ID::ALTIUM } )
    {
        for( bool legacy : { false, true } )
        {
            BOOST_CHECK( !INTERACTION_PROFILE::IsLive3DRefreshOn( 0, legacy, profile ) );
            BOOST_CHECK( INTERACTION_PROFILE::IsLive3DRefreshOn( 1, legacy, profile ) );
        }
    }
}


BOOST_AUTO_TEST_CASE( OnlyTheUserClosingTheViewerHidesIt )
{
    // The user's close can be vetoed; the editor's, the project's and the program's are forced
    BOOST_CHECK( INTERACTION_PROFILE::ShouldHide3DViewerOnClose( INTERACTION_PROFILE_ID::ALTIUM, true ) );
    BOOST_CHECK( !INTERACTION_PROFILE::ShouldHide3DViewerOnClose( INTERACTION_PROFILE_ID::ALTIUM, false ) );

    // The stock profile destroys it either way
    BOOST_CHECK( !INTERACTION_PROFILE::ShouldHide3DViewerOnClose( INTERACTION_PROFILE_ID::STOCK, true ) );
    BOOST_CHECK( !INTERACTION_PROFILE::ShouldHide3DViewerOnClose( INTERACTION_PROFILE_ID::STOCK, false ) );
}


BOOST_AUTO_TEST_CASE( AnIdleKeyDoesNotRunInsideACommandUnderTheProfile )
{
    BOOST_CHECK( INTERACTION_PROFILE::MayRunIdleKey( INTERACTION_PROFILE_ID::ALTIUM, true ) );
    BOOST_CHECK( !INTERACTION_PROFILE::MayRunIdleKey( INTERACTION_PROFILE_ID::ALTIUM, false ) );

    // The stock profile never limits a key this way
    BOOST_CHECK( INTERACTION_PROFILE::MayRunIdleKey( INTERACTION_PROFILE_ID::STOCK, true ) );
    BOOST_CHECK( INTERACTION_PROFILE::MayRunIdleKey( INTERACTION_PROFILE_ID::STOCK, false ) );
}


BOOST_AUTO_TEST_CASE( TheMouseButtonsChooseTheCameraMode )
{
    using MODE = CAMERA_DRAG_MODE;
    const INTERACTION_PROFILE_ID stock = INTERACTION_PROFILE_ID::STOCK;
    const INTERACTION_PROFILE_ID altium = INTERACTION_PROFILE_ID::ALTIUM;

    //                                                profile, left, middle, right, shift, middle pans, right pans
    // Left-drag orbits in both profiles, with or without Shift
    for( INTERACTION_PROFILE_ID profile : { stock, altium } )
    {
        for( bool shift : { false, true } )
            BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( profile, true, false, false, shift, false, true )
                         == MODE::ORBIT );
    }

    // Right-drag pans where the setting says so, and Shift+right-drag orbits only in the Altium-style profile
    BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( altium, false, false, true, false, false, true ) == MODE::PAN );
    BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( altium, false, false, true, true, false, true )
                 == MODE::ORBIT );
    BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( stock, false, false, true, false, false, true ) == MODE::PAN );
    BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( stock, false, false, true, true, false, true ) == MODE::PAN );

    // A right button that is set to do nothing does nothing, with or without Shift in the stock profile
    BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( stock, false, false, true, false, false, false )
                 == MODE::NONE );
    BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( stock, false, false, true, true, false, false )
                 == MODE::NONE );

    // The middle button follows its own setting; Shift changes nothing for it
    for( INTERACTION_PROFILE_ID profile : { stock, altium } )
    {
        BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( profile, false, true, false, false, true, false )
                     == MODE::PAN );
        BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( profile, false, true, false, true, true, false )
                     == MODE::PAN );
        BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( profile, false, true, false, false, false, false )
                     == MODE::NONE );
    }

    // No button, no drag
    BOOST_CHECK( INTERACTION_PROFILE::Camera3DDragMode( altium, false, false, false, true, true, true )
                 == MODE::NONE );
}


BOOST_AUTO_TEST_CASE( TheAltiumTableBindsTheThreeDKeys )
{
    PROFILE_HOTKEY_LAYER altium = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );
    PROFILE_HOTKEY_LAYER stock = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::STOCK );

    const std::map<std::string, int> expected = {
        { "common.Control.show3DViewer", '3' },        { "3DViewer.Control.showPcbEditor", '2' },
        { "3DViewer.Control.viewTop", '0' },           { "3DViewer.Control.rotateZ90", '9' },
        { "3DViewer.Control.viewIsometric", '8' },     { "3DViewer.Control.flipView", MD_CTRL + 'F' },
    };

    for( const auto& [name, key] : expected )
    {
        auto binding = altium.m_bindings.find( name );

        BOOST_REQUIRE_MESSAGE( binding != altium.m_bindings.end(), name << " is not in the Altium-style table" );
        BOOST_CHECK_EQUAL( binding->second.first, key );

        // The stock profile has no table, so none of them is bound there
        BOOST_CHECK_MESSAGE( stock.m_bindings.find( name ) == stock.m_bindings.end(), name << " is bound in stock" );
    }

    // PgUp and PgDn zoom the viewer through the common zoom actions the table already binds
    BOOST_CHECK_EQUAL( altium.m_bindings.at( "common.Control.zoomIn" ).first, static_cast<int>( WXK_PAGEUP ) );
    BOOST_CHECK_EQUAL( altium.m_bindings.at( "common.Control.zoomOut" ).first, static_cast<int>( WXK_PAGEDOWN ) );

    // The viewer's keys are taken in the viewer's scope only, so no editor loses a key to them
    // (9 is not checked for the PCB editor: the router takes it for Continue From Other End, 5.8, and the skew tuner has
    // it bound again)
    BOOST_CHECK( altium.m_takenKeysByScope.at( "3DViewer." ).count( '9' ) );
    BOOST_CHECK( !altium.m_takenKeysByScope.count( "pcbnew." )
                 || !altium.m_takenKeysByScope.at( "pcbnew." ).count( '8' ) );
}

BOOST_AUTO_TEST_SUITE_END()

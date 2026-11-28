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

#include <interaction_profile.h>

// The profile's decisions about routing (docs/reckless/interaction-profile.md, 5.8, step 5), as functions of the profile
// and the inputs: whether "use starting track width" is on, what the project stores for it, and which click attempts to
// finish a route.

BOOST_AUTO_TEST_SUITE( InteractionProfileRouting )

BOOST_AUTO_TEST_CASE( UseStartingTrackWidthFollowsTheProfileUntilItIsSet )
{
    // -1 follows the profile: on in the Altium-style one; in the stock one it is what the older switch says
    BOOST_CHECK( INTERACTION_PROFILE::IsAutoTrackWidthOn( -1, false, INTERACTION_PROFILE_ID::ALTIUM ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsAutoTrackWidthOn( -1, true, INTERACTION_PROFILE_ID::ALTIUM ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsAutoTrackWidthOn( -1, false, INTERACTION_PROFILE_ID::STOCK ) );
    BOOST_CHECK( INTERACTION_PROFILE::IsAutoTrackWidthOn( -1, true, INTERACTION_PROFILE_ID::STOCK ) );

    // 0 and 1 are the user's, in either profile, whatever the older switch says
    for( INTERACTION_PROFILE_ID profile : { INTERACTION_PROFILE_ID::STOCK, INTERACTION_PROFILE_ID::ALTIUM } )
    {
        for( bool legacy : { false, true } )
        {
            BOOST_CHECK( !INTERACTION_PROFILE::IsAutoTrackWidthOn( 0, legacy, profile ) );
            BOOST_CHECK( INTERACTION_PROFILE::IsAutoTrackWidthOn( 1, legacy, profile ) );
        }
    }

    // Any other value follows the profile too
    BOOST_CHECK( INTERACTION_PROFILE::IsAutoTrackWidthOn( 7, false, INTERACTION_PROFILE_ID::ALTIUM ) );
    BOOST_CHECK( !INTERACTION_PROFILE::IsAutoTrackWidthOn( -3, false, INTERACTION_PROFILE_ID::STOCK ) );
}


BOOST_AUTO_TEST_CASE( AProjectThatIsOnlyOpenedKeepsFollowingTheProfile )
{
    for( INTERACTION_PROFILE_ID profile : { INTERACTION_PROFILE_ID::STOCK, INTERACTION_PROFILE_ID::ALTIUM } )
    {
        for( bool legacy : { false, true } )
        {
            // What the profile gave is what is on, so nothing is stored but the following
            const bool isOn = INTERACTION_PROFILE::IsAutoTrackWidthOn( -1, legacy, profile );

            BOOST_CHECK_EQUAL( INTERACTION_PROFILE::AutoTrackWidthSettingToStore( -1, isOn, legacy, profile ), -1 );

            // The user changed it: the choice is stored, whichever way it went
            BOOST_CHECK_EQUAL( INTERACTION_PROFILE::AutoTrackWidthSettingToStore( -1, !isOn, legacy, profile ),
                               !isOn ? 1 : 0 );
        }
    }

    // A choice stays a choice, even when it is what the profile would give
    BOOST_CHECK_EQUAL(
            INTERACTION_PROFILE::AutoTrackWidthSettingToStore( 1, true, false, INTERACTION_PROFILE_ID::ALTIUM ), 1 );
    BOOST_CHECK_EQUAL(
            INTERACTION_PROFILE::AutoTrackWidthSettingToStore( 0, false, false, INTERACTION_PROFILE_ID::STOCK ), 0 );
    BOOST_CHECK_EQUAL(
            INTERACTION_PROFILE::AutoTrackWidthSettingToStore( 0, true, true, INTERACTION_PROFILE_ID::ALTIUM ), 1 );
}


BOOST_AUTO_TEST_CASE( OnlyControlClickAttemptsToFinishUnderTheAltiumStyleProfile )
{
    BOOST_CHECK( INTERACTION_PROFILE::DoesClickAttemptFinish( INTERACTION_PROFILE_ID::ALTIUM, true, false, false ) );

    // Not without Ctrl, and not with another modifier beside it
    BOOST_CHECK( !INTERACTION_PROFILE::DoesClickAttemptFinish( INTERACTION_PROFILE_ID::ALTIUM, false, false, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::DoesClickAttemptFinish( INTERACTION_PROFILE_ID::ALTIUM, true, true, false ) );
    BOOST_CHECK( !INTERACTION_PROFILE::DoesClickAttemptFinish( INTERACTION_PROFILE_ID::ALTIUM, true, false, true ) );

    // Stock KiCad never does
    BOOST_CHECK( !INTERACTION_PROFILE::DoesClickAttemptFinish( INTERACTION_PROFILE_ID::STOCK, true, false, false ) );
}

BOOST_AUTO_TEST_SUITE_END()

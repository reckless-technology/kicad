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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <pcbnew_settings.h>
#include <widgets/pcb_layer_tab_bar_model.h>

namespace
{
LSET fourLayerBoard()
{
    return LSET::AllNonCuMask() | LSET( { F_Cu, In1_Cu, In2_Cu, B_Cu } );
}
} // namespace


BOOST_AUTO_TEST_SUITE( LayerTabBarModel )


BOOST_AUTO_TEST_CASE( CopperComesFirstFromTopToBottom )
{
    const LSET enabled = fourLayerBoard();

    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( enabled, enabled );

    BOOST_REQUIRE_GE( order.size(), 4u );
    BOOST_CHECK_EQUAL( order[0], F_Cu );
    BOOST_CHECK_EQUAL( order[1], In1_Cu );
    BOOST_CHECK_EQUAL( order[2], In2_Cu );
    BOOST_CHECK_EQUAL( order[3], B_Cu );
}


BOOST_AUTO_TEST_CASE( TechnicalLayersFollowInPairs )
{
    const LSET enabled = fourLayerBoard();

    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( enabled, enabled );

    const std::vector<PCB_LAYER_ID> expected = { F_SilkS, B_SilkS, F_Mask,  B_Mask, F_Paste, B_Paste,   F_Adhes,
                                                 B_Adhes, F_CrtYd, B_CrtYd, F_Fab,  B_Fab,   Edge_Cuts, Margin };

    BOOST_REQUIRE_GE( order.size(), 4 + expected.size() );
    BOOST_CHECK_EQUAL_COLLECTIONS( order.begin() + 4, order.begin() + 4 + expected.size(), expected.begin(),
                                   expected.end() );
}


BOOST_AUTO_TEST_CASE( UserLayersCloseTheList )
{
    const LSET enabled = fourLayerBoard();

    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( enabled, enabled );

    // Every enabled layer but Rescue has exactly one tab, and the user layers come after Margin
    BOOST_CHECK_EQUAL( order.size() + 1, enabled.count() ); // all but the Rescue layer
    BOOST_CHECK( std::find( order.begin(), order.end(), Rescue ) == order.end() );

    const auto margin = std::find( order.begin(), order.end(), Margin );
    BOOST_REQUIRE( margin != order.end() );

    for( auto it = margin + 1; it != order.end(); ++it )
    {
        const bool isUserLayer = ( *it == Dwgs_User || *it == Cmts_User || *it == Eco1_User || *it == Eco2_User
                                   || ( *it >= User_1 && *it <= User_9 ) || LSET::UserDefinedLayersMask().test( *it ) );
        BOOST_CHECK_MESSAGE( isUserLayer, "layer " << static_cast<int>( *it ) << " is not a user layer" );
    }
}


BOOST_AUTO_TEST_CASE( HiddenAndDisabledLayersGetNoTab )
{
    const LSET enabled = fourLayerBoard();
    LSET       visible = enabled;

    visible.reset( F_SilkS ).reset( In1_Cu );
    visible.set( In10_Cu ); // marked visible, but the board has no such layer

    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( visible, enabled );

    BOOST_CHECK( std::find( order.begin(), order.end(), F_SilkS ) == order.end() );
    BOOST_CHECK( std::find( order.begin(), order.end(), In1_Cu ) == order.end() );
    BOOST_CHECK( std::find( order.begin(), order.end(), In10_Cu ) == order.end() );
    BOOST_CHECK_EQUAL( order[1], In2_Cu );
}


BOOST_AUTO_TEST_CASE( NothingVisibleMeansNoTabs )
{
    BOOST_CHECK( LayerTabOrder( LSET(), fourLayerBoard() ).empty() );
}


BOOST_AUTO_TEST_CASE( TwoLayerBoardHasOnlyTheOuterCopper )
{
    const LSET enabled = LSET::AllNonCuMask() | LSET( { F_Cu, B_Cu } );

    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( enabled, enabled );

    BOOST_REQUIRE_GE( order.size(), 2u );
    BOOST_CHECK_EQUAL( order[0], F_Cu );
    BOOST_CHECK_EQUAL( order[1], B_Cu );
    BOOST_CHECK_EQUAL( order[2], F_SilkS );
}


BOOST_AUTO_TEST_CASE( SettingFollowsTheProfile )
{
    BOOST_CHECK( IsLayerTabBarShown( LAYER_TAB_BAR_FOLLOW_PROFILE, INTERACTION_PROFILE_ID::ALTIUM ) );
    BOOST_CHECK( !IsLayerTabBarShown( LAYER_TAB_BAR_FOLLOW_PROFILE, INTERACTION_PROFILE_ID::STOCK ) );
}


BOOST_AUTO_TEST_CASE( StoredSettingWinsOverTheProfile )
{
    for( INTERACTION_PROFILE_ID profile : { INTERACTION_PROFILE_ID::STOCK, INTERACTION_PROFILE_ID::ALTIUM } )
    {
        BOOST_CHECK( IsLayerTabBarShown( LAYER_TAB_BAR_ON, profile ) );
        BOOST_CHECK( !IsLayerTabBarShown( LAYER_TAB_BAR_OFF, profile ) );
    }
}


BOOST_AUTO_TEST_CASE( UnknownSettingFollowsTheProfile )
{
    BOOST_CHECK( IsLayerTabBarShown( 7, INTERACTION_PROFILE_ID::ALTIUM ) );
    BOOST_CHECK( !IsLayerTabBarShown( 7, INTERACTION_PROFILE_ID::STOCK ) );
}


BOOST_AUTO_TEST_CASE( DefaultSettingIsToFollowTheProfile )
{
    PCBNEW_SETTINGS settings;

    BOOST_CHECK_EQUAL( settings.m_LayerTabBarMode, LAYER_TAB_BAR_FOLLOW_PROFILE );
}


BOOST_AUTO_TEST_CASE( ToggleStoresTheOppositeExplicitly )
{
    // On under the Altium-style profile by default, so the toggle stores off
    BOOST_CHECK_EQUAL( ToggledLayerTabBarSetting( LAYER_TAB_BAR_FOLLOW_PROFILE, INTERACTION_PROFILE_ID::ALTIUM ),
                       LAYER_TAB_BAR_OFF );

    // Off under stock KiCad by default, so the toggle stores on
    BOOST_CHECK_EQUAL( ToggledLayerTabBarSetting( LAYER_TAB_BAR_FOLLOW_PROFILE, INTERACTION_PROFILE_ID::STOCK ),
                       LAYER_TAB_BAR_ON );

    BOOST_CHECK_EQUAL( ToggledLayerTabBarSetting( LAYER_TAB_BAR_ON, INTERACTION_PROFILE_ID::STOCK ),
                       LAYER_TAB_BAR_OFF );
    BOOST_CHECK_EQUAL( ToggledLayerTabBarSetting( LAYER_TAB_BAR_OFF, INTERACTION_PROFILE_ID::ALTIUM ),
                       LAYER_TAB_BAR_ON );
}


BOOST_AUTO_TEST_CASE( HideThisLayerHidesOnlyThatLayer )
{
    const LSET enabled = fourLayerBoard();

    const LSET result = ApplyLayerTabCommand( LAYER_TAB_COMMAND::HIDE_THIS_LAYER, F_Mask, enabled, enabled );

    BOOST_CHECK( !result.test( F_Mask ) );
    BOOST_CHECK_EQUAL( result.count() + 1, enabled.count() );
}


BOOST_AUTO_TEST_CASE( ShowAllLayersShowsEveryEnabledLayerAndNoOther )
{
    const LSET enabled = fourLayerBoard();
    LSET       visible = LSET( { F_Cu } );
    visible.set( In10_Cu );

    const LSET result = ApplyLayerTabCommand( LAYER_TAB_COMMAND::SHOW_ALL_LAYERS, UNDEFINED_LAYER, visible, enabled );

    BOOST_CHECK( result.test( B_Cu ) );
    BOOST_CHECK( result.test( F_SilkS ) );
    BOOST_CHECK( ( result & enabled ) == enabled );
}


BOOST_AUTO_TEST_CASE( ShowOnlyCopperLayersLeavesTheEnabledCopper )
{
    const LSET enabled = fourLayerBoard();

    const LSET result =
            ApplyLayerTabCommand( LAYER_TAB_COMMAND::SHOW_ONLY_COPPER_LAYERS, UNDEFINED_LAYER, enabled, enabled );

    BOOST_CHECK( result == LSET( { F_Cu, In1_Cu, In2_Cu, B_Cu } ) );
}


BOOST_AUTO_TEST_CASE( HidingTheActiveLayerActivatesTheNextTab )
{
    const LSET enabled = fourLayerBoard();

    BOOST_CHECK_EQUAL( LayerToActivateAfterHiding( F_Cu, enabled, enabled ), In1_Cu );
    BOOST_CHECK_EQUAL( LayerToActivateAfterHiding( In2_Cu, enabled, enabled ), B_Cu );
}


BOOST_AUTO_TEST_CASE( HidingTheLastTabActivatesTheOneBefore )
{
    const LSET                      enabled = fourLayerBoard();
    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( enabled, enabled );

    const PCB_LAYER_ID last = order.back();
    const PCB_LAYER_ID before = order[order.size() - 2];

    BOOST_CHECK_EQUAL( LayerToActivateAfterHiding( last, enabled, enabled ), before );
}


BOOST_AUTO_TEST_CASE( HidingTheOnlyTabLeavesNothingToActivate )
{
    const LSET enabled = fourLayerBoard();

    BOOST_CHECK_EQUAL( LayerToActivateAfterHiding( F_Cu, LSET( { F_Cu } ), enabled ), UNDEFINED_LAYER );
}


BOOST_AUTO_TEST_CASE( TheLayerKeysWalkEveryVisibleTabAndSkipTheHiddenOnes )
{
    const LSET enabled = fourLayerBoard();
    LSET       visible = enabled;

    // Every layer is in the bar: the walk is the bar's order, wrapping at the ends
    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( visible, enabled );

    BOOST_REQUIRE_GT( order.size(), 6u );

    for( size_t i = 0; i < order.size(); ++i )
    {
        BOOST_CHECK_EQUAL( SteppedVisibleLayer( order[i], 1, visible, enabled ), order[( i + 1 ) % order.size()] );
        BOOST_CHECK_EQUAL( SteppedVisibleLayer( order[i], -1, visible, enabled ),
                           order[( i + order.size() - 1 ) % order.size()] );
    }

    // The walk goes through the technical layers, not copper only
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( B_Cu, 1, visible, enabled ), F_SilkS );

    // A hidden layer is stepped over, in both directions
    visible.reset( F_SilkS );
    visible.reset( In1_Cu );

    BOOST_CHECK_EQUAL( SteppedVisibleLayer( B_Cu, 1, visible, enabled ), B_SilkS );
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( F_Cu, 1, visible, enabled ), In2_Cu );
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( In2_Cu, -1, visible, enabled ), F_Cu );
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( B_SilkS, -1, visible, enabled ), B_Cu );

    // The first tab follows the last one, and the last precedes the first
    const std::vector<PCB_LAYER_ID> shown = LayerTabOrder( visible, enabled );

    BOOST_CHECK_EQUAL( SteppedVisibleLayer( shown.back(), 1, visible, enabled ), shown.front() );
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( shown.front(), -1, visible, enabled ), shown.back() );

    // A layer the board does not have is never reached, however it is marked
    BOOST_CHECK( !LayerTabOrder( visible | LSET( { In5_Cu } ), enabled ).empty() );
    BOOST_CHECK_NE( SteppedVisibleLayer( B_Cu, 1, visible | LSET( { In5_Cu } ), enabled ), In5_Cu );
}


BOOST_AUTO_TEST_CASE( TheLayerKeysFromAHiddenLayerReachTheEndsAndNothingVisibleReachesNothing )
{
    const LSET enabled = fourLayerBoard();
    LSET       visible = enabled;

    visible.reset( In1_Cu );

    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( visible, enabled );

    // The active layer is hidden: forward goes to the first tab, back to the last
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( In1_Cu, 1, visible, enabled ), order.front() );
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( In1_Cu, -1, visible, enabled ), order.back() );

    // One tab steps to itself
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( F_Cu, 1, LSET( { F_Cu } ), enabled ), F_Cu );
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( F_Cu, -1, LSET( { F_Cu } ), enabled ), F_Cu );

    // None visible
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( F_Cu, 1, LSET(), enabled ), UNDEFINED_LAYER );
    BOOST_CHECK_EQUAL( SteppedVisibleLayer( F_Cu, -1, LSET(), enabled ), UNDEFINED_LAYER );
}


BOOST_AUTO_TEST_SUITE_END()

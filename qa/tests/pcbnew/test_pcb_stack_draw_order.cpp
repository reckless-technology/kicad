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

#include <algorithm>
#include <vector>

#include <pcb_stack_draw_order.h>

namespace
{
const INTERACTION_PROFILE_ID altiumProfile = INTERACTION_PROFILE_ID::ALTIUM;
const INTERACTION_PROFILE_ID stockProfile = INTERACTION_PROFILE_ID::STOCK;

std::vector<PCB_LAYER_ID> fromAbove( int aCopperLayerCount )
{
    return StackLayersNearestFirst( aCopperLayerCount, false );
}

/// The position of a view layer in an order, or -1
int positionOf( const std::vector<int>& aOrder, int aViewLayer )
{
    const auto found = std::find( aOrder.begin(), aOrder.end(), aViewLayer );
    return found == aOrder.end() ? -1 : static_cast<int>( found - aOrder.begin() );
}
} // namespace


BOOST_AUTO_TEST_SUITE( PcbStackDrawOrder )


BOOST_AUTO_TEST_CASE( TwoLayerStackFromAbove )
{
    const std::vector<PCB_LAYER_ID> expected = { F_CrtYd, F_Fab,   F_SilkS, F_Mask, F_Paste, F_Adhes, F_Cu,
                                                 B_Cu,    B_Adhes, B_Paste, B_Mask, B_SilkS, B_Fab,   B_CrtYd };

    BOOST_CHECK( fromAbove( 2 ) == expected );
}


BOOST_AUTO_TEST_CASE( FourAndSixLayerStacksHaveTheInnerLayersInOrder )
{
    const std::vector<PCB_LAYER_ID> four = fromAbove( 4 );
    const std::vector<PCB_LAYER_ID> six = fromAbove( 6 );

    BOOST_CHECK_EQUAL( four.size(), 16u );
    BOOST_CHECK_EQUAL( six.size(), 18u );

    const std::vector<PCB_LAYER_ID> fourCopper( four.begin() + 6, four.begin() + 10 );
    const std::vector<PCB_LAYER_ID> sixCopper( six.begin() + 6, six.begin() + 12 );

    BOOST_CHECK( fourCopper == ( std::vector<PCB_LAYER_ID>{ F_Cu, In1_Cu, In2_Cu, B_Cu } ) );
    BOOST_CHECK( sixCopper == ( std::vector<PCB_LAYER_ID>{ F_Cu, In1_Cu, In2_Cu, In3_Cu, In4_Cu, B_Cu } ) );
}


BOOST_AUTO_TEST_CASE( FlippedIsTheReverse )
{
    for( int copperLayerCount : { 2, 4, 6 } )
    {
        std::vector<PCB_LAYER_ID> reversed = fromAbove( copperLayerCount );
        std::reverse( reversed.begin(), reversed.end() );

        BOOST_CHECK( StackLayersNearestFirst( copperLayerCount, true ) == reversed );
    }

    const std::vector<PCB_LAYER_ID> flippedFour = StackLayersNearestFirst( 4, true );

    BOOST_CHECK_EQUAL( flippedFour.front(), B_CrtYd );
    BOOST_CHECK_EQUAL( flippedFour[9], F_Cu );
    BOOST_CHECK_EQUAL( flippedFour.back(), F_CrtYd );
}


BOOST_AUTO_TEST_CASE( BoardOutlineAndUserLayersAreNotInTheStack )
{
    for( PCB_LAYER_ID layer : fromAbove( 4 ) )
    {
        BOOST_CHECK( layer != Edge_Cuts );
        BOOST_CHECK( layer != Margin );
        BOOST_CHECK( layer != Dwgs_User );
    }
}


BOOST_AUTO_TEST_CASE( GroupsHoldEachLayersSublayers )
{
    const std::vector<int> technical = StackLayerGroup( F_SilkS );

    BOOST_CHECK( technical == ( std::vector<int>{ F_SilkS, ZONE_LAYER_FOR( F_SilkS ) } ) );

    const std::vector<int> inner = StackLayerGroup( In1_Cu );

    BOOST_CHECK_EQUAL( inner.size(), 7u );
    BOOST_CHECK( positionOf( inner, In1_Cu ) >= 0 );
    BOOST_CHECK( positionOf( inner, ZONE_LAYER_FOR( In1_Cu ) ) >= 0 );
    BOOST_CHECK( positionOf( inner, PAD_COPPER_LAYER_FOR( In1_Cu ) ) >= 0 );
    BOOST_CHECK( positionOf( inner, VIA_COPPER_LAYER_FOR( In1_Cu ) ) >= 0 );
    BOOST_CHECK( positionOf( inner, CLEARANCE_LAYER_FOR( In1_Cu ) ) >= 0 );

    // The outer layers carry their pad net names; the front point layer stays with the overlays
    BOOST_CHECK( positionOf( StackLayerGroup( F_Cu ), LAYER_PAD_FR_NETNAMES ) >= 0 );
    BOOST_CHECK( positionOf( StackLayerGroup( B_Cu ), LAYER_PAD_BK_NETNAMES ) >= 0 );
    BOOST_CHECK_EQUAL( positionOf( StackLayerGroup( F_Cu ), POINT_LAYER_FOR( F_Cu ) ), -1 );
}


BOOST_AUTO_TEST_CASE( StackOrderDrawsNearestLast )
{
    for( int copperLayerCount : { 2, 4, 6 } )
    {
        for( bool isFlipped : { false, true } )
        {
            // An active layer outside the stack leaves the stack order untouched
            const std::vector<int> order =
                    StackDrawOrder( copperLayerCount, Edge_Cuts, isFlipped, STACK_DRAW_ORDER_ON, stockProfile );

            const std::vector<PCB_LAYER_ID> stack = StackLayersNearestFirst( copperLayerCount, isFlipped );

            // Layers nearer the viewer come earlier, which is on top
            for( size_t i = 0; i + 1 < stack.size(); ++i )
            {
                BOOST_CHECK_LT( positionOf( order, stack[i] ), positionOf( order, stack[i + 1] ) );
                BOOST_CHECK_LT( positionOf( order, ZONE_LAYER_FOR( stack[i] ) ),
                                positionOf( order, ZONE_LAYER_FOR( stack[i + 1] ) ) );
            }
        }
    }
}


BOOST_AUTO_TEST_CASE( ActiveLayerAndItsSublayersAreFirst )
{
    struct CASE
    {
        int          copperLayerCount;
        PCB_LAYER_ID active;
        bool         isFlipped;
    };

    const CASE cases[] = { { 2, F_Cu, false },   { 2, B_Cu, true },    { 4, In1_Cu, false }, { 4, In2_Cu, true },
                           { 4, B_Cu, false },   { 6, In3_Cu, false }, { 6, In4_Cu, true },  { 6, F_SilkS, false },
                           { 6, B_SilkS, true }, { 4, B_Mask, false }, { 4, F_Fab, true } };

    for( const CASE& test : cases )
    {
        const std::vector<int> order = StackDrawOrder( test.copperLayerCount, test.active, test.isFlipped,
                                                       STACK_DRAW_ORDER_ON, altiumProfile );
        const std::vector<int> group = StackLayerGroup( test.active );

        BOOST_TEST_CONTEXT( "layers " << test.copperLayerCount << " active " << test.active << " flipped "
                                      << test.isFlipped )
        {
            // The group is the first thing in the order, in its own order
            BOOST_REQUIRE_GE( order.size(), group.size() );
            BOOST_CHECK( std::equal( group.begin(), group.end(), order.begin() ) );

            // Every layer of the stack appears exactly once
            size_t expectedSize = 0;

            for( PCB_LAYER_ID layer : StackLayersNearestFirst( test.copperLayerCount, test.isFlipped ) )
                expectedSize += StackLayerGroup( layer ).size();

            BOOST_CHECK_EQUAL( order.size(), expectedSize );

            std::vector<int> sorted = order;
            std::sort( sorted.begin(), sorted.end() );
            BOOST_CHECK( std::adjacent_find( sorted.begin(), sorted.end() ) == sorted.end() );
        }
    }
}


BOOST_AUTO_TEST_CASE( TheRestKeepStackOrderBehindTheActiveLayer )
{
    // From above with In1.Cu active: In1, then F.CrtYd ... F.Cu, then In2 and the rest
    const std::vector<int> order = StackDrawOrder( 4, In1_Cu, false, STACK_DRAW_ORDER_ON, altiumProfile );

    BOOST_CHECK_LT( positionOf( order, In1_Cu ), positionOf( order, F_CrtYd ) );
    BOOST_CHECK_LT( positionOf( order, F_Cu ), positionOf( order, In2_Cu ) );
    BOOST_CHECK_LT( positionOf( order, In2_Cu ), positionOf( order, B_Cu ) );
    BOOST_CHECK_LT( positionOf( order, B_Cu ), positionOf( order, B_CrtYd ) );

    // Flipped with B.Cu active: B.Cu, B technical layers, then In2, In1, F.Cu
    const std::vector<int> flipped = StackDrawOrder( 4, B_Cu, true, STACK_DRAW_ORDER_ON, altiumProfile );

    BOOST_CHECK_LT( positionOf( flipped, B_Cu ), positionOf( flipped, B_CrtYd ) );
    BOOST_CHECK_LT( positionOf( flipped, B_Adhes ), positionOf( flipped, In2_Cu ) );
    BOOST_CHECK_LT( positionOf( flipped, In2_Cu ), positionOf( flipped, In1_Cu ) );
    BOOST_CHECK_LT( positionOf( flipped, In1_Cu ), positionOf( flipped, F_Cu ) );
    BOOST_CHECK_LT( positionOf( flipped, F_Cu ), positionOf( flipped, F_CrtYd ) );
}


BOOST_AUTO_TEST_CASE( StockProfileKeepsKiCadsOrder )
{
    // Following the profile, stock KiCad gets no order at all, so the canvas keeps its fixed table
    BOOST_CHECK( StackDrawOrder( 4, F_Cu, false, STACK_DRAW_ORDER_FOLLOW_PROFILE, stockProfile ).empty() );
    BOOST_CHECK( StackDrawOrder( 4, B_Cu, true, STACK_DRAW_ORDER_FOLLOW_PROFILE, stockProfile ).empty() );
    BOOST_CHECK( !StackDrawOrder( 4, F_Cu, false, STACK_DRAW_ORDER_FOLLOW_PROFILE, altiumProfile ).empty() );
}


BOOST_AUTO_TEST_CASE( SettingIsTriState )
{
    BOOST_CHECK( IsStackDrawOrderOn( STACK_DRAW_ORDER_FOLLOW_PROFILE, altiumProfile ) );
    BOOST_CHECK( !IsStackDrawOrderOn( STACK_DRAW_ORDER_FOLLOW_PROFILE, stockProfile ) );

    BOOST_CHECK( !IsStackDrawOrderOn( STACK_DRAW_ORDER_OFF, altiumProfile ) );
    BOOST_CHECK( !IsStackDrawOrderOn( STACK_DRAW_ORDER_OFF, stockProfile ) );

    BOOST_CHECK( IsStackDrawOrderOn( STACK_DRAW_ORDER_ON, altiumProfile ) );
    BOOST_CHECK( IsStackDrawOrderOn( STACK_DRAW_ORDER_ON, stockProfile ) );

    // A stored value that is none of the three follows the profile
    BOOST_CHECK( IsStackDrawOrderOn( 7, altiumProfile ) );
    BOOST_CHECK( !IsStackDrawOrderOn( 7, stockProfile ) );

    BOOST_CHECK( StackDrawOrder( 4, F_Cu, false, STACK_DRAW_ORDER_OFF, altiumProfile ).empty() );
    BOOST_CHECK( !StackDrawOrder( 4, F_Cu, false, STACK_DRAW_ORDER_ON, stockProfile ).empty() );
}


BOOST_AUTO_TEST_SUITE_END()

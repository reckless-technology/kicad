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

#include "pcb_stack_draw_order.h"

#include <algorithm>

#include <lseq.h>
#include <lset.h>


bool IsStackDrawOrderOn( int aSetting, INTERACTION_PROFILE_ID aProfile )
{
    if( aSetting == STACK_DRAW_ORDER_OFF )
        return false;

    if( aSetting == STACK_DRAW_ORDER_ON )
        return true;

    return aProfile == INTERACTION_PROFILE_ID::ALTIUM;
}


std::vector<PCB_LAYER_ID> StackLayersNearestFirst( int aCopperLayerCount, bool aIsFlipped )
{
    // Seen from above, the documentation layers sit furthest out on each side, then silkscreen, mask, paste and adhesive
    // close to the copper
    static const PCB_LAYER_ID frontTechnicalLayers[] = { F_CrtYd, F_Fab, F_SilkS, F_Mask, F_Paste, F_Adhes };
    static const PCB_LAYER_ID backTechnicalLayers[] = { B_Adhes, B_Paste, B_Mask, B_SilkS, B_Fab, B_CrtYd };

    std::vector<PCB_LAYER_ID> stack( std::begin( frontTechnicalLayers ), std::end( frontTechnicalLayers ) );

    for( PCB_LAYER_ID layer : LSET::AllCuMask( std::max( aCopperLayerCount, 2 ) ).CuStack() )
        stack.push_back( layer );

    stack.insert( stack.end(), std::begin( backTechnicalLayers ), std::end( backTechnicalLayers ) );

    if( aIsFlipped )
        std::reverse( stack.begin(), stack.end() );

    return stack;
}


std::vector<int> StackLayerGroup( PCB_LAYER_ID aLayer )
{
    if( !IsCopperLayer( aLayer ) )
        return { aLayer, ZONE_LAYER_FOR( aLayer ) };

    std::vector<int> group;

    // The outer layers' pad net names draw just above them
    if( aLayer == F_Cu )
        group.push_back( LAYER_PAD_FR_NETNAMES );
    else if( aLayer == B_Cu )
        group.push_back( LAYER_PAD_BK_NETNAMES );

    group.push_back( NETNAMES_LAYER_INDEX( aLayer ) );
    group.push_back( PAD_COPPER_LAYER_FOR( aLayer ) );
    group.push_back( VIA_COPPER_LAYER_FOR( aLayer ) );
    group.push_back( CLEARANCE_LAYER_FOR( aLayer ) );

    // The front copper's point layer is drawn with the overlays, where KiCad keeps it
    if( aLayer != F_Cu )
        group.push_back( POINT_LAYER_FOR( aLayer ) );

    group.push_back( aLayer );
    group.push_back( ZONE_LAYER_FOR( aLayer ) );
    return group;
}


std::vector<int> StackDrawOrder( int aCopperLayerCount, PCB_LAYER_ID aActiveLayer, bool aIsFlipped, int aSetting,
                                 INTERACTION_PROFILE_ID aProfile )
{
    if( !IsStackDrawOrderOn( aSetting, aProfile ) )
        return {};

    const std::vector<PCB_LAYER_ID> stack = StackLayersNearestFirst( aCopperLayerCount, aIsFlipped );
    std::vector<int>                order;

    auto addGroup = [&]( PCB_LAYER_ID aLayer )
    {
        for( int viewLayer : StackLayerGroup( aLayer ) )
            order.push_back( viewLayer );
    };

    if( std::find( stack.begin(), stack.end(), aActiveLayer ) != stack.end() )
        addGroup( aActiveLayer );

    for( PCB_LAYER_ID layer : stack )
    {
        if( layer != aActiveLayer )
            addGroup( layer );
    }

    return order;
}

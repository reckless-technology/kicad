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

#include "pcb_layer_tab_bar_model.h"

#include <algorithm>


std::vector<PCB_LAYER_ID> LayerTabOrder( const LSET& aVisible, const LSET& aEnabled )
{
    const LSET shown = aVisible & aEnabled;

    std::vector<PCB_LAYER_ID> order;

    auto addIfShown = [&]( PCB_LAYER_ID aLayer )
    {
        if( shown.test( aLayer ) )
            order.push_back( aLayer );
    };

    for( PCB_LAYER_ID layer : shown.CuStack() )
        order.push_back( layer );

    static const PCB_LAYER_ID pairedTechnicalLayers[] = { F_SilkS, B_SilkS, F_Mask,  B_Mask,  F_Paste, B_Paste,
                                                          F_Adhes, B_Adhes, F_CrtYd, B_CrtYd, F_Fab,   B_Fab };

    for( PCB_LAYER_ID layer : pairedTechnicalLayers )
        addIfShown( layer );

    addIfShown( Edge_Cuts );
    addIfShown( Margin );

    // Everything else is a user layer; layer order keeps them in the order the Appearance panel lists them
    for( PCB_LAYER_ID layer : shown.Seq() )
    {
        // Rescue holds items of a rescued file and has no row in the Appearance panel either
        if( layer != Rescue && std::find( order.begin(), order.end(), layer ) == order.end() )
            order.push_back( layer );
    }

    return order;
}


bool IsLayerTabBarShown( int aSetting, INTERACTION_PROFILE_ID aProfile )
{
    if( aSetting == LAYER_TAB_BAR_OFF )
        return false;

    if( aSetting == LAYER_TAB_BAR_ON )
        return true;

    return aProfile == INTERACTION_PROFILE_ID::ALTIUM;
}


int ToggledLayerTabBarSetting( int aSetting, INTERACTION_PROFILE_ID aProfile )
{
    return IsLayerTabBarShown( aSetting, aProfile ) ? LAYER_TAB_BAR_OFF : LAYER_TAB_BAR_ON;
}


LSET ApplyLayerTabCommand( LAYER_TAB_COMMAND aCommand, PCB_LAYER_ID aLayer, const LSET& aVisible, const LSET& aEnabled )
{
    switch( aCommand )
    {
    case LAYER_TAB_COMMAND::HIDE_THIS_LAYER:
    {
        LSET visible = aVisible;
        visible.reset( aLayer );
        return visible;
    }

    case LAYER_TAB_COMMAND::SHOW_ALL_LAYERS: return aVisible | aEnabled;

    case LAYER_TAB_COMMAND::SHOW_ONLY_COPPER_LAYERS: return aEnabled & LSET::AllCuMask();
    }

    return aVisible;
}


PCB_LAYER_ID LayerToActivateAfterHiding( PCB_LAYER_ID aHidden, const LSET& aVisible, const LSET& aEnabled )
{
    const std::vector<PCB_LAYER_ID> before = LayerTabOrder( aVisible, aEnabled );
    const std::vector<PCB_LAYER_ID> after = LayerTabOrder(
            ApplyLayerTabCommand( LAYER_TAB_COMMAND::HIDE_THIS_LAYER, aHidden, aVisible, aEnabled ), aEnabled );

    if( after.empty() )
        return UNDEFINED_LAYER;

    const auto position = std::find( before.begin(), before.end(), aHidden );

    // After hiding, the tab that followed the hidden one sits at the hidden one's index
    const size_t index = ( position == before.end() ) ? 0 : static_cast<size_t>( position - before.begin() );

    return after[std::min( index, after.size() - 1 )];
}


PCB_LAYER_ID SteppedVisibleLayer( PCB_LAYER_ID aActive, int aStep, const LSET& aVisible, const LSET& aEnabled )
{
    const std::vector<PCB_LAYER_ID> order = LayerTabOrder( aVisible, aEnabled );

    if( order.empty() )
        return UNDEFINED_LAYER;

    const auto position = std::find( order.begin(), order.end(), aActive );
    const int  count = static_cast<int>( order.size() );

    if( position == order.end() )
        return aStep >= 0 ? order.front() : order.back();

    const int index = static_cast<int>( position - order.begin() );

    // The sum is kept positive so that the remainder is too
    return order[( index + ( aStep >= 0 ? 1 : count - 1 ) ) % count];
}

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

#include "pcb_heads_up_model.h"

#include <algorithm>

#include <wx/translation.h>

#include <base_screen.h>
#include <base_units.h>
#include <board_item.h>
#include <eda_units.h>
#include <layer_ids.h>
#include <math/util.h>
#include <core/typeinfo.h>
#include <board_connected_item.h>
#include <footprint.h>
#include <pad.h>
#include <pcb_track.h>
#include <view/view.h>
#include <zone.h>

#include <router/pns_routing_settings.h>
#include <router/pns_sizes_settings.h>


bool IsHeadsUpShown( int aSetting, INTERACTION_PROFILE_ID aProfile )
{
    if( aSetting == HEADS_UP_OFF )
        return false;

    if( aSetting == HEADS_UP_ON )
        return true;

    return aProfile == INTERACTION_PROFILE_ID::ALTIUM;
}


int ToggledHeadsUpSetting( int aSetting, INTERACTION_PROFILE_ID aProfile )
{
    return IsHeadsUpShown( aSetting, aProfile ) ? HEADS_UP_OFF : HEADS_UP_ON;
}


HEADS_UP_WIDTH_SOURCE HeadsUpWidthSource( bool aIsCustomSizeInUse, bool aIsNetClassWidthInUse )
{
    if( aIsCustomSizeInUse )
        return HEADS_UP_WIDTH_SOURCE::CUSTOM;

    return aIsNetClassWidthInUse ? HEADS_UP_WIDTH_SOURCE::NET_CLASS : HEADS_UP_WIDTH_SOURCE::USER;
}


HEADS_UP_ROUTE HeadsUpRouteFromRouter( const PNS::ROUTING_SETTINGS& aSettings, const PNS::SIZES_SETTINGS& aSizes,
                                       bool aIsDiffPair, HEADS_UP_WIDTH_SOURCE aWidthSource, const wxString& aLayerFrom,
                                       const wxString& aLayerTo, long long aNetLength )
{
    HEADS_UP_ROUTE route;

    if( aSettings.GetFreeAngleMode() )
    {
        route.cornerMode = HEADS_UP_CORNER_MODE::FREE_ANGLE;
    }
    else
    {
        switch( aSettings.GetCornerMode() )
        {
        case DIRECTION_45::CORNER_MODE::MITERED_45: route.cornerMode = HEADS_UP_CORNER_MODE::MITERED_45; break;
        case DIRECTION_45::CORNER_MODE::ROUNDED_45: route.cornerMode = HEADS_UP_CORNER_MODE::ROUNDED_45; break;
        case DIRECTION_45::CORNER_MODE::MITERED_90: route.cornerMode = HEADS_UP_CORNER_MODE::MITERED_90; break;
        case DIRECTION_45::CORNER_MODE::ROUNDED_90: route.cornerMode = HEADS_UP_CORNER_MODE::ROUNDED_90; break;
        default: break;
        }
    }

    switch( aSettings.Mode() )
    {
    case PNS::PNS_MODE::RM_MarkObstacles: route.routerMode = HEADS_UP_ROUTER_MODE::HIGHLIGHT_COLLISIONS; break;
    case PNS::PNS_MODE::RM_Walkaround: route.routerMode = HEADS_UP_ROUTER_MODE::WALK_AROUND; break;
    case PNS::PNS_MODE::RM_Shove: route.routerMode = HEADS_UP_ROUTER_MODE::SHOVE; break;
    default: break;
    }

    route.trackWidth = aIsDiffPair ? aSizes.DiffPairWidth() : aSizes.TrackWidth();
    route.widthSource = aWidthSource;
    route.viaDiameter = aSizes.ViaDiameter();
    route.viaDrill = aSizes.ViaDrill();
    route.layerFrom = aLayerFrom;
    route.layerTo = aLayerTo;
    route.netLength = aNetLength;

    return route;
}


namespace
{
wxString formatDistance( EDA_UNITS aUnits, double aValue, bool aAddUnitLabel )
{
    return EDA_UNIT_UTILS::UI::MessageTextFromValue( pcbIUScale, aUnits, aValue, aAddUnitLabel );
}
} // namespace


wxString BuildHeadsUpRouteLine( const HEADS_UP_ROUTE& aRoute, EDA_UNITS aUnits )
{
    wxString corner;

    switch( aRoute.cornerMode )
    {
    case HEADS_UP_CORNER_MODE::FREE_ANGLE: corner = _( "Free-angle" ); break;
    case HEADS_UP_CORNER_MODE::MITERED_45: corner = _( "45-degree" ); break;
    case HEADS_UP_CORNER_MODE::ROUNDED_45: corner = _( "45-degree rounded" ); break;
    case HEADS_UP_CORNER_MODE::MITERED_90: corner = _( "90-degree" ); break;
    case HEADS_UP_CORNER_MODE::ROUNDED_90: corner = _( "90-degree rounded" ); break;
    }

    wxString mode;

    switch( aRoute.routerMode )
    {
    case HEADS_UP_ROUTER_MODE::WALK_AROUND: mode = _( "Walk around" ); break;
    case HEADS_UP_ROUTER_MODE::SHOVE: mode = _( "Shove" ); break;
    case HEADS_UP_ROUTER_MODE::HIGHLIGHT_COLLISIONS: mode = _( "Highlight collisions" ); break;
    }

    wxString source;

    switch( aRoute.widthSource )
    {
    case HEADS_UP_WIDTH_SOURCE::NET_CLASS: source = _( "net class" ); break;
    case HEADS_UP_WIDTH_SOURCE::USER: source = _( "user" ); break;
    case HEADS_UP_WIDTH_SOURCE::CUSTOM: source = _( "custom" ); break;
    }

    wxString layers = aRoute.layerFrom;

    if( !aRoute.layerTo.IsEmpty() && aRoute.layerTo != aRoute.layerFrom )
        layers += wxS( " > " ) + aRoute.layerTo;

    return wxString::Format( _( "%s, %s, width %s (%s), via %s / %s, %s, length %s" ), corner, mode,
                             formatDistance( aUnits, aRoute.trackWidth, true ), source,
                             formatDistance( aUnits, aRoute.viaDiameter, false ),
                             formatDistance( aUnits, aRoute.viaDrill, true ), layers,
                             formatDistance( aUnits, static_cast<double>( aRoute.netLength ), true ) );
}


std::vector<wxString> WrapAtCommas( const wxString& aLine, int aMaxCharacters )
{
    std::vector<wxString> rows;
    wxString              row;
    size_t                start = 0;

    while( start < aLine.length() )
    {
        // The part up to and including its comma and the space after it
        size_t end = aLine.find( wxS( ", " ), start );
        end = end == wxString::npos ? aLine.length() : end + 2;

        wxString part = aLine.substr( start, end - start );

        if( !row.IsEmpty() && static_cast<int>( row.length() + part.length() ) > aMaxCharacters + 1 )
        {
            rows.push_back( row.Trim() );
            row.clear();
        }

        row += part;
        start = end;
    }

    if( !row.IsEmpty() )
        rows.push_back( row.Trim() );

    return rows;
}


std::vector<HEADS_UP_LINE> BuildHeadsUpLines( const HEADS_UP_STATE& aState )
{
    std::vector<HEADS_UP_LINE> lines;

    const wxString unitLabel = EDA_UNIT_UTILS::GetLabel( aState.units );

    lines.push_back( { wxString::Format( wxS( "X %s  Y %s %s" ), formatDistance( aState.units, aState.cursor.x, false ),
                                         formatDistance( aState.units, aState.cursor.y, false ), unitLabel ),
                       false } );

    lines.push_back(
            { wxString::Format( wxS( "dX %s  dY %s  dist %s" ), formatDistance( aState.units, aState.delta.x, false ),
                                formatDistance( aState.units, aState.delta.y, false ),
                                formatDistance( aState.units, aState.delta.EuclideanNorm(), true ) ),
              false } );

    lines.push_back( { wxString::Format( _( "Layer %s" ), aState.layerName ), true } );

    wxString grid = formatDistance( aState.units, aState.gridX, false );

    if( aState.gridX != aState.gridY )
        grid += wxS( " x " ) + formatDistance( aState.units, aState.gridY, false );

    lines.push_back( { wxString::Format( _( "Grid %s %s, snap %s" ), grid, unitLabel,
                                         aState.isSnapEnabled ? _( "on" ) : _( "off" ) ),
                       false } );

    const wxString none = wxS( "-" );

    lines.push_back(
            { wxString::Format( _( "Net %s  Ref %s" ), aState.item.netName.IsEmpty() ? none : aState.item.netName,
                                aState.item.reference.IsEmpty() ? none : aState.item.reference ),
              false } );

    if( aState.route )
    {
        for( const wxString& row :
             WrapAtCommas( BuildHeadsUpRouteLine( *aState.route, aState.units ), HEADS_UP_ROUTE_ROW_CHARACTERS ) )
        {
            lines.push_back( { row, false } );
        }
    }

    return lines;
}


VECTOR2D HeadsUpDelta( const VECTOR2D& aCursor, const BASE_SCREEN& aScreen )
{
    return aCursor - aScreen.m_LocalOrigin;
}


namespace
{
/// How strongly a kind of item claims the point; the active layer breaks ties inside a kind
int kindRank( KICAD_T aType )
{
    switch( aType )
    {
    case PCB_PAD_T: return 5;
    case PCB_VIA_T: return 4;
    case PCB_TRACE_T:
    case PCB_ARC_T: return 3;
    case PCB_ZONE_T: return 2;
    case PCB_FOOTPRINT_T: return 1;
    default: return 0;
    }
}
} // namespace


HEADS_UP_ITEM FindHeadsUpItem( const KIGFX::VIEW& aView, const VECTOR2I& aPosition, int aMargin,
                               PCB_LAYER_ID aActiveLayer )
{
    std::vector<KIGFX::VIEW::LAYER_ITEM_PAIR> candidates;

    const BOX2I area( aPosition - VECTOR2I( aMargin, aMargin ), VECTOR2I( aMargin * 2 + 1, aMargin * 2 + 1 ) );

    aView.Query( area, candidates );

    const BOARD_ITEM* best = nullptr;
    int               bestScore = 0;

    for( const KIGFX::VIEW::LAYER_ITEM_PAIR& candidate : candidates )
    {
        const BOARD_ITEM* item = dynamic_cast<const BOARD_ITEM*>( candidate.first );

        if( !item )
            continue;

        // The layers of the view a board item is drawn on include virtual ones (net names, clearances, the object
        // kinds). The board layers say whether the item itself is on show; a footprint has only virtual ones.
        if( item->Type() != PCB_FOOTPRINT_T && candidate.second >= PCB_LAYER_ID_COUNT )
            continue;

        if( !aView.IsLayerVisible( candidate.second ) )
            continue;

        const int rank = kindRank( item->Type() );

        if( rank == 0 )
            continue;

        const int score = rank * 2 + ( item->IsOnLayer( aActiveLayer ) ? 1 : 0 );

        if( score <= bestScore || !item->HitTest( aPosition, aMargin ) )
            continue;

        best = item;
        bestScore = score;
    }

    HEADS_UP_ITEM found;

    if( !best )
        return found;

    if( const BOARD_CONNECTED_ITEM* connected = dynamic_cast<const BOARD_CONNECTED_ITEM*>( best ) )
        found.netName = connected->GetNetname();

    if( const FOOTPRINT* footprint =
                best->Type() == PCB_FOOTPRINT_T ? static_cast<const FOOTPRINT*>( best ) : best->GetParentFootprint() )
    {
        found.reference = footprint->GetReference();
    }

    return found;
}

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Alex Shvartzkop <dudesuchamazing@gmail.com>
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

#include <pcb_generator.h>
#include <generators_mgr.h>

#include <functional>
#include <optional>
#include <magic_enum.hpp>

#include <wx/debug.h>
#include <wx/log.h>

#include <gal/graphics_abstraction_layer.h>
#include <geometry/shape_circle.h>
#include <geometry/geometry_utils.h>
#include <kiplatform/ui.h>
#include <dialogs/dialog_unit_entry.h>
#include <collectors.h>
#include <scoped_set_reset.h>
#include <core/mirror.h>
#include <string_utils.h>

#include <board.h>
#include <board_design_settings.h>
#include <drc/drc_engine.h>
#include <pcb_track.h>
#include <pcb_shape.h>
#include <pcb_group.h>

#include <tool/edit_points.h>
#include <tool/tool_manager.h>
#include <tools/drawing_tool.h>
#include <tools/generator_tool.h>
#include <tools/pcb_picker_tool.h>
#include <tools/pcb_selection_tool.h>
#include <tools/zone_filler_tool.h>

#include <preview_items/draw_context.h>
#include <preview_items/preview_utils.h>
#include <view/view.h>
#include <view/view_controls.h>

#include <router/pns_dp_meander_placer.h>
#include <router/pns_meander_placer_base.h>
#include <router/pns_meander.h>
#include <router/pns_kicad_iface.h>
#include <router/pns_segment.h>
#include <router/pns_arc.h>
#include <router/pns_solid.h>
#include <router/pns_topology.h>
#include <router/router_preview_item.h>

#include <dialogs/dialog_tuning_pattern_properties.h>

#include <generators/pcb_tuning_pattern.h>
#include <properties/property.h>
#include <properties/property_mgr.h>


PCB_TUNING_PATTERN::PCB_TUNING_PATTERN( BOARD_ITEM* aParent, PCB_LAYER_ID aLayer,
                                        LENGTH_TUNING_MODE aMode ) :
        PCB_GENERATOR( aParent, aLayer ),
        m_trackWidth( 0 ),
        m_diffPairGap( 0 ),
        m_tuningMode( aMode ),
        m_tuningLength( 0 ),
        m_tuningStatus( PNS::MEANDER_PLACER_BASE::TUNING_STATUS::TUNED ),
        m_updateSideFromEnd( false )
{
    m_generatorType = GENERATOR_TYPE;
    m_name = DISPLAY_NAME;
    m_end = VECTOR2I( pcbIUScale.mmToIU( 10 ), 0 );
    m_settings.m_initialSide = PNS::MEANDER_SIDE_LEFT;
}

PCB_TUNING_PATTERN* PCB_TUNING_PATTERN::CreateNew( GENERATOR_TOOL*, PCB_BASE_EDIT_FRAME*, BOARD_CONNECTED_ITEM*,
                                                   LENGTH_TUNING_MODE )
{
    return nullptr;
}


bool PCB_TUNING_PATTERN::baselineValid()
{
    return false;
}


void PCB_TUNING_PATTERN::EditStart( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* )
{
}


bool PCB_TUNING_PATTERN::initBaseLine( PNS::ROUTER*, int, BOARD*, VECTOR2I&, VECTOR2I&, NETINFO_ITEM*,
                                       std::optional<SHAPE_LINE_CHAIN>& )
{
    return true;
}


bool PCB_TUNING_PATTERN::initBaseLines( PNS::ROUTER*, int, BOARD* )
{
    return true;
}


bool PCB_TUNING_PATTERN::removeToBaseline( PNS::ROUTER*, int, SHAPE_LINE_CHAIN& )
{
 
    return true;
}


void PCB_TUNING_PATTERN::Remove( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* )
{
    
}


bool PCB_TUNING_PATTERN::recoverBaseline( PNS::ROUTER*, int )
{
    return true;
}


bool PCB_TUNING_PATTERN::resetToBaseline( GENERATOR_TOOL*, int, SHAPE_LINE_CHAIN&, bool )
{
    return true;
}


bool PCB_TUNING_PATTERN::Update( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* )
{
    return true;
}


void PCB_TUNING_PATTERN::EditFinish( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* )
{
}


void PCB_TUNING_PATTERN::EditCancel( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* )
{
}


bool PCB_TUNING_PATTERN::MakeEditPoints( EDIT_POINTS& ) const
{
    return true;
}


bool PCB_TUNING_PATTERN::UpdateFromEditPoints( EDIT_POINTS& )
{
 
    return true;
}


bool PCB_TUNING_PATTERN::UpdateEditPoints( EDIT_POINTS& )
{
 
    return true;
}


SHAPE_LINE_CHAIN PCB_TUNING_PATTERN::getOutline() const
{
    return SHAPE_LINE_CHAIN();
}


void PCB_TUNING_PATTERN::ViewDraw( int, KIGFX::VIEW* ) const
{
}


const STRING_ANY_MAP PCB_TUNING_PATTERN::GetProperties() const
{
    return STRING_ANY_MAP();
}


void PCB_TUNING_PATTERN::SetProperties( const STRING_ANY_MAP& )
{
    
}


void PCB_TUNING_PATTERN::Serialize( google::protobuf::Any& ) const
{
}


bool PCB_TUNING_PATTERN::Deserialize( const google::protobuf::Any& )
{
    return false;
}


void PCB_TUNING_PATTERN::ShowPropertiesDialog( PCB_BASE_EDIT_FRAME* )
{
}


std::vector<EDA_ITEM*> PCB_TUNING_PATTERN::GetPreviewItems( GENERATOR_TOOL*, PCB_BASE_EDIT_FRAME*, bool )
{
    std::vector<EDA_ITEM*> previewItems;

    return previewItems;
}


void PCB_TUNING_PATTERN::GetMsgPanelInfo( EDA_DRAW_FRAME*, std::vector<MSG_PANEL_ITEM>& )
{
}


const wxString PCB_TUNING_PATTERN::DISPLAY_NAME = _HKI( "Tuning Pattern" );
const wxString PCB_TUNING_PATTERN::GENERATOR_TYPE = wxS( "tuning_pattern" );


using SCOPED_DRAW_MODE = SCOPED_SET_RESET<DRAWING_TOOL::MODE>;


#define HITTEST_THRESHOLD_PIXELS 5


int DRAWING_TOOL::PlaceTuningPattern( const TOOL_EVENT& )
{
    return 0;
}


static GENERATORS_MGR::REGISTER<PCB_TUNING_PATTERN> registerMe;

// Also register under the 7.99 name
template <typename T>
struct REGISTER_LEGACY_TUNING_PATTERN
{
    REGISTER_LEGACY_TUNING_PATTERN()
    {
        GENERATORS_MGR::Instance().Register( wxS( "meanders" ), T::DISPLAY_NAME,
                                             []()
                                             {
                                                 return new T;
                                             } );
    }
};

static REGISTER_LEGACY_TUNING_PATTERN<PCB_TUNING_PATTERN> registerMeToo;

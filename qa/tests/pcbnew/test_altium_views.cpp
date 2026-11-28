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

/**
 * @file test_altium_views.cpp
 * The views of the Altium-style interaction profile in the PCB editors (design note, 5.7): the actions behind L,
 * Ctrl+D, [, ] and G are registered, bound in that profile and in no other; through-hole pads and vias stay visible
 * in the dimmed and hidden single-layer modes under it; net highlight fades by the mask level under it.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <board.h>
#include <footprint.h>
#include <interaction_profile.h>
#include <pad.h>
#include <pcb_painter.h>
#include <pcb_track.h>
#include <pgm_base.h>
#include <settings/color_settings.h>
#include <settings/common_settings.h>
#include <tool/action_manager.h>
#include <tool/tool_event.h>
#include <tools/pcb_actions.h>

#include <algorithm>


namespace
{

struct VIEW_ACTION
{
    TOOL_ACTION* m_action;
    std::string  m_name;
    int          m_altiumKey;
};


std::vector<VIEW_ACTION> viewActions()
{
    return { { &PCB_ACTIONS::showAppearanceLayers, "pcbnew.Control.showAppearanceLayers", 'L' },
             { &PCB_ACTIONS::showAppearanceObjects, "pcbnew.Control.showAppearanceObjects", MD_CTRL + 'D' },
             { &PCB_ACTIONS::maskLevelDown, "pcbnew.Control.maskLevelDown", '[' },
             { &PCB_ACTIONS::maskLevelUp, "pcbnew.Control.maskLevelUp", ']' },
             { &PCB_ACTIONS::showGridMenu, "pcbnew.Control.showGridMenu", 'G' } };
}


/// Sets the interaction profile for a test and puts the one that was there back
struct PROFILE_SCOPE
{
    explicit PROFILE_SCOPE( INTERACTION_PROFILE_ID aProfile ) :
            m_previous( INTERACTION_PROFILE::Active() )
    {
        INTERACTION_PROFILE::SetActive( aProfile );
    }

    ~PROFILE_SCOPE() { INTERACTION_PROFILE::SetActive( m_previous ); }

    INTERACTION_PROFILE_ID m_previous;
};


/// What a profile makes of an action's compiled default keys in the PCB editor
int keyUnder( INTERACTION_PROFILE_ID aProfile, const VIEW_ACTION& aEntry )
{
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( aProfile );
    std::pair<int, int>  keys = INTERACTION_PROFILE::ApplyHotKeyLayer(
            layer, aEntry.m_name, { aEntry.m_action->GetHotKey(), aEntry.m_action->GetHotKeyAlt() }, "pcbnew." );

    return keys.first;
}

} // namespace


BOOST_AUTO_TEST_SUITE( AltiumViews )


BOOST_AUTO_TEST_CASE( EveryViewActionIsRegisteredAndNamedAsTheTableNamesIt )
{
    const std::list<TOOL_ACTION*>& registered = ACTION_MANAGER::GetActionList();

    for( const VIEW_ACTION& entry : viewActions() )
    {
        BOOST_CHECK_EQUAL( entry.m_action->GetName(), entry.m_name );
        BOOST_CHECK_MESSAGE( std::find( registered.begin(), registered.end(), entry.m_action ) != registered.end(),
                             entry.m_name << " is not registered" );
    }
}


BOOST_AUTO_TEST_CASE( TheViewActionsAreBoundInTheAltiumProfileAndNotInStock )
{
    for( const VIEW_ACTION& entry : viewActions() )
    {
        BOOST_CHECK_MESSAGE( keyUnder( INTERACTION_PROFILE_ID::ALTIUM, entry ) == entry.m_altiumKey,
                             entry.m_name << " is not on its Altium-style key" );

        // The stock profile is untouched: an action of this step has no compiled key, so none is bound
        BOOST_CHECK_MESSAGE( keyUnder( INTERACTION_PROFILE_ID::STOCK, entry ) == 0,
                             entry.m_name << " is bound in the stock profile" );
        BOOST_CHECK_EQUAL( entry.m_action->GetHotKey(), 0 );
    }
}


BOOST_AUTO_TEST_CASE( TheGridKeyOpensTheListInThePcbEditorsAndCyclesInTheSchematicEditor )
{
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    // G is a pcbnew. action's key in the PCB editors, so the common. grid action that also holds it loses it there
    std::pair<int, int> gridNext =
            INTERACTION_PROFILE::ApplyHotKeyLayer( layer, "common.Control.gridNext", { 0, 0 }, "pcbnew." );
    BOOST_CHECK( layer.m_bindings.at( "common.Control.gridNext" ).first == 'G' );
    BOOST_CHECK( layer.m_bindings.at( "pcbnew.Control.showGridMenu" ).first == 'G' );
    BOOST_CHECK( layer.m_takenKeysByScope.at( "pcbnew." ).count( 'G' ) );

    // ... and keeps G in the schematic editor, where Shift+G is the previous grid
    BOOST_CHECK( layer.m_bindings.at( "common.Control.gridPrev" ).first == ( MD_SHIFT + 'G' ) );
    BOOST_CHECK( layer.m_takenKeysByScope.at( "common." ).count( 'G' ) );

    // Ctrl+G and Q are bound for both
    BOOST_CHECK( layer.m_bindings.at( "common.Control.editGrids" ).first == MD_CTRL + 'G' );
    BOOST_CHECK( layer.m_bindings.at( "common.Control.toggleUnits" ).first == 'Q' );
    (void) gridNext;
}


BOOST_AUTO_TEST_CASE( LIsTheIdleKeyAndStillTheFlipKeyWhileAFootprintMoves )
{
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    BOOST_CHECK( layer.m_bindings.at( "pcbnew.InteractiveEdit.flipWhileMoving" ).first == 'L' );
    BOOST_CHECK( layer.m_bindings.at( "pcbnew.InteractiveDrawing.flipWhilePlacing" ).first == 'L' );
    BOOST_CHECK( layer.m_bindings.at( "pcbnew.Control.showAppearanceLayers" ).first == 'L' );

    // The running command's alias wins: it is a context action, the Appearance action a global one
    BOOST_CHECK( PCB_ACTIONS::flipWhileMoving.GetScope() == AS_CONTEXT );
    BOOST_CHECK( PCB_ACTIONS::showAppearanceLayers.GetScope() == AS_GLOBAL );

    // ... and the alias keeps L where the new action takes it
    std::pair<int, int> kept = INTERACTION_PROFILE::ApplyHotKeyLayer( layer, "pcbnew.InteractiveEdit.flipWhileMoving",
                                                                      { 0, 0 }, "pcbnew.", true );
    BOOST_CHECK( kept.first == 'L' );
}


namespace
{

/// Colors where the background is black and everything else is gray, so a dimmed item is told from a plain one
class VIEW_FIXTURE
{
public:
    VIEW_FIXTURE()
    {
        m_colors.SetColor( LAYER_PCB_BACKGROUND, KIGFX::COLOR4D( 0.0, 0.0, 0.0, 1.0 ) );

        for( int layer = 0; layer < PCB_LAYER_ID_COUNT; layer++ )
            m_colors.SetColor( layer, KIGFX::COLOR4D( 0.8, 0.8, 0.8, 1.0 ) );

        for( int layer : { LAYER_VIA_THROUGH, LAYER_VIA_BLIND, LAYER_VIA_HOLES } )
            m_colors.SetColor( layer, KIGFX::COLOR4D( 0.8, 0.8, 0.8, 1.0 ) );

        m_colors.SetColor( LAYER_PCB_BACKGROUND, KIGFX::COLOR4D( 0.0, 0.0, 0.0, 1.0 ) );

        m_board.reset( new BOARD );
    }

    /// Color settings loaded under the profile in force, with the single-layer mode and the active layer
    void Prepare( HIGH_CONTRAST_MODE aMode, PCB_LAYER_ID aActiveLayer )
    {
        m_settings.LoadColors( &m_colors );

        PCB_DISPLAY_OPTIONS options;
        options.m_ContrastModeDisplay = aMode;
        m_settings.LoadDisplayOptions( options );

        // What PCB_DRAW_PANEL_GAL::applyHighContrastLayer marks as active for a layer
        m_settings.SetActiveLayer( aActiveLayer );
        m_settings.ClearHighContrastLayers();
        m_settings.SetLayerIsHighContrast( aActiveLayer );

        if( IsCopperLayer( aActiveLayer ) )
        {
            for( int layer : std::initializer_list<int>{
                         PAD_COPPER_LAYER_FOR( aActiveLayer ), VIA_COPPER_LAYER_FOR( aActiveLayer ),
                         ZONE_LAYER_FOR( aActiveLayer ), LAYER_VIA_THROUGH, LAYER_PAD_PLATEDHOLES, LAYER_VIA_HOLES } )
            {
                m_settings.SetLayerIsHighContrast( layer );
            }
        }
    }

    KIGFX::COLOR4D PlainColor( const BOARD_ITEM* aItem, int aLayer )
    {
        Prepare( HIGH_CONTRAST_MODE::NORMAL, F_Cu );
        return m_settings.GetColor( aItem, aLayer );
    }

    COLOR_SETTINGS             m_colors;
    KIGFX::PCB_RENDER_SETTINGS m_settings;
    std::unique_ptr<BOARD>     m_board;
};

} // namespace


BOOST_FIXTURE_TEST_CASE( ThroughHolePadsAndViasStayVisibleInTheSingleLayerModesUnderTheProfile, VIEW_FIXTURE )
{
    FOOTPRINT* footprint = new FOOTPRINT( m_board.get() );
    m_board->Add( footprint );

    PAD* throughPad = new PAD( footprint );
    throughPad->SetAttribute( PAD_ATTRIB::PTH );
    throughPad->SetLayerSet( PAD::PTHMask() );
    footprint->Add( throughPad );

    PAD* surfacePad = new PAD( footprint );
    surfacePad->SetAttribute( PAD_ATTRIB::SMD );
    surfacePad->SetLayerSet( PAD::SMDMask() );
    footprint->Add( surfacePad );

    PCB_VIA* throughVia = new PCB_VIA( m_board.get() );
    throughVia->SetViaType( VIATYPE::THROUGH );
    m_board->Add( throughVia );

    PCB_VIA* blindVia = new PCB_VIA( m_board.get() );
    blindVia->SetViaType( VIATYPE::BLIND );
    m_board->Add( blindVia );

    const int padLayer = PAD_COPPER_LAYER_FOR( B_Cu );
    const int viaLayer = LAYER_VIA_THROUGH;

    // Layers that are inactive: the back copper while the front is active, and every copper layer when the silkscreen is
    for( HIGH_CONTRAST_MODE mode : { HIGH_CONTRAST_MODE::DIMMED, HIGH_CONTRAST_MODE::HIDDEN } )
    {
        for( PCB_LAYER_ID active : { F_Cu, F_SilkS } )
        {
            {
                // Stock: the inactive layers are dimmed or hidden, through-hole parts with them
                PROFILE_SCOPE        stock( INTERACTION_PROFILE_ID::STOCK );
                const KIGFX::COLOR4D plainPad = PlainColor( throughPad, padLayer );
                const KIGFX::COLOR4D plainVia = PlainColor( throughVia, viaLayer );

                Prepare( mode, active );
                BOOST_CHECK_MESSAGE( m_settings.GetColor( throughPad, padLayer ) != plainPad,
                                     "stock does not dim a through-hole pad, mode " << (int) mode );
                BOOST_CHECK( !m_settings.IsThroughHoleKept( throughPad ) );

                if( active == F_SilkS )
                    BOOST_CHECK( m_settings.GetColor( throughVia, viaLayer ) != plainVia );
            }

            {
                PROFILE_SCOPE        altium( INTERACTION_PROFILE_ID::ALTIUM );
                const KIGFX::COLOR4D plainPad = PlainColor( throughPad, padLayer );
                const KIGFX::COLOR4D plainVia = PlainColor( throughVia, viaLayer );

                Prepare( mode, active );
                BOOST_CHECK( m_settings.IsThroughHoleKept( throughPad ) );
                BOOST_CHECK( m_settings.IsThroughHoleKept( throughVia ) );
                BOOST_CHECK( m_settings.GetColor( throughPad, padLayer ) == plainPad );
                BOOST_CHECK( m_settings.GetColor( throughVia, viaLayer ) == plainVia );

                // Surface pads and blind vias are dimmed or hidden as they always were
                BOOST_CHECK( !m_settings.IsThroughHoleKept( surfacePad ) );
                BOOST_CHECK( !m_settings.IsThroughHoleKept( blindVia ) );
                BOOST_CHECK( m_settings.GetColor( surfacePad, PAD_COPPER_LAYER_FOR( B_Cu ) ) != plainPad );
            }
        }
    }
}


BOOST_FIXTURE_TEST_CASE( NetHighlightFadesByTheMaskLevelUnderTheProfileAndByAHalfInStock, VIEW_FIXTURE )
{
    std::bitset<MAX_LAYERS_FOR_VIEW> faded;
    faded.set( F_Cu );

    auto keepOfAFadedLayer = [&]() -> double
    {
        m_settings.SetCanvasFadedLayers( faded );
        m_settings.SetHighlight( true, 7 );

        KIGFX::COLOR4D toward;
        double         keep = 0.0;

        BOOST_REQUIRE( m_settings.GetLayerDim( F_Cu, toward, keep ) );
        return keep;
    };

    {
        PROFILE_SCOPE stock( INTERACTION_PROFILE_ID::STOCK );
        m_settings.LoadColors( &m_colors );
        m_settings.SetHighContrastDimming( 0.7 );

        BOOST_CHECK_CLOSE( keepOfAFadedLayer(), 0.5, 0.001 );
    }

    {
        PROFILE_SCOPE altium( INTERACTION_PROFILE_ID::ALTIUM );
        m_settings.LoadColors( &m_colors );
        m_settings.SetHighContrastDimming( 0.7 );

        BOOST_CHECK_CLOSE( keepOfAFadedLayer(), 0.3, 0.001 );

        m_settings.SetHighContrastDimming( 0.2 );
        BOOST_CHECK_CLOSE( keepOfAFadedLayer(), 0.8, 0.001 );
    }
}


BOOST_AUTO_TEST_SUITE_END()

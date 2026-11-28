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

/**
 * @file test_heads_up_model.cpp
 * The heads-up display of the Altium-style interaction profile (design note, 5.7) without a window: what it says for a
 * state, in each unit; its routing line from the router's own settings; the delta after a local origin reset; the item
 * under a point; its setting; and the actions and keys behind Shift+H, Shift+G and Insert.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <base_units.h>
#include <board.h>
#include <footprint.h>
#include <interaction_profile.h>
#include <io/io_mgr.h>
#include <netinfo.h>
#include <pad.h>
#include <pcb_io/pcb_io.h>
#include <pcb_io/pcb_io_mgr.h>
#include <pcb_screen.h>
#include <pcb_track.h>
#include <pcbnew_settings.h>
#include <settings/json_settings_internals.h>
#include <tool/action_manager.h>
#include <tool/actions.h>
#include <tool/tool_event.h>
#include <tools/pcb_actions.h>
#include <view/view.h>
#include <widgets/pcb_heads_up_model.h>
#include <zone.h>

#include <router/pns_routing_settings.h>
#include <router/pns_sizes_settings.h>


namespace
{
HEADS_UP_STATE sampleState( EDA_UNITS aUnits )
{
    HEADS_UP_STATE state;

    state.units = aUnits;
    state.cursor = VECTOR2D( pcbIUScale.mmToIU( 25.4 ), pcbIUScale.mmToIU( -12.7 ) );
    state.delta = VECTOR2D( pcbIUScale.mmToIU( 2.54 ), pcbIUScale.mmToIU( 5.08 ) );
    state.layerName = wxS( "F.Cu" );
    state.gridX = pcbIUScale.mmToIU( 1.27 );
    state.gridY = pcbIUScale.mmToIU( 1.27 );
    state.isSnapEnabled = true;
    state.item.netName = wxS( "GND" );
    state.item.reference = wxS( "U3" );

    return state;
}
} // namespace


BOOST_AUTO_TEST_SUITE( HeadsUpModel )


BOOST_AUTO_TEST_CASE( TheLinesSayEachThingTheNoteListsInMillimeters )
{
    const std::vector<HEADS_UP_LINE> lines = BuildHeadsUpLines( sampleState( EDA_UNITS::MM ) );

    BOOST_REQUIRE_EQUAL( lines.size(), 5u );
    BOOST_CHECK_EQUAL( lines[0].text, wxS( "X 25.4000  Y -12.7000 mm" ) );
    BOOST_CHECK_EQUAL( lines[1].text, wxS( "dX 2.5400  dY 5.0800  dist 5.6796 mm" ) );
    BOOST_CHECK_EQUAL( lines[2].text, wxS( "Layer F.Cu" ) );
    BOOST_CHECK( lines[2].isLayerLine );
    BOOST_CHECK_EQUAL( lines[3].text, wxS( "Grid 1.2700 mm, snap on" ) );
    BOOST_CHECK_EQUAL( lines[4].text, wxS( "Net GND  Ref U3" ) );
}


BOOST_AUTO_TEST_CASE( TheSameStateReadsInMilsAndInches )
{
    const std::vector<HEADS_UP_LINE> mils = BuildHeadsUpLines( sampleState( EDA_UNITS::MILS ) );

    BOOST_CHECK_EQUAL( mils[0].text, wxS( "X 1000.00  Y -500.00 mils" ) );
    BOOST_CHECK_EQUAL( mils[3].text, wxS( "Grid 50.00 mils, snap on" ) );

    const std::vector<HEADS_UP_LINE> inches = BuildHeadsUpLines( sampleState( EDA_UNITS::INCH ) );

    BOOST_CHECK_EQUAL( inches[0].text, wxS( "X 1.0000  Y -0.5000 in" ) );
    BOOST_CHECK_EQUAL( inches[3].text, wxS( "Grid 0.0500 in, snap on" ) );
}


BOOST_AUTO_TEST_CASE( UnitsAreKiCadsOwnFormatting )
{
    // The display adds nothing of its own to a number: it is the status bar's message text for the same value
    for( EDA_UNITS units : { EDA_UNITS::MM, EDA_UNITS::MILS, EDA_UNITS::INCH } )
    {
        HEADS_UP_STATE state = sampleState( units );
        state.cursor = VECTOR2D( 123457, 7654321 );

        const wxString expected = wxString::Format(
                wxS( "X %s  Y %s %s" ), EDA_UNIT_UTILS::UI::MessageTextFromValue( pcbIUScale, units, 123457.0, false ),
                EDA_UNIT_UTILS::UI::MessageTextFromValue( pcbIUScale, units, 7654321.0, false ),
                EDA_UNIT_UTILS::GetLabel( units ) );

        BOOST_CHECK_EQUAL( BuildHeadsUpLines( state )[0].text, expected );
    }
}


BOOST_AUTO_TEST_CASE( NothingUnderTheCursorShowsDashes )
{
    HEADS_UP_STATE state = sampleState( EDA_UNITS::MM );
    state.item = HEADS_UP_ITEM();

    BOOST_CHECK_EQUAL( BuildHeadsUpLines( state )[4].text, wxS( "Net -  Ref -" ) );
}


BOOST_AUTO_TEST_CASE( AGridThatIsNotSquareSaysBothSides )
{
    HEADS_UP_STATE state = sampleState( EDA_UNITS::MM );
    state.gridY = pcbIUScale.mmToIU( 0.5 );
    state.isSnapEnabled = false;

    BOOST_CHECK_EQUAL( BuildHeadsUpLines( state )[3].text, wxS( "Grid 1.2700 x 0.5000 mm, snap off" ) );
}


BOOST_AUTO_TEST_CASE( TheRoutingLineComesLastAndOnlyWhileRouting )
{
    HEADS_UP_STATE state = sampleState( EDA_UNITS::MM );

    BOOST_CHECK_EQUAL( BuildHeadsUpLines( state ).size(), 5u );

    state.route = HEADS_UP_ROUTE();

    const std::vector<HEADS_UP_LINE> lines = BuildHeadsUpLines( state );

    // The routing line is wrapped at commas into rows that fit, and the rows say all of it
    BOOST_REQUIRE_GT( lines.size(), 5u );

    wxString joined;

    for( size_t row = 5; row < lines.size(); row++ )
    {
        BOOST_CHECK_LE( static_cast<int>( lines[row].text.length() ), HEADS_UP_ROUTE_ROW_CHARACTERS );
        joined += ( joined.IsEmpty() ? wxS( "" ) : wxS( " " ) ) + lines[row].text;
    }

    BOOST_CHECK_EQUAL( joined, BuildHeadsUpRouteLine( *state.route, EDA_UNITS::MM ) );
}


BOOST_AUTO_TEST_CASE( AWrappedLineBreaksOnlyAfterACommaAndKeepsALongPartWhole )
{
    const std::vector<wxString> rows = WrapAtCommas( wxS( "aaaa, bbbb, cccc, dddd" ), 11 );

    BOOST_REQUIRE_EQUAL( rows.size(), 2u );
    BOOST_CHECK_EQUAL( rows[0], wxS( "aaaa, bbbb," ) );
    BOOST_CHECK_EQUAL( rows[1], wxS( "cccc, dddd" ) );

    // A part wider than a row sits on a row of its own, unbroken
    const std::vector<wxString> longPart = WrapAtCommas( wxS( "a, bbbbbbbbbbbbbbbbbbbb, c" ), 8 );

    BOOST_REQUIRE_EQUAL( longPart.size(), 3u );
    BOOST_CHECK_EQUAL( longPart[1], wxS( "bbbbbbbbbbbbbbbbbbbb," ) );

    BOOST_CHECK( WrapAtCommas( wxS( "" ), 10 ).empty() );
    BOOST_CHECK_EQUAL( WrapAtCommas( wxS( "no commas at all" ), 4 ).size(), 1u );
}


BOOST_AUTO_TEST_CASE( TheRoutingLineIsBuiltFromTheRoutersOwnSettings )
{
    PNS::ROUTING_SETTINGS settings( nullptr, "" );
    settings.SetFreeAngleMode( false );
    settings.SetCornerMode( DIRECTION_45::CORNER_MODE::ROUNDED_45 );
    settings.SetMode( PNS::PNS_MODE::RM_Walkaround );

    PNS::SIZES_SETTINGS sizes;
    sizes.SetTrackWidth( pcbIUScale.mmToIU( 0.25 ) );
    sizes.SetViaDiameter( pcbIUScale.mmToIU( 0.8 ) );
    sizes.SetViaDrill( pcbIUScale.mmToIU( 0.4 ) );
    sizes.SetDiffPairWidth( pcbIUScale.mmToIU( 0.15 ) );

    const HEADS_UP_ROUTE route = HeadsUpRouteFromRouter( settings, sizes, false, HEADS_UP_WIDTH_SOURCE::NET_CLASS,
                                                         wxS( "F.Cu" ), wxS( "B.Cu" ), pcbIUScale.mmToIU( 12.5 ) );

    BOOST_CHECK( route.cornerMode == HEADS_UP_CORNER_MODE::ROUNDED_45 );
    BOOST_CHECK( route.routerMode == HEADS_UP_ROUTER_MODE::WALK_AROUND );
    BOOST_CHECK_EQUAL( route.trackWidth, pcbIUScale.mmToIU( 0.25 ) );

    BOOST_CHECK_EQUAL( BuildHeadsUpRouteLine( route, EDA_UNITS::MM ),
                       wxS( "45-degree rounded, Walk around, width 0.2500 mm (net class), via 0.8000 / 0.4000 mm, "
                            "F.Cu > B.Cu, length 12.5000 mm" ) );
}


BOOST_AUTO_TEST_CASE( TheRoutingLineFollowsEachModeAndSource )
{
    PNS::ROUTING_SETTINGS settings( nullptr, "" );
    PNS::SIZES_SETTINGS   sizes;
    sizes.SetTrackWidth( pcbIUScale.mmToIU( 0.2 ) );
    sizes.SetDiffPairWidth( pcbIUScale.mmToIU( 0.1 ) );

    settings.SetMode( PNS::PNS_MODE::RM_Shove );
    settings.SetCornerMode( DIRECTION_45::CORNER_MODE::MITERED_90 );

    HEADS_UP_ROUTE route = HeadsUpRouteFromRouter( settings, sizes, false, HEADS_UP_WIDTH_SOURCE::USER, wxS( "F.Cu" ),
                                                   wxS( "F.Cu" ), 0 );

    BOOST_CHECK( route.routerMode == HEADS_UP_ROUTER_MODE::SHOVE );
    BOOST_CHECK( route.cornerMode == HEADS_UP_CORNER_MODE::MITERED_90 );

    // One layer on both ends is said once
    BOOST_CHECK( BuildHeadsUpRouteLine( route, EDA_UNITS::MM ).Contains( wxS( "(user), via" ) ) );
    BOOST_CHECK( BuildHeadsUpRouteLine( route, EDA_UNITS::MM ).Contains( wxS( "90-degree, Shove" ) ) );
    BOOST_CHECK( BuildHeadsUpRouteLine( route, EDA_UNITS::MM ).Contains( wxS( ", F.Cu, length" ) ) );

    settings.SetMode( PNS::PNS_MODE::RM_MarkObstacles );
    settings.SetFreeAngleMode( true );

    route = HeadsUpRouteFromRouter( settings, sizes, true, HEADS_UP_WIDTH_SOURCE::CUSTOM, wxS( "F.Cu" ), wxS( "B.Cu" ),
                                    0 );

    // A free-angle route is a mode of its own whatever the corner mode says, and a pair is as wide as the pair
    BOOST_CHECK( route.cornerMode == HEADS_UP_CORNER_MODE::FREE_ANGLE );
    BOOST_CHECK( route.routerMode == HEADS_UP_ROUTER_MODE::HIGHLIGHT_COLLISIONS );
    BOOST_CHECK_EQUAL( route.trackWidth, pcbIUScale.mmToIU( 0.1 ) );
    BOOST_CHECK( BuildHeadsUpRouteLine( route, EDA_UNITS::MM ).Contains( wxS( "(custom)" ) ) );
}


BOOST_AUTO_TEST_CASE( TheWidthSourceIsTheCustomSizeThenTheNetClassThenTheList )
{
    BOOST_CHECK( HeadsUpWidthSource( true, true ) == HEADS_UP_WIDTH_SOURCE::CUSTOM );
    BOOST_CHECK( HeadsUpWidthSource( true, false ) == HEADS_UP_WIDTH_SOURCE::CUSTOM );
    BOOST_CHECK( HeadsUpWidthSource( false, true ) == HEADS_UP_WIDTH_SOURCE::NET_CLASS );
    BOOST_CHECK( HeadsUpWidthSource( false, false ) == HEADS_UP_WIDTH_SOURCE::USER );
}


BOOST_AUTO_TEST_CASE( TheDeltaIsZeroRightAfterALocalOriginReset )
{
    PCB_SCREEN screen( VECTOR2I( pcbIUScale.mmToIU( 297 ), pcbIUScale.mmToIU( 210 ) ) );
    screen.m_LocalOrigin = VECTOR2D( 0, 0 );

    const VECTOR2D cursor( pcbIUScale.mmToIU( 10 ), pcbIUScale.mmToIU( 20 ) );

    BOOST_CHECK( HeadsUpDelta( cursor, screen ) == cursor );

    // What common.Control.resetLocalCoords does: the local origin becomes the cursor
    screen.m_LocalOrigin = cursor;

    BOOST_CHECK( HeadsUpDelta( cursor, screen ) == VECTOR2D( 0, 0 ) );

    // ... and the delta counts from there as the cursor moves on
    const VECTOR2D moved = cursor + VECTOR2D( pcbIUScale.mmToIU( 3 ), pcbIUScale.mmToIU( -4 ) );

    BOOST_CHECK( HeadsUpDelta( moved, screen ) == VECTOR2D( pcbIUScale.mmToIU( 3 ), pcbIUScale.mmToIU( -4 ) ) );

    HEADS_UP_STATE state = sampleState( EDA_UNITS::MM );
    state.delta = HeadsUpDelta( moved, screen );

    BOOST_CHECK_EQUAL( BuildHeadsUpLines( state )[1].text, wxS( "dX 3.0000  dY -4.0000  dist 5.0000 mm" ) );
}


BOOST_AUTO_TEST_CASE( EqualStatesSayTheSameAndTheWindowCanSkipThem )
{
    HEADS_UP_STATE a = sampleState( EDA_UNITS::MM );
    HEADS_UP_STATE b = sampleState( EDA_UNITS::MM );

    BOOST_CHECK( a == b );

    b.cursor.x += 1;
    BOOST_CHECK( !( a == b ) );

    b = a;
    b.route = HEADS_UP_ROUTE();
    BOOST_CHECK( !( a == b ) );
}


namespace
{
/// A board with a pad of U3 on net GND, a track of the same net beside it and a zone of another net under both
struct VIEWED_BOARD
{
    VIEWED_BOARD()
    {
        board.SetCopperLayerCount( 2 );

        gnd = new NETINFO_ITEM( &board, wxS( "GND" ), 1 );
        board.Add( gnd );
        vcc = new NETINFO_ITEM( &board, wxS( "VCC" ), 2 );
        board.Add( vcc );

        footprint = new FOOTPRINT( &board );
        footprint->SetReference( wxS( "U3" ) );
        footprint->SetPosition( VECTOR2I( 0, 0 ) );

        pad = new PAD( footprint );
        pad->SetPosition( VECTOR2I( 0, 0 ) );
        pad->SetSize( F_Cu, VECTOR2I( pcbIUScale.mmToIU( 1.6 ), pcbIUScale.mmToIU( 1.6 ) ) );
        pad->SetLayerSet( PAD::SMDMask() );
        pad->SetNet( gnd );
        footprint->Add( pad );
        board.Add( footprint );

        track = new PCB_TRACK( &board );
        track->SetStart( VECTOR2I( pcbIUScale.mmToIU( 5 ), 0 ) );
        track->SetEnd( VECTOR2I( pcbIUScale.mmToIU( 10 ), 0 ) );
        track->SetWidth( pcbIUScale.mmToIU( 0.4 ) );
        track->SetLayer( F_Cu );
        track->SetNet( vcc );
        board.Add( track );

        view.Add( footprint );
        footprint->RunOnChildren(
                [this]( BOARD_ITEM* aChild )
                {
                    view.Add( aChild );
                },
                RECURSE_MODE::NO_RECURSE );
        view.Add( track );

        for( int layer : { F_Cu, B_Cu, F_Fab, F_CrtYd } )
            view.SetLayerVisible( layer, true );

        view.SetLayerVisible( LAYER_PADS, true );
        view.SetLayerVisible( LAYER_FOOTPRINTS_FR, true );
        view.SetLayerVisible( LAYER_TRACKS, true );
    }

    // The view outlives the board, whose items tell it when they go
    KIGFX::VIEW   view;
    BOARD         board;
    NETINFO_ITEM* gnd;
    NETINFO_ITEM* vcc;
    FOOTPRINT*    footprint;
    PAD*          pad;
    PCB_TRACK*    track;
};
} // namespace


BOOST_FIXTURE_TEST_CASE( ThePadUnderThePointGivesItsNetAndItsFootprintsReference, VIEWED_BOARD )
{
    const HEADS_UP_ITEM found = FindHeadsUpItem( view, VECTOR2I( 0, 0 ), pcbIUScale.mmToIU( 0.01 ), F_Cu );

    BOOST_CHECK_EQUAL( found.netName, wxS( "GND" ) );
    BOOST_CHECK_EQUAL( found.reference, wxS( "U3" ) );
}


BOOST_FIXTURE_TEST_CASE( ATrackGivesItsNetAndNoReference, VIEWED_BOARD )
{
    const HEADS_UP_ITEM found =
            FindHeadsUpItem( view, VECTOR2I( pcbIUScale.mmToIU( 7 ), 0 ), pcbIUScale.mmToIU( 0.01 ), F_Cu );

    BOOST_CHECK_EQUAL( found.netName, wxS( "VCC" ) );
    BOOST_CHECK( found.reference.IsEmpty() );
}


BOOST_FIXTURE_TEST_CASE( EmptyBoardUnderThePointGivesNothing, VIEWED_BOARD )
{
    const HEADS_UP_ITEM found =
            FindHeadsUpItem( view, VECTOR2I( pcbIUScale.mmToIU( 30 ), pcbIUScale.mmToIU( 30 ) ), 1000, F_Cu );

    BOOST_CHECK( found == HEADS_UP_ITEM() );
}


BOOST_FIXTURE_TEST_CASE( TheMarginIsHowFarOffAnItemTheCursorMayBe, VIEWED_BOARD )
{
    // The track is 0.4 mm wide, so a point 0.3 mm off its axis is 0.1 mm off its edge
    const VECTOR2I off( pcbIUScale.mmToIU( 7 ), pcbIUScale.mmToIU( 0.3 ) );

    BOOST_CHECK( FindHeadsUpItem( view, off, pcbIUScale.mmToIU( 0.01 ), F_Cu ).netName.IsEmpty() );
    BOOST_CHECK_EQUAL( FindHeadsUpItem( view, off, pcbIUScale.mmToIU( 0.15 ), F_Cu ).netName, wxS( "VCC" ) );
}


BOOST_FIXTURE_TEST_CASE( ALayerTheViewDoesNotDrawIsNotHit, VIEWED_BOARD )
{
    view.SetLayerVisible( F_Cu, false );
    view.SetLayerVisible( LAYER_TRACKS, false );

    BOOST_CHECK( FindHeadsUpItem( view, VECTOR2I( pcbIUScale.mmToIU( 7 ), 0 ), 1000, F_Cu ).netName.IsEmpty() );
}


BOOST_AUTO_TEST_CASE( TheDisplaySettingFollowsTheProfileUntilItIsToggled )
{
    BOOST_CHECK( IsHeadsUpShown( HEADS_UP_FOLLOW_PROFILE, INTERACTION_PROFILE_ID::ALTIUM ) );
    BOOST_CHECK( !IsHeadsUpShown( HEADS_UP_FOLLOW_PROFILE, INTERACTION_PROFILE_ID::STOCK ) );

    for( INTERACTION_PROFILE_ID profile : { INTERACTION_PROFILE_ID::STOCK, INTERACTION_PROFILE_ID::ALTIUM } )
    {
        BOOST_CHECK( IsHeadsUpShown( HEADS_UP_ON, profile ) );
        BOOST_CHECK( !IsHeadsUpShown( HEADS_UP_OFF, profile ) );

        // A value that is none of the three follows the profile
        BOOST_CHECK_EQUAL( IsHeadsUpShown( 7, profile ), IsHeadsUpShown( HEADS_UP_FOLLOW_PROFILE, profile ) );
    }

    BOOST_CHECK_EQUAL( ToggledHeadsUpSetting( HEADS_UP_FOLLOW_PROFILE, INTERACTION_PROFILE_ID::ALTIUM ), HEADS_UP_OFF );
    BOOST_CHECK_EQUAL( ToggledHeadsUpSetting( HEADS_UP_FOLLOW_PROFILE, INTERACTION_PROFILE_ID::STOCK ), HEADS_UP_ON );
    BOOST_CHECK_EQUAL( ToggledHeadsUpSetting( HEADS_UP_ON, INTERACTION_PROFILE_ID::STOCK ), HEADS_UP_OFF );
    BOOST_CHECK_EQUAL( ToggledHeadsUpSetting( HEADS_UP_OFF, INTERACTION_PROFILE_ID::ALTIUM ), HEADS_UP_ON );
}


BOOST_AUTO_TEST_CASE( TheDisplayAndFollowSettingsStartAtTheirDefaults )
{
    PCBNEW_SETTINGS settings;

    // An int that follows the profile and not a bool whose default would have to
    BOOST_CHECK_EQUAL( settings.m_HeadsUpMode, HEADS_UP_FOLLOW_PROFILE );
    BOOST_CHECK( !settings.m_IsHeadsUpFollowingCursor );

    // Both are stored under the viewer settings' aui section, the display as the int it is
    settings.Store();

    BOOST_CHECK_EQUAL( settings.Internals()->Get<int>( "aui.heads_up_display" ), -1 );
    BOOST_CHECK_EQUAL( settings.Internals()->Get<bool>( "aui.heads_up_follows_cursor" ), false );

    settings.m_HeadsUpMode = HEADS_UP_OFF;
    settings.m_IsHeadsUpFollowingCursor = true;
    settings.Store();

    BOOST_CHECK_EQUAL( settings.Internals()->Get<int>( "aui.heads_up_display" ), 0 );
    BOOST_CHECK_EQUAL( settings.Internals()->Get<bool>( "aui.heads_up_follows_cursor" ), true );
}


BOOST_AUTO_TEST_SUITE_END()


namespace
{
struct HEADS_UP_ACTION
{
    TOOL_ACTION* m_action;
    std::string  m_name;
    int          m_altiumKey;
};


std::vector<HEADS_UP_ACTION> headsUpActions()
{
    return { { &PCB_ACTIONS::toggleHeadsUp, "pcbnew.Control.toggleHeadsUp", MD_SHIFT + 'H' },
             { &PCB_ACTIONS::toggleHeadsUpFollow, "pcbnew.Control.toggleHeadsUpFollow", MD_SHIFT + 'G' },
             { &PCB_ACTIONS::resetHeadsUpDelta, "pcbnew.Control.resetHeadsUpDelta", WXK_INSERT } };
}


int keyUnderProfile( INTERACTION_PROFILE_ID aProfile, const std::string& aName, std::pair<int, int> aCompiledKeys,
                     const std::string& aEditorScope, bool aIsContextAction = false )
{
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( aProfile );

    return INTERACTION_PROFILE::ApplyHotKeyLayer( layer, aName, aCompiledKeys, aEditorScope, aIsContextAction ).first;
}
} // namespace


BOOST_AUTO_TEST_SUITE( HeadsUpKeys )


BOOST_AUTO_TEST_CASE( EveryHeadsUpActionIsRegisteredAndNamedAsTheTableNamesIt )
{
    const std::list<TOOL_ACTION*>& registered = ACTION_MANAGER::GetActionList();

    for( const HEADS_UP_ACTION& entry : headsUpActions() )
    {
        BOOST_CHECK_EQUAL( entry.m_action->GetName(), entry.m_name );
        BOOST_CHECK_MESSAGE( std::find( registered.begin(), registered.end(), entry.m_action ) != registered.end(),
                             entry.m_name << " is not registered" );
    }
}


BOOST_AUTO_TEST_CASE( TheHeadsUpActionsAreBoundInTheAltiumProfileAndNotInStock )
{
    for( const HEADS_UP_ACTION& entry : headsUpActions() )
    {
        const std::pair<int, int> compiled = { entry.m_action->GetHotKey(), entry.m_action->GetHotKeyAlt() };

        BOOST_CHECK_MESSAGE( keyUnderProfile( INTERACTION_PROFILE_ID::ALTIUM, entry.m_name, compiled, "pcbnew." )
                                     == entry.m_altiumKey,
                             entry.m_name << " is not on its Altium-style key" );

        // The stock profile is untouched: the new actions have no compiled key, so none is bound
        BOOST_CHECK_MESSAGE( keyUnderProfile( INTERACTION_PROFILE_ID::STOCK, entry.m_name, compiled, "pcbnew." ) == 0,
                             entry.m_name << " is bound in the stock profile" );
        BOOST_CHECK_EQUAL( entry.m_action->GetHotKey(), 0 );
    }
}


BOOST_AUTO_TEST_CASE( ShiftGFollowsTheCursorInThePcbEditorsAndStillStepsTheGridInTheSchematicEditor )
{
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    // The table holds Shift+G for the previous grid, which the schematic editor keeps, and for the display following
    // the cursor, a pcbnew. action ...
    BOOST_CHECK( layer.m_bindings.at( "common.Control.gridPrev" ).first == MD_SHIFT + 'G' );
    BOOST_CHECK( layer.m_bindings.at( "pcbnew.Control.toggleHeadsUpFollow" ).first == MD_SHIFT + 'G' );

    // ... and in the PCB editors the second wins, as the grid list's G does over the next grid: an action of the frame
    // whose key is not its compiled one is promoted ahead of a common. action on the same key
    BOOST_CHECK( PCB_ACTIONS::toggleHeadsUpFollow.GetName().starts_with( "pcbnew." ) );
    BOOST_CHECK_EQUAL( PCB_ACTIONS::toggleHeadsUpFollow.GetHotKey(), 0 );
    BOOST_CHECK( PCB_ACTIONS::toggleHeadsUpFollow.GetScope() == AS_GLOBAL );

    // Shift+H has no other owner anywhere in the table
    for( const auto& [name, keys] : layer.m_bindings )
    {
        if( name != "pcbnew.Control.toggleHeadsUp" )
            BOOST_CHECK_MESSAGE( keys.first != MD_SHIFT + 'H' && keys.second != MD_SHIFT + 'H',
                                 name << " also holds Shift+H" );
    }
}


BOOST_AUTO_TEST_CASE( InsertZeroesTheDeltaAndSpaceKeepsOnlyItsRoleInACommand )
{
    PROFILE_HOTKEY_LAYER layer = INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM );

    // Stock KiCad binds resetLocalCoords to Space, and the Altium-style profile has taken Space in both editors
    BOOST_CHECK_EQUAL( ACTIONS::resetLocalCoords.GetHotKey(), ' ' );
    BOOST_CHECK_EQUAL(
            keyUnderProfile( INTERACTION_PROFILE_ID::STOCK, "common.Control.resetLocalCoords", { ' ', 0 }, "pcbnew." ),
            ' ' );
    BOOST_CHECK_EQUAL(
            keyUnderProfile( INTERACTION_PROFILE_ID::ALTIUM, "common.Control.resetLocalCoords", { ' ', 0 }, "pcbnew." ),
            0 );
    BOOST_CHECK_EQUAL( keyUnderProfile( INTERACTION_PROFILE_ID::ALTIUM, "common.Control.resetLocalCoords", { ' ', 0 },
                                        "eeschema." ),
                       0 );

    // Space is still what the move and placement loops take, as context aliases
    BOOST_CHECK( layer.m_bindings.at( "pcbnew.InteractiveEdit.rotateCcwWhileMoving" ).first == ' ' );
    BOOST_CHECK( PCB_ACTIONS::rotateCcwWhileMoving.GetScope() == AS_CONTEXT );

    // The new action is bound to Insert, in the PCB editors alone: the schematic's Repeat Last Item keeps its key
    BOOST_CHECK_EQUAL( keyUnderProfile( INTERACTION_PROFILE_ID::ALTIUM, "eeschema.InteractiveEdit.repeatDrawItem",
                                        { WXK_INSERT, 0 }, "eeschema." ),
                       WXK_INSERT );

    // ... and Insert leaves the point editor's Create Corner in the PCB editors, which the table has no row for. Its
    // stock key is Insert except on macOS, where it is F1, which the profile leaves alone. Both cases are checked on
    // every platform, since the layer is given the compiled keys.
#ifdef __WXMAC__
    const int stockAddCornerKey = WXK_F1;
#else
    const int stockAddCornerKey = WXK_INSERT;
#endif
    BOOST_CHECK_EQUAL( PCB_ACTIONS::pointEditorAddCorner.GetHotKey(), stockAddCornerKey );
    BOOST_CHECK_EQUAL( keyUnderProfile( INTERACTION_PROFILE_ID::ALTIUM, "pcbnew.PointEditor.addCorner",
                                        { WXK_INSERT, 0 }, "pcbnew." ),
                       0 );
    BOOST_CHECK_EQUAL(
            keyUnderProfile( INTERACTION_PROFILE_ID::ALTIUM, "pcbnew.PointEditor.addCorner", { WXK_F1, 0 }, "pcbnew." ),
            WXK_F1 );
}


BOOST_AUTO_TEST_SUITE_END()


/**
 * The cost of the display on a board of the user's choosing, for the pull request that introduced it. It does nothing
 * unless RECKLESS_HEADS_UP_BOARD names a board file, so the suites never pay for it.
 */
BOOST_AUTO_TEST_SUITE( HeadsUpCost )


BOOST_AUTO_TEST_CASE( MeasureTheWorkOneCursorMoveCostsOnABoard )
{
    const char* path = std::getenv( "RECKLESS_HEADS_UP_BOARD" );

    if( !path )
        return;

    // The view outlives the board, whose items tell it when they go
    KIGFX::VIEW view;

    IO_RELEASER<PCB_IO>    plugin( PCB_IO_MGR::FindPlugin( PCB_IO_MGR::KICAD_SEXP ) );
    std::unique_ptr<BOARD> board = plugin->LoadBoard( wxString::FromUTF8( path ), nullptr );
    BOOST_REQUIRE( board );

    for( BOARD_ITEM* item : board->Tracks() )
        view.Add( item );

    for( FOOTPRINT* footprint : board->Footprints() )
    {
        view.Add( footprint );
        footprint->RunOnChildren(
                [&]( BOARD_ITEM* aChild )
                {
                    view.Add( aChild );
                },
                RECURSE_MODE::NO_RECURSE );
    }

    for( ZONE* zone : board->Zones() )
        view.Add( zone );

    for( int layer = 0; layer < LAYER_ID_COUNT; layer++ )
        view.SetLayerVisible( layer, true );

    const BOX2I box = board->GetBoundingBox();
    const int   moves = 20000;

    HEADS_UP_STATE state = sampleState( EDA_UNITS::MM );
    size_t         hits = 0;

    auto start = std::chrono::steady_clock::now();

    for( int move = 0; move < moves; move++ )
    {
        const VECTOR2I position(
                box.GetX() + ( static_cast<long long>( move ) * 7919 ) % std::max<long long>( 1, box.GetWidth() ),
                box.GetY() + ( static_cast<long long>( move ) * 104729 ) % std::max<long long>( 1, box.GetHeight() ) );

        state.cursor = position;
        state.item = FindHeadsUpItem( view, position, pcbIUScale.mmToIU( 0.05 ), F_Cu );
        hits += !state.item.netName.IsEmpty();

        const std::vector<HEADS_UP_LINE> lines = BuildHeadsUpLines( state );
        hits += lines.size() == 0;
    }

    auto elapsed = std::chrono::steady_clock::now() - start;

    const double milliseconds = std::chrono::duration<double, std::milli>( elapsed ).count();

    std::ostringstream report;
    report << "board " << path << ": " << board->Tracks().size() << " tracks, " << board->Footprints().size()
           << " footprints; " << moves << " moves in " << milliseconds << " ms, " << milliseconds * 1000.0 / moves
           << " us per move (" << hits << " hits)";

    BOOST_TEST_MESSAGE( report.str() );
}


BOOST_AUTO_TEST_SUITE_END()

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

#include <board.h>
#include <footprint.h>
#include <interaction/pcb_context_menu_altium.h>
#include <interaction/pcb_menus_altium.h>
#include <pad.h>
#include <pcb_track.h>
#include <tool/action_manager.h>
#include <tool/action_menu.h>
#include <tool/actions.h>
#include <tool/conditional_menu.h>
#include <tools/pcb_actions.h>
#include <tools/pcb_selection.h>
#include <zone.h>

#include "../common/context_menu_checks.h"
#include "../common/menu_tree_checks.h"

// The PCB editor's right-click tree for the Altium-style profile (interaction-profile note, 5.4 and 6.6)

using namespace CONTEXT_MENU_CHECKS;

namespace
{
/// The tools' sources that put actions into the idle context menu, and the menus of a command they also fill
const std::vector<STOCK_MENU_SOURCE>& stockSources()
{
    static const std::vector<STOCK_MENU_SOURCE> sources = {
        { "pcbnew/tools/pcb_selection_tool.cpp", {} },
        { "pcbnew/tools/board_editor_control.cpp", { "ctxMenu" } },
        { "pcbnew/generators/via_stitch_tool.cpp", {} },
        { "pcbnew/tools/align_distribute_tool.cpp", {} },
        { "pcbnew/tools/constraint_edit_tool.cpp", {} },
        { "pcbnew/tools/pad_tool.cpp", { "ctxMenu" } },
        { "pcbnew/tools/multichannel_tool.cpp", {} },
        { "pcbnew/tools/pcb_point_editor.cpp", {} },
        { "pcbnew/tools/match_properties_tool.cpp", {} },
        { "pcbnew/tools/generator_tool.cpp", {} },
        { "pcbnew/tools/edit_tool.cpp", {} },
        { "pcbnew/tools/convert_tool.cpp", {} },
        { "pcbnew/tools/board_inspection_tool.cpp", {} },
        { "include/tool/edit_table_tool_base.h", {} },
        { "common/tool/group_tool.cpp", {} },
        { "common/tool/grid_menu.cpp", {} },
    };

    return sources;
}


const std::vector<std::string>& definitionFiles()
{
    static const std::vector<std::string> files = { "common/tool/actions.cpp", "pcbnew/tools/pcb_actions.cpp" };
    return files;
}


/// What KiCad appends to every context menu and the tree leaves out on purpose
const std::vector<MENU_DROP>& deliberateDrops()
{
    static const std::vector<MENU_DROP> drops = {
        { "PCB_ACTIONS::skip",
          "shown only inside the Move Individually command, whose menu is left as KiCad builds it" },
        { "ACTIONS::gridOrigin",
          "the Grid submenu is dropped (keys G and V); the origin is set with Edit, Origin, Set" },
    };

    return drops;
}


/// Actions of a submenu that KiCad's tool builds at run time and the tree shows as it is (a slot): the tree has no entry
const std::vector<MENU_DROP>& reachedByShared()
{
    static const std::vector<MENU_DROP> shared = {};

    return shared;
}
} // namespace


BOOST_AUTO_TEST_SUITE( PcbAltiumContextMenuTree )


BOOST_AUTO_TEST_CASE( TheTreeBuildsAndEveryActionIsRegistered )
{
    MENU_DEF tree = PcbAltiumContextMenu( nullptr, nullptr );

    MENU_TREE_CHECKS::ExpectNoProblems( MENU_TREE_CHECKS::ActionProblems( tree, 100 ),
                                        "every action of the PCB right-click tree is registered" );
}


BOOST_AUTO_TEST_CASE( TheTopLevelFollowsTheNote )
{
    MENU_DEF tree = PcbAltiumContextMenu( nullptr, nullptr );

    // The note's first entries, past a Cancel that shows only while a tool is active
    std::vector<const MENU_DEF*> entries;

    for( const MENU_DEF& entry : tree.m_children )
        entries.push_back( &entry );

    BOOST_REQUIRE_GT( entries.size(), 10u );
    BOOST_CHECK( entries[1]->m_action == &PCB_ACTIONS::findByProperties );
    BOOST_CHECK( entries[2]->m_action == &PCB_ACTIONS::clearHighlight );
    BOOST_CHECK( entries[3]->IsSeparator() );
    BOOST_CHECK( entries.back()->m_action == &PCB_ACTIONS::properties );

    std::vector<wxString> order;

    for( const MENU_DEF* entry : entries )
    {
        if( entry->IsSubmenu() )
            order.push_back( entry->m_label );
    }

    auto position = [&]( const wxString& aLabel )
    {
        auto found = std::find( order.begin(), order.end(), aLabel );
        BOOST_REQUIRE_MESSAGE( found != order.end(), aLabel << " is missing" );
        return found - order.begin();
    };

    // The object form's fixed order, Altium's own submenus then KiCad's for their kind, then Align
    BOOST_CHECK_LT( position( "Place" ), position( "Unions" ) );
    BOOST_CHECK_LT( position( "Unions" ), position( "Component Actions" ) );
    BOOST_CHECK_LT( position( "Component Actions" ), position( "Polygon Actions" ) );
    BOOST_CHECK_LT( position( "Polygon Actions" ), position( "Net Actions" ) );
    BOOST_CHECK_LT( position( "Net Actions" ), position( "Pad Actions" ) );
    BOOST_CHECK_LT( position( "Pad Actions" ), position( "Track Actions" ) );
    BOOST_CHECK_LT( position( "Track Actions" ), position( "Convert" ) );
    BOOST_CHECK_LT( position( "Convert" ), position( "Align" ) );
    BOOST_CHECK_LT( position( "Align" ), position( "Violations" ) );
}


BOOST_AUTO_TEST_CASE( TheFormFollowsWhatIsUnderTheCursor )
{
    KI_TEST::DISPLAY_FOR_WIDGETS display;

    if( !display.IsAvailable() )
    {
        BOOST_TEST_MESSAGE( "Skipping test - no display available" );
        return;
    }

    ACTION_MANAGER actionManager( nullptr ); // gives every action its id, which a menu entry needs

    std::unique_ptr<CONDITIONAL_MENU> menu( BuildConditionalMenu( PcbAltiumContextMenu( nullptr, nullptr ), nullptr ) );

    BOARD     board;
    PCB_TRACK track( &board );
    FOOTPRINT footprint( &board );
    ZONE      zone( &board );
    PAD       pad( &footprint );

    // Free space: routing, Place and the clipboard
    PCB_SELECTION selection;
    menu->Evaluate( selection );

    BOOST_CHECK( HasAction( *menu, PCB_ACTIONS::findByProperties ) );
    BOOST_CHECK( HasAction( *menu, PCB_ACTIONS::routeSingleTrack ) );
    BOOST_CHECK( HasAction( *menu, PCB_ACTIONS::tuneSingleTrack ) );
    BOOST_CHECK( Submenu( *menu, "Place" ) );
    BOOST_CHECK( HasAction( *menu, ACTIONS::cut ) );
    BOOST_CHECK( !HasAction( *menu, PCB_ACTIONS::toggleLock ) );
    BOOST_CHECK( !Submenu( *menu, "Component Actions" ) );
    BOOST_CHECK( !Submenu( *menu, "Polygon Actions" ) );
    BOOST_CHECK( !HasAction( *menu, PCB_ACTIONS::properties ) );

    // A track: the fixed order, with Component and Polygon Actions in their places, and Net and Track Actions
    selection.Add( &track );
    menu->Evaluate( selection );

    BOOST_CHECK( !HasAction( *menu, PCB_ACTIONS::routeSingleTrack ) );
    BOOST_CHECK( HasAction( *menu, PCB_ACTIONS::toggleLock ) );
    BOOST_CHECK( Submenu( *menu, "Component Actions" ) );
    BOOST_CHECK( Submenu( *menu, "Polygon Actions" ) );

    wxMenu* netActions = Submenu( *menu, "Net Actions" );
    BOOST_REQUIRE( netActions );
    BOOST_CHECK( HasAction( *netActions, PCB_ACTIONS::selectNet ) );

    wxMenu* trackActions = Submenu( *menu, "Track Actions" );
    BOOST_REQUIRE( trackActions );
    BOOST_CHECK( HasAction( *trackActions, PCB_ACTIONS::breakTrack ) );
    BOOST_CHECK( !Submenu( *menu, "Pad Actions" ) );
    BOOST_CHECK( HasAction( *menu, PCB_ACTIONS::properties ) );

    // A footprint: no net, no track
    PCB_SELECTION footprintSelection;
    footprintSelection.Add( &footprint );
    menu->Evaluate( footprintSelection );

    wxMenu* componentActions = Submenu( *menu, "Component Actions" );
    BOOST_REQUIRE( componentActions );
    BOOST_CHECK( HasAction( *componentActions, PCB_ACTIONS::updateFootprint ) );
    BOOST_CHECK( !Submenu( *menu, "Net Actions" ) );
    BOOST_CHECK( !Submenu( *menu, "Track Actions" ) );

    // A zone: Polygon Actions has the entries that need one
    PCB_SELECTION zoneSelection;
    zoneSelection.Add( &zone );
    menu->Evaluate( zoneSelection );

    wxMenu* polygonActions = Submenu( *menu, "Polygon Actions" );
    BOOST_REQUIRE( polygonActions );
    BOOST_CHECK( HasAction( *polygonActions, PCB_ACTIONS::zoneFill ) );

    PCB_SELECTION trackSelection;
    trackSelection.Add( &track );
    menu->Evaluate( trackSelection );

    polygonActions = Submenu( *menu, "Polygon Actions" );
    BOOST_REQUIRE( polygonActions );
    BOOST_CHECK( !HasAction( *polygonActions, PCB_ACTIONS::zoneFill ) );
    BOOST_CHECK( HasAction( *polygonActions, PCB_ACTIONS::zoneFillAll ) );
}


BOOST_AUTO_TEST_CASE( NothingKiCadsOwnRightClickOffersIsLost )
{
    std::map<std::string, std::string> names = NamesByIdentifier( definitionFiles() );

    BOOST_REQUIRE_GT( names.size(), 300u );

    std::set<std::string> stock = StockIdentifiers( stockSources(), names );
    std::set<std::string> altium = TreeIdentifiers( PcbAltiumContextMenu( nullptr, nullptr ), names );

    BOOST_REQUIRE_GT( stock.size(), 50u );

    for( const std::string& problem : Problems( stock, altium, deliberateDrops(), reachedByShared() ) )
        BOOST_ERROR( problem );
}


BOOST_AUTO_TEST_SUITE_END()

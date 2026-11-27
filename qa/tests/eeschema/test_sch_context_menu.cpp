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

#include <interaction/sch_context_menu_altium.h>
#include <interaction/sch_menus_altium.h>
#include <sch_line.h>
#include <sch_sheet.h>
#include <sch_symbol.h>
#include <tool/action_manager.h>
#include <tool/action_menu.h>
#include <tool/actions.h>
#include <tool/conditional_menu.h>
#include <tools/sch_actions.h>
#include <tools/sch_selection.h>

#include "../common/context_menu_checks.h"
#include "../common/menu_tree_checks.h"

// The schematic editor's right-click tree for the Altium-style profile (interaction-profile note, 5.4 and 6.6)

using namespace CONTEXT_MENU_CHECKS;

namespace
{
/// The tools' sources that put actions into the idle context menu, and the menus of a command they also fill
const std::vector<STOCK_MENU_SOURCE>& stockSources()
{
    static const std::vector<STOCK_MENU_SOURCE> sources = {
        { "eeschema/tools/sch_selection_tool.cpp", {} },
        { "eeschema/tools/sch_edit_tool.cpp", { "moveMenu", "drawMenu" } },
        { "eeschema/tools/sch_align_tool.cpp", {} },
        { "eeschema/tools/sch_inspection_tool.cpp", {} },
        { "eeschema/tools/sch_move_tool.cpp", {} },
        { "eeschema/tools/sch_point_editor.cpp", {} },
        { "include/tool/edit_table_tool_base.h", {} },
        { "common/tool/group_tool.cpp", {} },
        { "common/tool/grid_menu.cpp", {} },
    };

    return sources;
}


const std::vector<std::string>& definitionFiles()
{
    static const std::vector<std::string> files = { "common/tool/actions.cpp", "eeschema/tools/sch_actions.cpp" };
    return files;
}


/// What KiCad appends to every context menu and the tree leaves out on purpose
const std::vector<MENU_DROP>& deliberateDrops()
{
    static const std::vector<MENU_DROP> drops = {
        { "ACTIONS::gridOrigin",
          "the Grid submenu is dropped (keys G and V); the origin is set with Edit, Origin, Set" },
        { "SCH_ACTIONS::symbolProperties", "shown in the symbol editor only, which keeps KiCad's menus" },
        { "SCH_ACTIONS::pinTable", "shown in the symbol editor only, which keeps KiCad's menus" },
    };

    return drops;
}


/// Actions of a submenu that KiCad's tool builds at run time and the tree shows as it is (a slot): the tree has no entry
const std::vector<MENU_DROP>& reachedByShared()
{
    static const std::vector<MENU_DROP> shared = {
        { "SCH_ACTIONS::createNetChain", "the Net Chain submenu is shown as the selection tool builds it" },
        { "SCH_ACTIONS::highlightNetChain", "the Net Chain submenu is shown as the selection tool builds it" },
        { "SCH_ACTIONS::removeFromNetChain", "the Net Chain submenu is shown as the selection tool builds it" },
        { "SCH_ACTIONS::nameNetChain", "the Net Chain submenu is shown as the selection tool builds it" },
        { "SCH_ACTIONS::createNetChainBetweenPins", "the Net Chain submenu is shown as the selection tool builds it" },
    };

    return shared;
}
} // namespace


BOOST_AUTO_TEST_SUITE( SchAltiumContextMenuTree )


BOOST_AUTO_TEST_CASE( TheTreeBuildsAndEveryActionIsRegistered )
{
    MENU_DEF tree = SchAltiumContextMenu( nullptr, nullptr );

    MENU_TREE_CHECKS::ExpectNoProblems( MENU_TREE_CHECKS::ActionProblems( tree, 100 ),
                                        "every action of the schematic right-click tree is registered" );
}


BOOST_AUTO_TEST_CASE( TheTopLevelFollowsTheNote )
{
    MENU_DEF tree = SchAltiumContextMenu( nullptr, nullptr );

    BOOST_REQUIRE_GT( tree.m_children.size(), 10u );

    // First and last
    BOOST_CHECK( tree.m_children.front().m_action == &ACTIONS::find );
    BOOST_CHECK( tree.m_children[1].m_action == &SCH_ACTIONS::clearHighlight );
    BOOST_CHECK( tree.m_children.back().m_action == &SCH_ACTIONS::properties );
    BOOST_CHECK( tree.m_children[2].IsSeparator() );

    // The submenus keep the note's order: Altium's own, then KiCad's for their kind, then Align
    std::vector<wxString> order;

    for( const MENU_DEF& entry : tree.m_children )
    {
        if( entry.IsSubmenu() )
            order.push_back( entry.m_label );
    }

    auto position = [&]( const wxString& aLabel )
    {
        auto found = std::find( order.begin(), order.end(), aLabel );
        BOOST_REQUIRE_MESSAGE( found != order.end(), aLabel << " is missing" );
        return found - order.begin();
    };

    BOOST_CHECK_LT( position( "Place" ), position( "Part Actions" ) );
    BOOST_CHECK_LT( position( "Part Actions" ), position( "Sheet Symbol Actions" ) );
    BOOST_CHECK_LT( position( "Sheet Symbol Actions" ), position( "Sheet Actions" ) );
    BOOST_CHECK_LT( position( "Sheet Actions" ), position( "Pin Actions" ) );
    BOOST_CHECK_LT( position( "Pin Actions" ), position( "Wire Actions" ) );
    BOOST_CHECK_LT( position( "Wire Actions" ), position( "Port Actions" ) );
    BOOST_CHECK_LT( position( "Port Actions" ), position( "Align" ) );
    BOOST_CHECK_LT( position( "Align" ), position( "Unions" ) );
}


BOOST_AUTO_TEST_CASE( TheSubmenusAppearForTheirKindOfObject )
{
    KI_TEST::DISPLAY_FOR_WIDGETS display;

    if( !display.IsAvailable() )
    {
        BOOST_TEST_MESSAGE( "Skipping test - no display available" );
        return;
    }

    ACTION_MANAGER actionManager( nullptr ); // gives every action its id, which a menu entry needs

    std::unique_ptr<CONDITIONAL_MENU> menu( BuildConditionalMenu( SchAltiumContextMenu( nullptr, nullptr ), nullptr ) );

    SCH_SYMBOL symbol;
    SCH_SYMBOL otherSymbol;
    SCH_SHEET  sheet;
    SCH_LINE   wire( VECTOR2I( 0, 0 ), LAYER_WIRE );

    // Nothing selected: the base menu, whose entries never move
    SCH_SELECTION selection;
    menu->Evaluate( selection );

    BOOST_CHECK( Submenu( *menu, "Place" ) );
    BOOST_CHECK( Submenu( *menu, "Part Actions" ) );
    BOOST_CHECK( Submenu( *menu, "Sheet Actions" ) );
    BOOST_CHECK( !Submenu( *menu, "Sheet Symbol Actions" ) );
    BOOST_CHECK( !Submenu( *menu, "Wire Actions" ) );
    BOOST_CHECK( !Submenu( *menu, "Pin Actions" ) );
    BOOST_CHECK( !Submenu( *menu, "Align" ) );
    BOOST_CHECK( !Submenu( *menu, "Unions" ) );
    BOOST_CHECK( HasAction( *menu, ACTIONS::find ) );
    BOOST_CHECK( HasAction( *menu, ACTIONS::cut ) );
    BOOST_CHECK( !HasAction( *menu, SCH_ACTIONS::properties ) );
    BOOST_CHECK( !HasAction( *menu, SCH_ACTIONS::drawWire ) );

    // A symbol: Part Actions has its symbol-only entries
    selection.Add( &symbol );
    menu->Evaluate( selection );

    wxMenu* partActions = Submenu( *menu, "Part Actions" );
    BOOST_REQUIRE( partActions );
    BOOST_CHECK( HasAction( *partActions, SCH_ACTIONS::updateSymbol ) );
    BOOST_CHECK( HasAction( *partActions, SCH_ACTIONS::editWithLibEdit ) );
    BOOST_CHECK( HasAction( *menu, SCH_ACTIONS::properties ) );
    BOOST_CHECK( !Submenu( *menu, "Wire Actions" ) );

    // Align and Unions come with any selection, and Align's entries that need two items come with the second
    BOOST_CHECK( Submenu( *menu, "Align" ) );
    BOOST_CHECK( Submenu( *menu, "Unions" ) );
    BOOST_CHECK( !HasAction( *Submenu( *menu, "Align" ), SCH_ACTIONS::alignLeft ) );
    BOOST_CHECK( HasAction( *Submenu( *menu, "Align" ), SCH_ACTIONS::alignToGrid ) );

    selection.Add( &otherSymbol );
    menu->Evaluate( selection );
    BOOST_CHECK( HasAction( *Submenu( *menu, "Align" ), SCH_ACTIONS::alignLeft ) );

    // A sheet symbol
    SCH_SELECTION sheetSelection;
    sheetSelection.Add( &sheet );
    menu->Evaluate( sheetSelection );

    wxMenu* sheetActions = Submenu( *menu, "Sheet Symbol Actions" );
    BOOST_REQUIRE( sheetActions );
    BOOST_CHECK( HasAction( *sheetActions, SCH_ACTIONS::enterSheet ) );
    BOOST_CHECK( !Submenu( *menu, "Wire Actions" ) );

    // A wire: Wire Actions, and Altium's Wire entry that starts one there
    SCH_SELECTION wireSelection;
    wireSelection.Add( &wire );
    menu->Evaluate( wireSelection );

    wxMenu* wireActions = Submenu( *menu, "Wire Actions" );
    BOOST_REQUIRE( wireActions );
    BOOST_CHECK( HasAction( *wireActions, SCH_ACTIONS::breakWire ) );
    BOOST_CHECK( HasAction( *menu, SCH_ACTIONS::drawWire ) );
    BOOST_CHECK( !Submenu( *menu, "Sheet Symbol Actions" ) );
}


BOOST_AUTO_TEST_CASE( NothingKiCadsOwnRightClickOffersIsLost )
{
    std::map<std::string, std::string> names = NamesByIdentifier( definitionFiles() );

    BOOST_REQUIRE_GT( names.size(), 300u );

    std::set<std::string> stock = StockIdentifiers( stockSources(), names );
    std::set<std::string> altium = TreeIdentifiers( SchAltiumContextMenu( nullptr, nullptr ), names );

    BOOST_REQUIRE_GT( stock.size(), 50u );

    for( const std::string& problem : Problems( stock, altium, deliberateDrops(), reachedByShared() ) )
        BOOST_ERROR( problem );
}


BOOST_AUTO_TEST_SUITE_END()

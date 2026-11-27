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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <interaction/sch_menus_altium.h>
#include <interaction_profile.h>
#include <kiface_base.h>
#include <key_sequence.h>

#include "../common/menu_tree_checks.h"

// The schematic editor's menu tree for the Altium-style profile (interaction-profile note, 5.4 and 6.1). The rows are
// the note's table 6.1 rows marked "=", carried by hand: the names are the registered names the table gives.

namespace
{
const std::vector<MENU_TREE_ROW>& tableRows()
{
    static const std::vector<MENU_TREE_ROW> rows = {
        { "P P", "eeschema.InteractiveDrawing.placeSymbol", true },
        { "P W", "eeschema.InteractiveDrawingLineWireBus.drawWires", true },
        { "P B", "eeschema.InteractiveDrawingLineWireBus.drawBuses", true },
        { "P U", "eeschema.InteractiveDrawing.placeBusWireEntry", true },
        { "P N", "eeschema.InteractiveDrawing.placeLabel", true },
        { "P O", "eeschema.InteractiveDrawing.placePowerSymbol", true },
        { "P R", "eeschema.InteractiveDrawing.placeHierarchicalLabel", true },
        { "P C", "eeschema.InteractiveDrawing.placeGlobalLabel", true },
        { "P S", "eeschema.InteractiveDrawing.drawSheet", true },
        { "P E", "eeschema.InteractiveDrawing.placeSheetPin", true },
        { "P J", "eeschema.InteractiveDrawing.placeJunction", true },
        { "P V N", "eeschema.InteractiveDrawing.placeNoConnect", true },
        { "P V M", "eeschema.InteractiveDrawing.placeClassLabel", true },
        { "P V L", "eeschema.InteractiveDrawing.drawRuleArea", true },
        { "P T", "eeschema.InteractiveDrawing.placeSchematicText", true },
        { "P F", "eeschema.InteractiveDrawing.drawTextBox", true },
        { "P D L", "eeschema.InteractiveDrawingLineWireBus.drawLines", true },
        { "P D R", "eeschema.InteractiveDrawing.drawRectangle", true },
        { "P D A", "eeschema.InteractiveDrawing.drawArc", true },
        { "P D U", "eeschema.InteractiveDrawing.drawCircle", true },
        { "P D E", "eeschema.InteractiveDrawing.drawEllipse", true },
        { "P D Y", "eeschema.InteractiveDrawing.drawPolygon", true },
        { "P D B", "eeschema.InteractiveDrawing.drawBezier", true },
        { "P D G", "eeschema.InteractiveDrawing.placeImage", true },
        { "E D", "common.Interactive.deleteTool", true },
        { "E W", "eeschema.InteractiveEdit.breakWire", true },
        { "M M", "eeschema.InteractiveMove.move", true },
        { "M S", "eeschema.InteractiveMove.move", true },
        { "M D", "eeschema.InteractiveMove.drag", true },
        { "M R", "eeschema.InteractiveMove.drag", true },
        { "M E", "eeschema.InteractiveEdit.rotateCCW", true },
        { "M L", "eeschema.InteractiveEdit.rotateCW", true },
        { "S A", "common.Interactive.selectAll", true },
        { "S C", "eeschema.InteractiveSelection.SelectConnection", true },
        { "S I", "common.Interactive.selectSetRect", true },
        { "S E", "common.Interactive.selectSetLasso", true },
        { "X A", "common.Interactive.unselectAll", true },
        { "A L", "eeschema.Align.alignLeft", true },
        { "A R", "eeschema.Align.alignRight", true },
        { "A T", "eeschema.Align.alignTop", true },
        { "A B", "eeschema.Align.alignBottom", true },
        { "A C", "eeschema.Align.alignCenterX", true },
        { "A V", "eeschema.Align.alignCenterY", true },
        { "A G", "eeschema.AlignToGrid", true },
        { "Z W", "common.Control.zoomTool", true },
        { "Z I", "common.Control.zoomIn", true },
        { "Z O", "common.Control.zoomOut", true },
        { "Z E", "common.Control.zoomFitSelection", true },
        { "Z L", "common.Control.undoZoom", true },
        { "Z A", "common.Control.zoomFitObjects", true },
        { "Z S", "common.Control.zoomFitScreen", true },
        { "J C", "common.Interactive.find", true },
        { "V D", "common.Control.zoomFitScreen", true },
        { "V F", "common.Control.zoomFitObjects", true },
        { "V A", "common.Control.zoomTool", true },
        { "V E", "common.Control.zoomFitSelection", true },
        { "V I", "common.Control.zoomIn", true },
        { "V O", "common.Control.zoomOut", true },
        { "V Z", "common.Control.undoZoom", true },
        { "V G G", "common.Control.gridNext", true },
        { "V G R", "common.Control.gridPrev", true },
        { "V G V", "common.Control.toggleGrid", true },
        { "V G S", "common.Control.editGrids", true },
        { "V U", "common.Control.toggleUnits", true },
        { "C C", "eeschema.InspectionTool.runERC", true },
        { "C O", "eeschema.EditorControl.schematicSetup", true },
        { "D U", "common.Control.updatePcbFromSchematic", true },
        { "D B", "common.Control.showSymbolBrowser", true },
        { "D N", "eeschema.EditorControl.exportNetlist", true },
        { "T A A", "eeschema.EditorControl.annotate", true },
        { "T G", "eeschema.EditorControl.assignFootprints", true },
        { "T R", "eeschema.EditorControl.editSymbolFields", true },
        { "T L", "eeschema.InteractiveEdit.updateSymbols", true },
        { "T C", "eeschema.EditorControl.selectOnPCB", true },
        { "T P", "common.SuiteControl.openPreferences", true },
        { "R I", "eeschema.EditorControl.generateBOM", true },
        { "R M", "common.Interactive.measureTool", true },
        { "O D", "common.Control.pageSettings", true },
        { "O S", "common.Control.pageSettings", true },
        { "O P", "common.SuiteControl.openPreferences", true },
        { "O G", "common.Control.editGrids", true },
        { "F S", "common.Control.save", true },
        { "F P", "common.Control.print", true },
    };

    return rows;
}
} // namespace


BOOST_AUTO_TEST_SUITE( SchAltiumMenuTree )

BOOST_AUTO_TEST_CASE( EveryEntryResolvesToARegisteredAction )
{
    MENU_DEF bar = SchAltiumMenuBar();

    BOOST_CHECK_GE( bar.Actions().size(), 150u );
    BOOST_CHECK_GT( bar.Actions().size(), bar.m_children.size() );
    MENU_TREE_CHECKS::ExpectNoProblems( MENU_TREE_CHECKS::ActionProblems( bar, 150 ),
                                        "a menu entry does not resolve to a registered action" );
}


BOOST_AUTO_TEST_CASE( MnemonicsAreUniqueInEveryMenu )
{
    MENU_DEF bar = SchAltiumMenuBar();

    BOOST_CHECK_GE( bar.m_children.size(), 9u );
    MENU_TREE_CHECKS::ExpectNoProblems( MENU_TREE_CHECKS::MnemonicProblems( bar, 9 ),
                                        "a menu has entries that share a mnemonic, or a menu has none" );
}


BOOST_AUTO_TEST_CASE( NothingTheNoteMarksAsExistingDisappears )
{
    MENU_DEF bar = SchAltiumMenuBar();

    BOOST_CHECK_GE( tableRows().size(), 20u );
    BOOST_CHECK( !SchAltiumMenuOmissions().empty() );
    MENU_TREE_CHECKS::ExpectNoProblems(
            MENU_TREE_CHECKS::DisappearanceProblems( bar, SchAltiumMenuOmissions(), tableRows() ),
            "a row of the note's table is lost, or its keys do not select its action" );
}


BOOST_AUTO_TEST_CASE( EverySequenceTheNoteMarksAsExistingRunsItsActionWhenTyped )
{
    MENU_DEF bar = SchAltiumMenuBar();

    BOOST_CHECK_GE( tableRows().size(), 80u );
    MENU_TREE_CHECKS::ExpectNoProblems(
            MENU_TREE_CHECKS::SequenceProblems( bar, SchAltiumSequenceRoots(), tableRows() ),
            "a sequence of the note's table 6.1 does not run the action the note names" );
}


BOOST_AUTO_TEST_CASE( EveryRootOpensAMenuAndIsBoundToItsLetterInTheProfileTable )
{
    MENU_DEF bar = SchAltiumMenuBar();

    BOOST_CHECK_EQUAL( SchAltiumSequenceRoots().size(), 15u );
    MENU_TREE_CHECKS::ExpectNoProblems(
            MENU_TREE_CHECKS::RootProblems( bar, SchAltiumSequenceRoots(),
                                            INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM ),
                                            tableRows() ),
            "a root is not registered, opens no menu, or is not bound to its letter" );
}


BOOST_AUTO_TEST_CASE( TheKeyOnlyEntriesAreInTheTreeButNotInTheMenuBar )
{
    MENU_DEF bar = SchAltiumMenuBar();

    // The Z popup is a menu of the tree that the bar does not show
    const MENU_DEF* zoom = KEY_SEQUENCE::FindRoot( bar, 'Z' );

    BOOST_REQUIRE( zoom );
    BOOST_CHECK( zoom->m_isKeyOnly );

    int keyOnlyEntries = 0;

    bar.Walk(
            [&]( const MENU_DEF& aNode, const std::vector<wxString>& )
            {
                keyOnlyEntries += aNode.m_isKeyOnly ? 1 : 0;
            } );

    BOOST_CHECK_GE( keyOnlyEntries, 4 );
}


BOOST_AUTO_TEST_CASE( SaveAsAndOpenAreSequencesOnlyWhereTheMenuHasThem )
{
    // File > Save As and Open are entries of a stand-alone editor: under the project manager they are not in the menu,
    // and so are not typed (F A and F O). Save and Print are always there.
    MENU_DEF bar = SchAltiumMenuBar();

    BOOST_CHECK_EQUAL( KEY_SEQUENCE::Resolve( bar, { 'F', 'A' } ) != nullptr, Kiface().IsSingle() );
    BOOST_CHECK_EQUAL( KEY_SEQUENCE::Resolve( bar, { 'F', 'O' } ) != nullptr, Kiface().IsSingle() );
    BOOST_CHECK( KEY_SEQUENCE::Resolve( bar, { 'F', 'S' } ) != nullptr );
    BOOST_CHECK( KEY_SEQUENCE::Resolve( bar, { 'F', 'P' } ) != nullptr );
}


BOOST_AUTO_TEST_CASE( EverySlotIsOneTheFrameFills )
{
    std::vector<std::string> slots = SchAltiumMenuBar().Slots();

    BOOST_REQUIRE_EQUAL( slots.size(), 4u );

    for( const std::string& slot : slots )
    {
        BOOST_CHECK_MESSAGE( slot == SCH_MENU_SLOT::OPEN_RECENT || slot == SCH_MENU_SLOT::EXIT
                                     || slot == SCH_MENU_SLOT::PLUGINS || slot == SCH_MENU_SLOT::LANGUAGES,
                             "the frame does not fill the slot " << slot );
    }
}

BOOST_AUTO_TEST_SUITE_END()

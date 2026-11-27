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

#include <interaction/pcb_menus_altium.h>
#include <interaction_profile.h>
#include <kiface_base.h>
#include <key_sequence.h>

#include "../common/menu_tree_checks.h"

// The PCB editor's menu tree for the Altium-style profile (interaction-profile note, 5.4 and 6.3). The rows are the
// note's table 6.3 rows marked "=", carried by hand: the names are the registered names the table gives.

namespace
{
const std::vector<MENU_TREE_ROW>& tableRows()
{
    static const std::vector<MENU_TREE_ROW> rows = {
        { "P T", "pcbnew.InteractiveRouter.SingleTrack", true },
        { "U T", "pcbnew.InteractiveRouter.SingleTrack", true },
        { "U I", "pcbnew.InteractiveRouter.DiffPair", true },
        { "U R", "pcbnew.LengthTuner.TuneSingleTrack", true },
        { "U P", "pcbnew.LengthTuner.TuneDiffPair", true },
        { "U L", "pcbnew.InteractiveRouter.OptimizeSelected", true },
        { "U U A", "pcbnew.GlobalEdit.globalDeletions", true },
        { "P V", "pcbnew.InteractiveDrawing.via", true },
        { "P C", "pcbnew.EditorControl.placeFootprint", true },
        { "P G", "pcbnew.InteractiveDrawing.zone", true },
        { "P K", "pcbnew.InteractiveDrawing.ruleArea", true },
        { "P L", "pcbnew.InteractiveDrawing.line", true },
        { "P S", "pcbnew.InteractiveDrawing.text", true },
        { "P F", "pcbnew.InteractiveDrawing.rectangle", true },
        { "P E", "pcbnew.InteractiveDrawing.rectangle", true },
        { "P R", "pcbnew.InteractiveDrawing.graphicPolygon", true },
        { "P A A", "pcbnew.InteractiveDrawing.arcCenter", true },
        { "P A E", "pcbnew.InteractiveDrawing.arc", true },
        { "P A U", "pcbnew.InteractiveDrawing.circle", true },
        { "P D L", "pcbnew.InteractiveDrawing.alignedDimension", true },
        { "P D R", "pcbnew.InteractiveDrawing.radialDimension", true },
        { "P D E", "pcbnew.InteractiveDrawing.leader", true },
        { "P D C", "pcbnew.InteractiveDrawing.centerDimension", true },
        { "P P", "pcbnew.PadTool.placePad", false },
        { "E D", "common.Interactive.deleteTool", true },
        { "E O S", "common.Control.gridSetOrigin", true },
        { "E O R", "common.Control.gridResetOrigin", true },
        { "E N", "pcbnew.EditorControl.findByProperties", true },
        { "M M", "pcbnew.InteractiveMove.move", true },
        { "M S", "pcbnew.InteractiveMove.move", true },
        { "M D", "pcbnew.InteractiveRouter.Drag45Degree", true },
        { "M C", "pcbnew.InteractiveEdit.FindMove", true },
        { "M B", "pcbnew.InteractiveRouter.BreakTrack", true },
        { "M X", "pcbnew.InteractiveEdit.moveExact", true },
        { "M I", "pcbnew.InteractiveEdit.flip", true },
        { "M O", "pcbnew.InteractiveEdit.rotateCcw", true },
        { "S A", "common.Interactive.selectAll", true },
        { "S B", "common.Interactive.selectAll", true },
        { "S C", "pcbnew.InteractiveSelection.SelectConnection", true },
        { "S N", "pcbnew.InteractiveSelection.SelectNet", true },
        { "S P", "pcbnew.InteractiveSelection.SelectConnection", true },
        { "S I", "common.Interactive.selectSetRect", true },
        { "S E", "common.Interactive.selectSetLasso", true },
        { "X A", "common.Interactive.unselectAll", true },
        { "A L", "pcbnew.AlignAndDistribute.alignLeft", true },
        { "A R", "pcbnew.AlignAndDistribute.alignRight", true },
        { "A T", "pcbnew.AlignAndDistribute.alignTop", true },
        { "A B", "pcbnew.AlignAndDistribute.alignBottom", true },
        { "A C", "pcbnew.AlignAndDistribute.alignCenterX", true },
        { "A V", "pcbnew.AlignAndDistribute.alignCenterY", true },
        { "A D", "pcbnew.AlignAndDistribute.distributeHorizontallyGaps", true },
        { "A S", "pcbnew.AlignAndDistribute.distributeVerticallyGaps", true },
        { "J C", "common.Interactive.find", true },
        { "J N", "common.Interactive.find", true },
        { "J P", "common.Interactive.find", true },
        { "V I", "common.Control.zoomIn", true },
        { "V O", "common.Control.zoomOut", true },
        { "V F", "common.Control.zoomFitObjects", true },
        { "V D", "common.Control.zoomFitScreen", true },
        { "V A", "common.Control.zoomTool", true },
        { "V E", "common.Control.zoomFitSelection", true },
        { "V Z", "common.Control.undoZoom", true },
        { "V B", "pcbnew.Control.flipBoard", true },
        { "V U", "common.Control.toggleUnits", true },
        { "N S A", "pcbnew.Control.showRatsnest", true },
        { "N H A", "pcbnew.Control.showRatsnest", true },
        { "N S N", "pcbnew.EditorControl.showNet", true },
        { "N H N", "pcbnew.EditorControl.hideNet", true },
        { "D R", "pcbnew.EditorControl.boardSetup", true },
        { "D K", "pcbnew.EditorControl.boardSetup", true },
        { "D C", "pcbnew.EditorControl.boardSetup", true },
        { "O K", "pcbnew.EditorControl.boardSetup", true },
        { "O W", "pcbnew.EditorControl.boardSetup", true },
        { "D I", "common.Control.updatePcbFromSchematic", true },
        { "D U", "common.Control.updateSchematicFromPCB", true },
        { "D N", "pcbnew.Control.showNetInspector", true },
        { "T D", "pcbnew.DRCTool.runDRC", true },
        { "T G A", "pcbnew.ZoneFiller.zoneFillAll", true },
        { "T G R", "pcbnew.ZoneFiller.zoneFill", true },
        { "T G M", "pcbnew.Control.zonesManager", true },
        { "T G H", "pcbnew.ZoneFiller.zoneUnfillAll", true },
        { "T E", "pcbnew.GlobalEdit.editTeardrops", true },
        { "T N", "pcbnew.ReannotateTool.ShowReannotateDialog", true },
        { "T H", "pcbnew.InteractiveDrawing.drawViaStitchArea", true },
        { "T C", "pcbnew.InteractiveSelection.SelectOnSchematic", true },
        { "T P", "common.SuiteControl.openPreferences", true },
        { "O P", "common.SuiteControl.openPreferences", true },
        { "O T", "pcbnew.InteractiveRouter.SettingsDialog", true },
        { "R B", "pcbnew.InspectionTool.ShowBoardStatistics", true },
        { "R M", "common.Interactive.measureTool", true },
        { "R P", "pcbnew.InspectionTool.InspectClearance", true },
        { "C C", "pcbnew.DRCTool.runDRC", true },
        { "C O", "pcbnew.EditorControl.boardSetup", true },
        { "F S", "common.Control.save", true },
        { "F P", "common.Control.print", true },
    };

    return rows;
}
} // namespace


/// The rows the engine can run in the PCB editor: those whose action is not on the list of omissions (the pad is
/// placed in the footprint editor, which has no tree of its own)
std::vector<MENU_TREE_ROW> engineRows()
{
    std::vector<MENU_TREE_ROW> rows;

    for( const MENU_TREE_ROW& row : tableRows() )
    {
        bool isOmitted = std::any_of( PcbAltiumMenuOmissions().begin(), PcbAltiumMenuOmissions().end(),
                                      [&]( const MENU_OMISSION& aOmission )
                                      {
                                          return aOmission.m_actionName
                                                 && std::string( aOmission.m_actionName ) == row.m_actionName;
                                      } );

        if( !isOmitted )
            rows.push_back( row );
    }

    return rows;
}


BOOST_AUTO_TEST_SUITE( PcbAltiumMenuTree )

BOOST_AUTO_TEST_CASE( EveryEntryResolvesToARegisteredAction )
{
    MENU_DEF bar = PcbAltiumMenuBar();

    BOOST_CHECK_GE( bar.Actions().size(), 200u );
    BOOST_CHECK_GT( bar.Actions().size(), bar.m_children.size() );
    MENU_TREE_CHECKS::ExpectNoProblems( MENU_TREE_CHECKS::ActionProblems( bar, 200 ),
                                        "a menu entry does not resolve to a registered action" );
}


BOOST_AUTO_TEST_CASE( MnemonicsAreUniqueInEveryMenu )
{
    MENU_DEF bar = PcbAltiumMenuBar();

    BOOST_CHECK_GE( bar.m_children.size(), 10u );
    MENU_TREE_CHECKS::ExpectNoProblems( MENU_TREE_CHECKS::MnemonicProblems( bar, 10 ),
                                        "a menu has entries that share a mnemonic, or a menu has none" );
}


BOOST_AUTO_TEST_CASE( NothingTheNoteMarksAsExistingDisappears )
{
    MENU_DEF bar = PcbAltiumMenuBar();

    BOOST_CHECK_GE( tableRows().size(), 20u );
    BOOST_CHECK( !PcbAltiumMenuOmissions().empty() );
    MENU_TREE_CHECKS::ExpectNoProblems(
            MENU_TREE_CHECKS::DisappearanceProblems( bar, PcbAltiumMenuOmissions(), tableRows() ),
            "a row of the note's table is lost, or its keys do not select its action" );
}


BOOST_AUTO_TEST_CASE( EverySequenceTheNoteMarksAsExistingRunsItsActionWhenTyped )
{
    MENU_DEF bar = PcbAltiumMenuBar();

    BOOST_CHECK_GE( engineRows().size(), 90u );
    BOOST_CHECK_EQUAL( tableRows().size() - engineRows().size(), 1u ); // the pad
    MENU_TREE_CHECKS::ExpectNoProblems(
            MENU_TREE_CHECKS::SequenceProblems( bar, PcbAltiumSequenceRoots(), engineRows() ),
            "a sequence of the note's table 6.3 does not run the action the note names" );
}


BOOST_AUTO_TEST_CASE( EveryRootOpensAMenuAndIsBoundToItsLetterInTheProfileTable )
{
    MENU_DEF bar = PcbAltiumMenuBar();

    BOOST_CHECK_EQUAL( PcbAltiumSequenceRoots().size(), 16u );
    MENU_TREE_CHECKS::ExpectNoProblems(
            MENU_TREE_CHECKS::RootProblems( bar, PcbAltiumSequenceRoots(),
                                            INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID::ALTIUM ),
                                            tableRows() ),
            "a root is not registered, opens no menu, or is not bound to its letter" );
}


BOOST_AUTO_TEST_CASE( TheNetsRootOpensTheConnectionsMenuAndTheKeyOnlyEntriesAreNotInTheBar )
{
    MENU_DEF bar = PcbAltiumMenuBar();

    // N is the Connections submenu of View, whose Show and Hide are entries for the keys only
    const MENU_DEF* nets = KEY_SEQUENCE::FindRoot( bar, 'N' );

    BOOST_REQUIRE( nets );
    BOOST_CHECK( !nets->m_isKeyOnly );
    BOOST_CHECK( KEY_SEQUENCE::FindChild( *nets, 'S' ) != nullptr );
    BOOST_CHECK( KEY_SEQUENCE::FindChild( *nets, 'S' )->m_isKeyOnly );
    BOOST_CHECK( KEY_SEQUENCE::FindChild( *nets, 'H' )->m_isKeyOnly );

    int keyOnlyEntries = 0;

    bar.Walk(
            [&]( const MENU_DEF& aNode, const std::vector<wxString>& )
            {
                keyOnlyEntries += aNode.m_isKeyOnly ? 1 : 0;
            } );

    BOOST_CHECK_GE( keyOnlyEntries, 12 );
}


BOOST_AUTO_TEST_CASE( SaveAsAndOpenAreSequencesOnlyWhereTheMenuHasThem )
{
    // File > Save As and Open are entries of a stand-alone editor: under the project manager they are not in the menu,
    // and so are not typed (F A and F O). Save and Print are always there.
    MENU_DEF bar = PcbAltiumMenuBar();

    BOOST_CHECK_EQUAL( KEY_SEQUENCE::Resolve( bar, { 'F', 'A' } ) != nullptr, Kiface().IsSingle() );
    BOOST_CHECK_EQUAL( KEY_SEQUENCE::Resolve( bar, { 'F', 'O' } ) != nullptr, Kiface().IsSingle() );
    BOOST_CHECK( KEY_SEQUENCE::Resolve( bar, { 'F', 'S' } ) != nullptr );
    BOOST_CHECK( KEY_SEQUENCE::Resolve( bar, { 'F', 'P' } ) != nullptr );
}


BOOST_AUTO_TEST_CASE( EverySlotIsOneTheFrameFills )
{
    std::vector<std::string> slots = PcbAltiumMenuBar().Slots();

    BOOST_REQUIRE_EQUAL( slots.size(), 5u );

    for( const std::string& slot : slots )
    {
        BOOST_CHECK_MESSAGE( slot == PCB_MENU_SLOT::OPEN_RECENT || slot == PCB_MENU_SLOT::EXIT
                                     || slot == PCB_MENU_SLOT::EXPORT_NETLIST || slot == PCB_MENU_SLOT::PLUGINS
                                     || slot == PCB_MENU_SLOT::LANGUAGES,
                             "the frame does not fill the slot " << slot );
    }
}

BOOST_AUTO_TEST_SUITE_END()

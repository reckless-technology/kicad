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
        { "M D", "pcbnew.InteractiveRouter.Drag45Degree", true },
        { "M C", "pcbnew.InteractiveEdit.FindMove", true },
        { "M B", "pcbnew.InteractiveRouter.BreakTrack", true },
        { "M X", "pcbnew.InteractiveEdit.moveExact", true },
        { "M I", "pcbnew.InteractiveEdit.flip", true },
        { "M O", "pcbnew.InteractiveEdit.rotateCcw", true },
        { "S A", "common.Interactive.selectAll", true },
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
        { "V F", "common.Control.zoomFitObjects", true },
        { "V D", "common.Control.zoomFitScreen", true },
        { "V A", "common.Control.zoomTool", true },
        { "V E", "common.Control.zoomFitSelection", true },
        { "V Z", "common.Control.undoZoom", true },
        { "V B", "pcbnew.Control.flipBoard", true },
        { "V U", "common.Control.toggleUnits", true },
        { "N S A", "pcbnew.Control.showRatsnest", false },
        { "N S N", "pcbnew.EditorControl.showNet", false },
        { "N H N", "pcbnew.EditorControl.hideNet", false },
        { "D R", "pcbnew.EditorControl.boardSetup", true },
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
        { "F A", "common.Control.saveAs", true },
        { "F P", "common.Control.print", true },
        { "F O", "common.Control.open", true },
    };

    return rows;
}
} // namespace


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

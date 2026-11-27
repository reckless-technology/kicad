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

#include "pcb_menus_altium.h"

#include <memory>

#include <advanced_config.h>
#include <bitmaps.h>
#include <file_history.h>
#include <key_sequence.h>
#include <kiface_base.h>
#include <pcb_edit_frame.h>
#include <pgm_base.h>
#include <settings/common_settings.h>
#include <tool/action_menu.h>
#include <tool/actions.h>
#include <tools/pcb_actions.h>
#include <widgets/wx_menubar.h>
#include <wx/translation.h>


// On macOS, wxWidgets moves a Preferences item into the application menu, and the bar has two (Tools and Options). The
// one at the end of Tools is kept out of the menus there, with its separator, so the one moved is the one in Options
// and Tools does not end in a separator. It stays in the tree as a key-only entry, so `T` `P` works on every platform.
static bool isNotMacOS()
{
#ifdef __WXMAC__
    return false;
#else
    return true;
#endif
}


static MENU_DEF keyOnlyOnMacOS( const MENU_DEF& aEntry )
{
#ifdef __WXMAC__
    return aEntry.KeyOnly();
#else
    return aEntry;
#endif
}


MENU_DEF PcbAltiumMenuBar()
{
    using M = MENU_DEF;

    // The entries follow the interaction-profile note, 6.3: the letter of an entry is the letter that follows the
    // menu's own in the note's key sequence, so the key-sequence engine can read this tree.

    M file = M::Submenu(
            _( "&File" ),
            { M::Item( _( "&New..." ), ACTIONS::doNew )
                      .When(
                              []()
                              {
                                  return Kiface().IsSingle();
                              } ),
              M::Item( _( "&Open..." ), ACTIONS::open )
                      .When(
                              []()
                              {
                                  return Kiface().IsSingle();
                              } ),
              M::Slot( _( "Open &Recent" ), PCB_MENU_SLOT::OPEN_RECENT ),
              M::Item( _( "Appen&d Board..." ), PCB_ACTIONS::appendBoard ),
              M::Item( _( "Co&mpare with File..." ), PCB_ACTIONS::compareBoardWithFile ),
              M::Item( _( "Compare with Histor&y..." ), PCB_ACTIONS::compareBoardWithHistory ),
              M::Separator(),
              M::Item( _( "&Save" ), ACTIONS::save ),
              M::Item( _( "Save &As..." ), ACTIONS::saveAs )
                      .When(
                              []()
                              {
                                  return Kiface().IsSingle();
                              } ),
              M::Item( _( "Save &Copy As..." ), ACTIONS::saveCopy )
                      .When(
                              []()
                              {
                                  return !Kiface().IsSingle();
                              } ),
              M::Item( _( "Re&vert" ), ACTIONS::revert ),
              M::Separator(),
              M::Item( _( "Resc&ue Autosave..." ), PCB_ACTIONS::rescueAutosave ),
              M::Separator(),
              M::Submenu( _( "&Import" ),
                          { M::Item( _( "&Netlist..." ), PCB_ACTIONS::importNetlist ),
                            M::Item( _( "&Specctra Session..." ), PCB_ACTIONS::importSpecctraSession ),
                            M::Item( _( "&Graphics..." ), PCB_ACTIONS::placeImportedGraphics ),
                            M::Item( _( "Non-KiCad &Board File..." ), PCB_ACTIONS::openNonKicadBoard ) } ),
              M::Submenu( _( "&Export" ),
                          { M::Item( _( "Specctra &DSN..." ), PCB_ACTIONS::exportSpecctraDSN ),
                            M::Item( _( "For &Older KiCad Version..." ), ACTIONS::exportForOlderVersion ),
                            M::Item( _( "&GenCAD..." ), PCB_ACTIONS::exportGenCAD ),
                            M::Item( _( "&VRML..." ), PCB_ACTIONS::exportVRML ),
                            M::Item( _( "&IDFv3..." ), PCB_ACTIONS::exportIDF ),
                            M::Item( _( "S&TEP/GLB/BREP/XAO/PLY/STL..." ), PCB_ACTIONS::exportSTEP ),
                            M::Item( _( "Footprint &Association (.cmp) File..." ), PCB_ACTIONS::exportCmpFile ),
                            M::Item( _( "&Hyperlynx..." ), PCB_ACTIONS::exportHyperlynx ),
                            M::Slot( _( "Netli&st..." ), PCB_MENU_SLOT::EXPORT_NETLIST ), M::Separator(),
                            M::Item( _( "&Footprints..." ), PCB_ACTIONS::exportFootprints ) } ),
              M::Submenu( _( "&Fabrication Outputs" ),
                          { M::Item( _( "&Gerbers (.gbr)..." ), PCB_ACTIONS::generateGerbers ),
                            M::Item( _( "&Drill Files (.drl)..." ), PCB_ACTIONS::generateDrillFiles ),
                            M::Item( _( "&IPC-2581 File..." ), PCB_ACTIONS::generateIPC2581File ),
                            M::Item( _( "&ODB++ File..." ), PCB_ACTIONS::generateODBPPFile ),
                            M::Item( _( "&Placement Files..." ), PCB_ACTIONS::generatePosFile ),
                            M::Item( _( "&Report File..." ), PCB_ACTIONS::generateReportFile ),
                            M::Item( _( "IPC-D-356 &Netlist File..." ), PCB_ACTIONS::generateD356File ),
                            M::Item( _( "&Bill of Materials..." ), PCB_ACTIONS::generateBOM ),
                            M::Item( _( "&Legacy Bill of Materials..." ), PCB_ACTIONS::generateBOMLegacy ) } ),
              M::Separator(),
              M::Item( _( "&Print..." ), ACTIONS::print ),
              M::Item( _( "P&lot..." ), ACTIONS::plot ),
              M::Separator(),
              M::Slot( _( "Quit" ), PCB_MENU_SLOT::EXIT ) } );

    M edit = M::Submenu(
            _( "&Edit" ),
            { M::Item( _( "&Undo" ), ACTIONS::undo ),
              M::Item( _( "&Redo" ), ACTIONS::redo ),
              M::Separator(),
              M::Item( _( "Cu&t" ), ACTIONS::cut ),
              M::Item( _( "&Copy" ), ACTIONS::copy ),
              M::Item( _( "&Paste" ), ACTIONS::paste ),
              M::Item( _( "Paste Specia&l..." ), ACTIONS::pasteSpecial ),
              M::Item( _( "Delete Select&ion" ), ACTIONS::doDelete ),
              M::Separator(),
              M::Item( _( "&Delete" ), ACTIONS::deleteTool ),
              M::Separator(),
              M::Item( _( "&Find..." ), ACTIONS::find ),
              M::Item( _( "Fi&nd Similar Objects..." ), PCB_ACTIONS::findByProperties ),
              M::Separator(),
              M::Submenu( _( "&Move" ), { M::Item( _( "&Move" ), PCB_ACTIONS::move ),
                                          M::Item( _( "Move &Selection" ), PCB_ACTIONS::move ).KeyOnly(),
                                          M::Item( _( "&Drag" ), PCB_ACTIONS::drag45Degree ),
                                          M::Item( _( "Move &Component..." ), PCB_ACTIONS::getAndPlace ),
                                          M::Item( _( "&Break Track" ), PCB_ACTIONS::breakTrack ),
                                          M::Item( _( "Move Selection by &X, Y..." ), PCB_ACTIONS::moveExact ),
                                          M::Item( _( "Fl&ip Selection" ), PCB_ACTIONS::flip ),
                                          M::Item( _( "R&otate Selection" ), PCB_ACTIONS::rotateCcw ) } ),
              M::Submenu( _( "&Select" ),
                          { M::Item( _( "&All" ), ACTIONS::selectAll ), M::Item( _( "&Net" ), PCB_ACTIONS::selectNet ),
                            M::Item( _( "Connected Co&pper" ), PCB_ACTIONS::selectConnection ),
                            M::Item( _( "&Board" ), ACTIONS::selectAll ).KeyOnly(),
                            M::Item( _( "Physical &Connection" ), PCB_ACTIONS::selectConnection ).KeyOnly(),
                            M::Item( _( "&Inside Area" ), ACTIONS::selectSetRect ),
                            M::Item( _( "Lasso S&elect" ), ACTIONS::selectSetLasso ) } ),
              M::Submenu( _( "DeSelect (&X)" ), { M::Item( _( "&All" ), ACTIONS::unselectAll ) } ),
              M::Submenu( _( "&Align" ),
                          { M::Item( _( "&Left" ), PCB_ACTIONS::alignLeft ),
                            M::Item( _( "&Right" ), PCB_ACTIONS::alignRight ),
                            M::Item( _( "&Top" ), PCB_ACTIONS::alignTop ),
                            M::Item( _( "&Bottom" ), PCB_ACTIONS::alignBottom ),
                            M::Item( _( "Horizontal &Centers" ), PCB_ACTIONS::alignCenterX ),
                            M::Item( _( "&Vertical Centers" ), PCB_ACTIONS::alignCenterY ),
                            M::Item( _( "&Distribute Horizontally" ), PCB_ACTIONS::distributeHorizontallyGaps ),
                            M::Item( _( "Di&stribute Vertically" ), PCB_ACTIONS::distributeVerticallyGaps ) } ),
              M::Submenu( _( "&Jump" ), { M::Item( _( "To &Component..." ), ACTIONS::find ),
                                          M::Item( _( "To &Net..." ), ACTIONS::find ).KeyOnly(),
                                          M::Item( _( "To &Pad..." ), ACTIONS::find ).KeyOnly() } ),
              M::Submenu( _( "&Origin" ), { M::Item( _( "&Set" ), ACTIONS::gridSetOrigin ),
                                            M::Item( _( "&Reset" ), ACTIONS::gridResetOrigin ) } ) } );

    M view = M::Submenu(
            _( "&View" ),
            { M::Item( _( "&Fit Board" ), ACTIONS::zoomFitObjects ),
              M::Item( _( "Fit &Document" ), ACTIONS::zoomFitScreen ), M::Item( _( "Zoom &Area" ), ACTIONS::zoomTool ),
              M::Item( _( "S&elected Objects" ), ACTIONS::zoomFitSelection ),
              M::Item( _( "Zoom &In" ), ACTIONS::zoomInCenter ).ByKey( ACTIONS::zoomIn ),
              M::Item( _( "Zoom &Out" ), ACTIONS::zoomOutCenter ).ByKey( ACTIONS::zoomOut ),
              M::Item( _( "&Zoom Last" ), ACTIONS::zoomUndo ), M::Item( _( "&Redraw" ), ACTIONS::zoomRedraw ),
              M::Separator(), M::CheckItem( _( "Flip &Board" ), PCB_ACTIONS::flipBoard ),
              M::Submenu( _( "&Grids" ), { M::Item( _( "Next &Grid" ), ACTIONS::gridNext ),
                                           M::Item( _( "&Reverse Grid" ), ACTIONS::gridPrev ),
                                           M::Item( _( "Grid &Visible" ), ACTIONS::toggleGrid ),
                                           M::Item( _( "&Set Grid..." ), ACTIONS::gridProperties ) } ),
              M::Item( _( "Toggle &Units" ), ACTIONS::toggleUnits ), M::Separator(),
              M::Submenu( _( "&Panels" ), { M::CheckItem( _( "&Properties" ), ACTIONS::showProperties ),
                                            M::CheckItem( _( "&Search" ), PCB_ACTIONS::showSearch ),
                                            M::CheckItem( _( "&Appearance" ), PCB_ACTIONS::showLayersManager ),
                                            M::CheckItem( _( "&Net Inspector" ), PCB_ACTIONS::showNetInspector ),
                                            M::CheckItem( _( "&Constraints" ), PCB_ACTIONS::showConstraintsPanel ),
                                            M::CheckItem( _( "&Git Commit" ), ACTIONS::showGitCommitPane ),
                                            M::CheckItem( _( "Design &Blocks" ), PCB_ACTIONS::showDesignBlockPanel )
                                                    .When(
                                                            []()
                                                            {
                                                                return ADVANCED_CFG::GetCfg().m_EnablePcbDesignBlocks;
                                                            } ) } ),
              M::Submenu( _( "Co&nnections" ),
                          { M::CheckItem( _( "Show/Hide &All Connections" ), PCB_ACTIONS::showRatsnest ),
                            M::Item( _( "Show &Net Connections" ), PCB_ACTIONS::showNetInRatsnest ),
                            M::Item( _( "Hide Net Connect&ions" ), PCB_ACTIONS::hideNetInRatsnest ),
                            // Altium's own order of the keys is show or hide, then all or a net
                            M::Submenu( _( "&Show" ), { M::Item( _( "&All" ), PCB_ACTIONS::showRatsnest ),
                                                        M::Item( _( "&Net" ), PCB_ACTIONS::showNetInRatsnest ) } )
                                    .KeyOnly(),
                            M::Submenu( _( "&Hide" ), { M::Item( _( "&All" ), PCB_ACTIONS::showRatsnest ),
                                                        M::Item( _( "&Net" ), PCB_ACTIONS::hideNetInRatsnest ) } )
                                    .KeyOnly() } ),
              M::Item( _( "Foo&tprint Browser" ), ACTIONS::showFootprintBrowser ),
              M::Item( _( "3D Vie&wer" ), ACTIONS::show3DViewer ),
              M::Submenu( _( "Drawing &Mode" ),
                          { M::CheckItem( _( "Zones &Filled" ), PCB_ACTIONS::zoneDisplayFilled ),
                            M::CheckItem( _( "Zones &Outline" ), PCB_ACTIONS::zoneDisplayOutline ),
                            M::CheckItem( _( "Zones Fra&ctured" ), PCB_ACTIONS::zoneDisplayFractured )
                                    .When(
                                            []()
                                            {
                                                return ADVANCED_CFG::GetCfg().m_ExtraZoneDisplayModes;
                                            } ),
                            M::CheckItem( _( "Zones &Triangulated" ), PCB_ACTIONS::zoneDisplayTriangulated )
                                    .When(
                                            []()
                                            {
                                                return ADVANCED_CFG::GetCfg().m_ExtraZoneDisplayModes;
                                            } ),
                            M::Separator(), M::CheckItem( _( "&Pads" ), PCB_ACTIONS::padDisplayMode ),
                            M::CheckItem( _( "&Vias" ), PCB_ACTIONS::viaDisplayMode ),
                            M::CheckItem( _( "T&racks" ), PCB_ACTIONS::trackDisplayMode ), M::Separator(),
                            M::CheckItem( _( "&Graphics Outlines" ), PCB_ACTIONS::graphicsOutlines ),
                            M::CheckItem( _( "Te&xt Outlines" ), PCB_ACTIONS::textOutlines ) } ),
              M::Submenu( _( "&Contrast Mode" ),
                          { M::CheckItem( _( "&High Contrast Mode" ), ACTIONS::highContrastMode ),
                            M::Item( _( "&Decrease Layer Opacity" ), PCB_ACTIONS::layerAlphaDec ),
                            M::Item( _( "&Increase Layer Opacity" ), PCB_ACTIONS::layerAlphaInc ) } ) } );

    M project = M::Submenu( _( "Proje&ct" ), { M::Item( _( "&Compile (Run DRC)..." ), PCB_ACTIONS::runDRC ),
                                               M::Item( _( "Project &Options..." ), PCB_ACTIONS::boardSetup ),
                                               M::Separator(), M::Item( _( "&Next Marker" ), ACTIONS::nextMarker ),
                                               M::Item( _( "&Previous Marker" ), ACTIONS::prevMarker ),
                                               M::Item( _( "&Exclude Marker" ), ACTIONS::excludeMarker ) } );

    // The constraints submenu is the list the context menu uses, so the two cannot drift
    M constraints = M::Submenu( _( "Co&nstraints" ), {} );

    for( const TOOL_ACTION* action : PCB_ACTIONS::ConstraintAddActions() )
        constraints.m_children.push_back( M::Item( action->GetFriendlyName(), *action ) );

    constraints.m_children.push_back( M::Separator() );
    constraints.m_children.push_back( M::Item( _( "&Remove Constraints" ), PCB_ACTIONS::removeConstraints ) );

    M place = M::Submenu(
            _( "&Place" ),
            { M::Item( _( "&Track (Interactive Routing)" ), PCB_ACTIONS::routeSingleTrack ),
              M::Item( _( "&Via" ), PCB_ACTIONS::drawVia ),
              M::Item( _( "&Component..." ), PCB_ACTIONS::placeFootprint ),
              M::Item( _( "Poly&gon Pour" ), PCB_ACTIONS::drawZone ),
              M::Item( _( "Rule Area (&Keepout)" ), PCB_ACTIONS::drawRuleArea ),
              M::Separator(),
              M::Item( _( "&Line" ), PCB_ACTIONS::drawLine ),
              M::Item( _( "Text &String" ), PCB_ACTIONS::placeText ),
              M::Item( _( "&Fill (Rectangle)" ), PCB_ACTIONS::drawRectangle ),
              M::Item( _( "R&ectangle" ), PCB_ACTIONS::drawRectangle ).KeyOnly(),
              M::Item( _( "Solid &Region (Polygon)" ), PCB_ACTIONS::drawPolygon ),
              M::Submenu( _( "&Arc" ), { M::Item( _( "&Arc by Center" ), PCB_ACTIONS::drawArcCenter ),
                                         M::Item( _( "Arc by &Edge" ), PCB_ACTIONS::drawArc ),
                                         M::Item( _( "F&ull Circle" ), PCB_ACTIONS::drawCircle ) } ),
              M::Submenu( _( "&Dimension" ), { M::Item( _( "&Linear" ), PCB_ACTIONS::drawAlignedDimension ),
                                               M::Item( _( "&Radial" ), PCB_ACTIONS::drawRadialDimension ),
                                               M::Item( _( "L&eader" ), PCB_ACTIONS::drawLeader ),
                                               M::Item( _( "&Center" ), PCB_ACTIONS::drawCenterDimension ),
                                               M::Item( _( "&Orthogonal" ), PCB_ACTIONS::drawOrthogonalDimension ) } ),
              M::Separator(),
              M::Item( _( "V&ia Stack" ), PCB_ACTIONS::placeViaStack ),
              M::Item( _( "&Make Via Stack" ), PCB_ACTIONS::makeViaStack ),
              M::Item( _( "Copper Thieving &Zone" ), PCB_ACTIONS::drawCopperThievingZone ),
              M::Submenu( _( "&Board Graphics" ),
                          { M::Item( _( "&Ellipse" ), PCB_ACTIONS::drawEllipse ),
                            M::Item( _( "Ellipse &Arc" ), PCB_ACTIONS::drawEllipseArc ),
                            M::Item( _( "&Bezier" ), PCB_ACTIONS::drawBezier ),
                            M::Item( _( "Reference &Image..." ), PCB_ACTIONS::placeReferenceImage ),
                            M::Item( _( "Te&xt Box" ), PCB_ACTIONS::drawTextBox ),
                            M::Item( _( "&Table" ), PCB_ACTIONS::drawTable ),
                            M::Item( _( "&Point" ), PCB_ACTIONS::placePoint ),
                            M::Item( _( "Bar&code" ), PCB_ACTIONS::placeBarcode ),
                            M::Item( _( "&Subgrid" ), PCB_ACTIONS::placeSubGrid ) } ),
              M::Submenu( _( "Micro&wave Shapes" ),
                          { M::Item( _( "&Line" ), PCB_ACTIONS::microwaveCreateLine ),
                            M::Item( _( "&Gap" ), PCB_ACTIONS::microwaveCreateGap ),
                            M::Item( _( "&Stub" ), PCB_ACTIONS::microwaveCreateStub ),
                            M::Item( _( "S&tub Arc" ), PCB_ACTIONS::microwaveCreateStubArc ),
                            M::Item( _( "&Polygonal Shape" ), PCB_ACTIONS::microwaveCreateFunctionShape ) } ),
              constraints,
              M::Submenu( _( "Doc&umentation" ),
                          { M::Item( _( "&Characteristics Table" ), PCB_ACTIONS::placeCharacteristics ),
                            M::Item( _( "&Stackup Table" ), PCB_ACTIONS::placeStackup ),
                            M::Item( _( "Drill Cha&rt" ), PCB_ACTIONS::placeDrillChart ),
                            M::Item( _( "Drill &Map" ), PCB_ACTIONS::placeDrillMap ) } ),
              M::Submenu( _( "Drill &Origin" ), { M::Item( _( "&Set" ), PCB_ACTIONS::drillOrigin ),
                                                  M::Item( _( "&Reset" ), PCB_ACTIONS::drillResetOrigin ) } ),
              M::Submenu( _( "Auto-&Place Footprints" ),
                          { M::Item( _( "&Off-Board Footprints" ), PCB_ACTIONS::autoplaceOffboardComponents ),
                            M::Item( _( "&Selected Footprints" ), PCB_ACTIONS::autoplaceSelectedComponents ) } ) } );

    M design = M::Submenu( _( "&Design" ),
                           { M::Item( _( "&Rules (Board Setup)..." ), PCB_ACTIONS::boardSetup ),
                             M::Item( _( "Layer Stac&k Manager..." ), PCB_ACTIONS::boardSetup ).KeyOnly(),
                             M::Item( _( "&Classes..." ), PCB_ACTIONS::boardSetup ).KeyOnly(),
                             M::Item( _( "Custom Rule &Editor..." ), PCB_ACTIONS::drcRuleEditor ), M::Separator(),
                             M::Item( _( "&Import Changes From Schematic..." ), ACTIONS::updatePcbFromSchematic ),
                             M::Item( _( "&Update Schematic from PCB..." ), ACTIONS::updateSchematicFromPcb ),
                             M::Separator(), M::Item( _( "&Net Inspector" ), PCB_ACTIONS::showNetInspector ) } );

    M tools = M::Submenu(
            _( "&Tools" ),
            { M::Item( _( "&Design Rule Check..." ), PCB_ACTIONS::runDRC ),
              M::Submenu( _( "Poly&gon Pours" ),
                          { M::Item( _( "Repour &All" ), PCB_ACTIONS::zoneFillAll ),
                            M::Item( _( "&Repour Selected" ), PCB_ACTIONS::zoneFill ),
                            M::Item( _( "Polygon &Manager..." ), PCB_ACTIONS::zonesManager ),
                            M::Item( _( "S&helve All Polygons" ), PCB_ACTIONS::zoneUnfillAll ) } ),
              M::Item( _( "T&eardrops..." ), PCB_ACTIONS::editTeardrops ),
              M::Item( _( "Re-A&nnotate..." ), PCB_ACTIONS::boardReannotate ),
              M::Item( _( "Via Stitc&hing" ), PCB_ACTIONS::drawViaStitchArea ),
              M::Item( _( "&Cross Probe to Schematic" ), PCB_ACTIONS::selectOnSchematic ),
              M::Separator(),
              M::Submenu( _( "Edit &Objects" ),
                          { M::Item( _( "Extend &Graphic" ), PCB_ACTIONS::extendGraphic ),
                            M::Item( _( "&Trim Graphic" ), PCB_ACTIONS::trimGraphic ),
                            M::Item( _( "Tracks and &Vias..." ), PCB_ACTIONS::editTracksAndVias ),
                            M::Item( _( "Te&xt and Graphics..." ), PCB_ACTIONS::editTextAndGraphics ),
                            M::Item( _( "&Mirror" ), PCB_ACTIONS::mirrorInteractive ),
                            M::Item( _( "Change &Footprints..." ), PCB_ACTIONS::changeFootprints ),
                            M::Item( _( "S&wap Layers..." ), PCB_ACTIONS::swapLayers ),
                            M::Item( _( "&Regenerate Tuning Patterns" ), PCB_ACTIONS::regenerateAllTuning ),
                            M::Item( _( "Footprint Fi&elds..." ), PCB_ACTIONS::editFootprintFields ) } ),
              M::Submenu( _( "Cle&anup" ), { M::Item( _( "&Tracks and Vias..." ), PCB_ACTIONS::cleanupTracksAndVias ),
                                             M::Item( _( "&Unused Pads..." ), PCB_ACTIONS::removeUnusedPads ),
                                             M::Item( _( "&Graphics..." ), PCB_ACTIONS::cleanupGraphics ),
                                             M::Item( _( "&Repair Board" ), PCB_ACTIONS::repairBoard ) } ),
              M::Separator(),
              M::Item( _( "&Footprint Editor" ), ACTIONS::showFootprintEditor ),
              M::Item( _( "Update Footprints from Librar&y..." ), PCB_ACTIONS::updateFootprints ),
              M::Item( _( "&Migrate 3D Models..." ), PCB_ACTIONS::migrate3DModels ),
              M::Item( _( "Collect 3D Model&s..." ), PCB_ACTIONS::collect3DModels ),
              M::Item( _( "Dr&ill Groups..." ), PCB_ACTIONS::showDrillGroups ),
              M::Separator(),
              M::Submenu( _( "Gene&rators" ),
                          { M::Item( _( "&Manager" ), PCB_ACTIONS::generatorsShowManager ),
                            M::Item( _( "Regenerate &All" ), PCB_ACTIONS::regenerateAll ),
                            M::Item( _( "Regenerate &Selected" ), PCB_ACTIONS::regenerateSelected ) } )
                      .When(
                              []()
                              {
                                  return ADVANCED_CFG::GetCfg().m_EnableGenerators;
                              } ),
              M::Submenu(
                      _( "Mu&lti-Channel" ),
                      { M::Item( _( "&Generate Placement Rule Areas..." ), PCB_ACTIONS::generatePlacementRuleAreas ),
                        M::Item( _( "Expand to &Matching Topology" ), PCB_ACTIONS::expandToMatchingTopology ),
                        M::Item( _( "Expand to &Largest Topology" ), PCB_ACTIONS::expandToLargestTopology ),
                        M::Item( _( "&Repeat Layout..." ), PCB_ACTIONS::repeatLayout ),
                        M::Item( _( "&Update Linked Layouts..." ), PCB_ACTIONS::updateLinkedLayouts ),
                        M::Item( _( "U&nlink Layout" ), PCB_ACTIONS::unlinkLayout ) } ),
              M::Separator(),
              M::Item( _( "Schema&tic Editor" ), PCB_ACTIONS::showEeschema ),
              M::Item( _( "Pro&ject Manager" ), ACTIONS::showProjectManager )
                      .When(
                              []()
                              {
                                  return !Kiface().IsSingle();
                              } ),
              M::Item( _( "Calc&ulator Tools" ), ACTIONS::showCalculatorTools ),
              M::Slot( _( "External Plugins" ), PCB_MENU_SLOT::PLUGINS ),
              M::Separator().When( isNotMacOS ),
              keyOnlyOnMacOS( M::Item( _( "&Preferences..." ), ACTIONS::openPreferences ) ) } );

    M route = M::Submenu( _( "Ro&ute" ),
                          { M::Item( _( "Interactive Rou&ting" ), PCB_ACTIONS::routeSingleTrack ),
                            M::Item( _( "Interactive D&ifferential Pair Routing" ), PCB_ACTIONS::routeDiffPair ),
                            M::Item( _( "Length Tuning (Single T&rack)" ), PCB_ACTIONS::tuneSingleTrack ),
                            M::Item( _( "Length Tuning (Differential &Pair)" ), PCB_ACTIONS::tuneDiffPair ),
                            M::Item( _( "S&kew Tuning (Differential Pair)" ), PCB_ACTIONS::tuneSkew ), M::Separator(),
                            M::Item( _( "G&loss Selected" ), PCB_ACTIONS::routerOptimizeSelected ),
                            M::Item( _( "Spread Tra&cks" ), PCB_ACTIONS::routerSpreadTracks ),
                            M::Submenu( _( "&Un-Route" ), { M::Item( _( "&All..." ), PCB_ACTIONS::globalDeletions ) } ),
                            M::Separator(), M::Item( _( "Select Lay&er Pair..." ), PCB_ACTIONS::selectLayerPair ),
                            M::Item( _( "Routing Setti&ngs..." ), PCB_ACTIONS::routerSettingsDialog ) } );

    M reports = M::Submenu( _( "&Reports" ),
                            { M::Item( _( "&Board Information" ), PCB_ACTIONS::boardStatistics ),
                              M::Item( _( "&Measure Distance" ), ACTIONS::measureTool ),
                              M::Item( _( "Measure &Primitives" ), PCB_ACTIONS::inspectClearance ),
                              M::Item( _( "Inspect C&onstraints..." ), PCB_ACTIONS::inspectConstraints ),
                              M::Item( _( "Footprint &Associations" ), PCB_ACTIONS::showFootprintAssociations ),
                              M::Item( _( "Footprint &Diff..." ), PCB_ACTIONS::diffFootprint ) } );

    M options = M::Submenu( _( "&Options" ),
                            { M::Item( _( "Page &Settings..." ), ACTIONS::pageSettings ),
                              M::Item( _( "&Preferences..." ), ACTIONS::openPreferences ),
                              M::Item( _( "&Grids..." ), ACTIONS::gridProperties ),
                              M::Item( _( "Rou&ting Options..." ), PCB_ACTIONS::routerSettingsDialog ),
                              M::Item( _( "Layer Stac&k Manager..." ), PCB_ACTIONS::boardSetup ).KeyOnly(),
                              M::Item( _( "Favorite Routing &Widths..." ), PCB_ACTIONS::boardSetup ).KeyOnly(),
                              M::Separator(), M::Item( _( "Conf&igure Paths..." ), ACTIONS::configurePaths ),
                              M::Item( _( "&Footprint Libraries..." ), ACTIONS::showFootprintLibTable ),
                              M::Item( _( "Design &Block Libraries..." ), ACTIONS::showDesignBlockLibTable )
                                      .When(
                                              []()
                                              {
                                                  return ADVANCED_CFG::GetCfg().m_EnablePcbDesignBlocks;
                                              } ),
                              M::Slot( _( "Language" ), PCB_MENU_SLOT::LANGUAGES ) } );

    return M::Submenu( wxEmptyString, { file, edit, view, project, place, design, tools, route, reports, options } );
}


const std::vector<MENU_OMISSION>& PcbAltiumMenuOmissions()
{
    static const std::vector<MENU_OMISSION> omissions = {
        { "P P", "pcbnew.PadTool.placePad", "Pad: placed in the footprint editor only" },
        { "P B, P O, T B", nullptr, "3D bodies: new (note, 5.10)" },
        { "U U N, U U C, U U O", nullptr, "Un-Route net, connection, component: new composite actions" },
        { "J A, J O", nullptr, "Jump to origin: new, small" },
        { "G", nullptr, "Snap grid popup: new, small (5.7)" },
        { "O D, Ctrl+D", nullptr, "View Options: new, small" },
        { "D T", nullptr, "Manage Layer Sets: the Appearance panel's presets, which have no action" },
    };

    return omissions;
}


WX_MENUBAR* PCB_EDIT_FRAME::createInteractionProfileMenuBar( TOOL_INTERACTIVE* aTool )
{
    // Open Recent is made once and cloned into each new bar, as the stock bar does
    static ACTION_MENU* openRecentMenu = nullptr;

    MENU_SLOT_FILLER fillSlot = [&]( const std::string& aSlot, const MENU_DEF& aNode, ACTION_MENU& aParent )
    {
        if( aSlot == PCB_MENU_SLOT::OPEN_RECENT )
        {
            if( !Kiface().IsSingle() )
                return;

            FILE_HISTORY& fileHistory = GetFileHistory();

            if( !openRecentMenu )
            {
                openRecentMenu = new ACTION_MENU( false, aTool );
                openRecentMenu->SetIcon( BITMAPS::recent );

                fileHistory.UseMenu( openRecentMenu );
                fileHistory.AddFilesToMenu( openRecentMenu );
            }

            openRecentMenu->SetTitle( aNode.m_label );
            fileHistory.UpdateClearText( openRecentMenu, _( "Clear Recent Files" ) );

            wxMenuItem*       item = aParent.Add( openRecentMenu->Clone() );
            ACTION_CONDITIONS cond;
            cond.Enable( FILE_HISTORY::FileHistoryNotEmpty( fileHistory ) );
            RegisterUIUpdateHandler( item->GetId(), cond );
        }
        else if( aSlot == PCB_MENU_SLOT::EXPORT_NETLIST )
        {
            if( ADVANCED_CFG::GetCfg().m_ShowPcbnewExportNetlist && m_exportNetlistAction )
                aParent.Add( *m_exportNetlistAction, false, aNode.m_label );
        }
        else if( aSlot == PCB_MENU_SLOT::EXIT )
        {
            aParent.AddQuitOrClose( &Kiface(), _( "PCB Editor" ) );
        }
        else if( aSlot == PCB_MENU_SLOT::PLUGINS )
        {
            ACTION_MENU* plugins = new ACTION_MENU( false, aTool );
            plugins->SetTitle( aNode.m_label );
            plugins->SetIcon( BITMAPS::puzzle_piece );

            if( Pgm().GetCommonSettings()->m_Api.enable_server && AddApiPluginMenuItems( plugins ) > 0 )
                plugins->AppendSeparator();

            plugins->Add( ACTIONS::pluginsReload );
            plugins->Add( ACTIONS::pluginsShowFolder );
            aParent.Add( plugins );
        }
        else if( aSlot == PCB_MENU_SLOT::LANGUAGES )
        {
            AddMenuLanguageList( &aParent, aTool );
        }
    };

    WX_MENUBAR* menuBar = BuildMenuBar( PcbAltiumMenuBar(), aTool, fillSlot );

    // Entries that exist under a project manager only, or that depend on a setting read when the bar is shown
    if( wxMenuItem* item = menuBar->FindItem( ACTIONS::updatePcbFromSchematic.GetUIId() ) )
        item->Enable( !Kiface().IsSingle() );

    if( wxMenuItem* item = menuBar->FindItem( ACTIONS::updateSchematicFromPcb.GetUIId() ) )
        item->Enable( !Kiface().IsSingle() );

    if( wxMenuItem* item = menuBar->FindItem( PCB_ACTIONS::compareBoardWithHistory.GetUIId() ) )
    {
        ACTION_CONDITIONS historyCond;
        historyCond.Enable(
                []( const SELECTION& )
                {
                    COMMON_SETTINGS* cs = Pgm().GetCommonSettings();
                    return cs && cs->AutosaveUsesLocalHistory();
                } );
        RegisterUIUpdateHandler( item->GetId(), historyCond );
    }

    AddStandardHelpMenu( menuBar );

    return menuBar;
}


const std::vector<std::pair<const TOOL_ACTION*, wxUniChar>>& PcbAltiumSequenceRoots()
{
    static const std::vector<std::pair<const TOOL_ACTION*, wxUniChar>> roots = {
        { &ACTIONS::sequencePlace, 'P' },   { &ACTIONS::sequenceEdit, 'E' },     { &ACTIONS::sequenceMove, 'M' },
        { &ACTIONS::sequenceSelect, 'S' },  { &ACTIONS::sequenceDeselect, 'X' }, { &ACTIONS::sequenceAlign, 'A' },
        { &ACTIONS::sequenceJump, 'J' },    { &ACTIONS::sequenceView, 'V' },     { &ACTIONS::sequenceProject, 'C' },
        { &ACTIONS::sequenceDesign, 'D' },  { &ACTIONS::sequenceTools, 'T' },    { &ACTIONS::sequenceReports, 'R' },
        { &ACTIONS::sequenceOptions, 'O' }, { &ACTIONS::sequenceFile, 'F' },     { &PCB_ACTIONS::sequenceRoute, 'U' },
        { &PCB_ACTIONS::sequenceNets, 'N' }
    };

    return roots;
}


bool PCB_EDIT_FRAME::GetKeySequenceSpec( KEY_SEQUENCE_SPEC& aSpec ) const
{
    aSpec.m_tree = std::make_shared<const MENU_DEF>( PcbAltiumMenuBar() );
    aSpec.m_roots = PcbAltiumSequenceRoots();
    return true;
}

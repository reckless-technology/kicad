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

#include "pcb_context_menu_altium.h"

#include "pcb_menus_altium.h"

#include <board_connected_item.h>
#include <pcb_edit_frame.h>
#include <tool/actions.h>
#include <tool/selection_conditions.h>
#include <tools/pcb_actions.h>
#include <tools/pcb_selection_conditions.h>
#include <tools/pcb_selection_tool.h>
#include <wx/translation.h>

namespace
{
using M = MENU_DEF;
using S = SELECTION_CONDITIONS;
using P = PCB_SELECTION_CONDITIONS;
using A = PCB_ACTIONS;

const std::vector<KICAD_T> footprintTypes = { PCB_FOOTPRINT_T };
const std::vector<KICAD_T> padTypes = { PCB_PAD_T };
const std::vector<KICAD_T> trackTypes = { PCB_TRACE_T, PCB_ARC_T, PCB_VIA_T };
const std::vector<KICAD_T> zoneTypes = { PCB_ZONE_T };
const std::vector<KICAD_T> markerTypes = { PCB_MARKER_T };
const std::vector<KICAD_T> crossProbingTypes = { PCB_FOOTPRINT_T, PCB_PAD_T };
const std::vector<KICAD_T> graphicTypes = { PCB_SHAPE_T, PCB_TEXT_T, PCB_TEXTBOX_T };


/// A slot that shows the submenu KiCad's tools build at run time under \a aTitle, while \a aCondition holds
M slotOf( const char* aTitle, const SELECTION_CONDITION& aCondition )
{
    M slot = M::Slot( wxGetTranslation( wxString::FromUTF8( aTitle ) ), aTitle );
    slot.m_show = aCondition;
    return slot;
}
} // namespace


MENU_DEF PcbAltiumContextMenu( PCB_EDIT_FRAME* aFrame, PCB_SELECTION_TOOL* aSelectionTool )
{
    const SELECTION_CONDITION toolActive = [aFrame]( const SELECTION& )
    {
        return aFrame && !aFrame->ToolStackIsEmpty();
    };

    const SELECTION_CONDITION toolInactive = [aFrame]( const SELECTION& )
    {
        return aFrame && aFrame->ToolStackIsEmpty();
    };

    const SELECTION_CONDITION inGroup = [aSelectionTool]( const SELECTION& )
    {
        return aSelectionTool && aSelectionTool->GetEnteredGroup() != nullptr;
    };

    const SELECTION_CONDITION anything = S::NotEmpty;
    const SELECTION_CONDITION nothing = S::Empty;
    const SELECTION_CONDITION footprint = S::HasTypes( footprintTypes );
    const SELECTION_CONDITION pad = S::HasTypes( padTypes );
    const SELECTION_CONDITION track = S::HasTypes( trackTypes );
    const SELECTION_CONDITION zone = S::HasTypes( zoneTypes );
    const SELECTION_CONDITION onlyZones = S::MoreThan( 0 ) && S::OnlyTypes( zoneTypes );
    const SELECTION_CONDITION onlyTracks = S::MoreThan( 0 ) && S::OnlyTypes( trackTypes );

    // Only an item on a real net: a pad with no net, such as an unplated mounting hole, has nothing to act on
    const SELECTION_CONDITION onNet = []( const SELECTION& aSelection )
    {
        for( const EDA_ITEM* item : aSelection )
        {
            if( const BOARD_CONNECTED_ITEM* connected = dynamic_cast<const BOARD_CONNECTED_ITEM*>( item ) )
            {
                if( connected->GetNetCode() > 0 )
                    return true;
            }
        }

        return false;
    };
    const SELECTION_CONDITION tableCells = S::MoreThan( 0 ) && S::OnlyTypes( { PCB_TABLECELL_T } );
    const SELECTION_CONDITION group = S::Count( 1 ) && S::HasType( PCB_GROUP_T );
    const SELECTION_CONDITION image = S::Count( 1 ) && S::HasType( PCB_REFERENCE_IMAGE_T );
    const SELECTION_CONDITION graphics = S::HasTypes( graphicTypes );
    const SELECTION_CONDITION twoItems = S::MoreThan( 1 );

    M place = PcbAltiumPlaceMenu();
    place.m_label = _( "Place" );

    // Align works on two items or more, so the entries of the shared tree wait for them while the submenu keeps its place
    M align = PcbAltiumAlignMenu();
    align.m_label = _( "Align" );
    align.m_show = anything;

    for( M& entry : align.m_children )
    {
        if( entry.IsAction() )
            entry.m_show = twoItems;
    }

    // The two distributions that KiCad's own menu has and the shared tree does not
    align.m_children.push_back(
            M::Item( _( "Distribute Horizontal Centers" ), A::distributeHorizontallyCenters, twoItems ) );
    align.m_children.push_back(
            M::Item( _( "Distribute Vertical Centers" ), A::distributeVerticallyCenters, twoItems ) );

    M slotChains = slotOf( PCB_CONTEXT_SLOT::NET_CHAINS, onNet );

    return M::Submenu(
            wxEmptyString,
            { M::Item( _( "Cancel" ), ACTIONS::cancelInteractive, toolActive ),
              M::Item( _( "Find Similar Objects..." ), A::findByProperties ),
              M::Item( _( "Clear Filter" ), A::clearHighlight ),
              M::Separator(),
              M::Item( _( "Interactive Routing" ), A::routeSingleTrack, nothing ),
              M::Item( _( "Interactive Length Tuning" ), A::tuneSingleTrack, nothing ),
              M::Item( _( "Get and Place Footprint" ), A::getAndPlace, nothing && toolInactive ),
              place,
              M::Item( _( "Toggle Primitive Lock" ), A::toggleLock, anything ),
              M::Submenu( _( "Unions" ), anything,
                          { M::Item( _( "Group" ), ACTIONS::group ), M::Item( _( "Ungroup" ), ACTIONS::ungroup ),
                            M::Item( _( "Add to Group" ), ACTIONS::addToGroup ),
                            M::Item( _( "Remove from Group" ), ACTIONS::removeFromGroup ),
                            M::Item( _( "Enter Group" ), ACTIONS::groupEnter, group ),
                            M::Item( _( "Leave Group" ), ACTIONS::groupLeave, inGroup ),
                            M::Item( _( "Apply Design Block Layout" ), A::applyDesignBlockLayout, group ),
                            M::Item( _( "Place Linked Design Block" ), A::placeLinkedDesignBlock, group ),
                            M::Item( _( "Save to Linked Design Block" ), A::saveToLinkedDesignBlock, group ) } ),
              M::Submenu( _( "Component Actions" ), anything,
                          { M::Item( _( "Move Component" ), A::move, footprint ),
                            M::Item( _( "Select Component Connections" ), A::selectConnection, footprint ),
                            M::Item( _( "Show Nets" ), A::showNetInRatsnest, footprint ),
                            M::Item( _( "Hide Nets" ), A::hideNetInRatsnest, footprint ),
                            M::Item( _( "Update Footprint from Library" ), A::updateFootprint, footprint ),
                            M::Item( _( "Update Footprints from Library" ), A::updateFootprints, footprint ),
                            M::Item( _( "Change Footprint" ), A::changeFootprint, footprint ),
                            M::Item( _( "Change Footprints" ), A::changeFootprints, footprint ),
                            M::Item( _( "Open in Footprint Editor" ), A::editFpInFpEditor, footprint ),
                            M::Item( _( "Swap Components" ), A::swap, twoItems ),
                            M::Item( _( "Swap Gate Nets" ), A::swapGateNets, footprint ),
                            M::Item( _( "Pack and Move Footprints" ), A::packAndMoveFootprints, footprint ),
                            M::Item( _( "Flip" ), A::flip, footprint ),
                            M::Item( _( "Rotate" ), A::rotateCcw, footprint ),
                            M::Item( _( "Exclude from BOM" ), A::toggleExcludeFromBOM, footprint ),
                            M::Item( _( "Exclude from Simulation" ), A::toggleExcludeFromSim, footprint ),
                            M::Item( _( "Exclude from Position Files" ), A::toggleExcludeFromPosFiles, footprint ),
                            M::Submenu( _( "Multichannel" ), footprint,
                                        { M::Item( _( "Expand to Matching Topology" ), A::expandToMatchingTopology ),
                                          M::Item( _( "Expand to Largest Topology" ), A::expandToLargestTopology ),
                                          M::Item( _( "Repeat Layout" ), A::repeatLayout ),
                                          M::Item( _( "Update Linked Layouts" ), A::updateLinkedLayouts ),
                                          M::Item( _( "Unlink Layout" ), A::unlinkLayout ) } ) } ),
              M::Submenu( _( "Polygon Actions" ), anything,
                          { M::Item( _( "Repour Selected" ), A::zoneFill, onlyZones ),
                            M::Item( _( "Repour All" ), A::zoneFillAll ),
                            M::Item( _( "Shelve Selected" ), A::zoneUnfill, onlyZones ),
                            M::Item( _( "Shelve All" ), A::zoneUnfillAll ),
                            M::Item( _( "Bring to Front" ), A::zonePriorityMoveToTop, onlyZones ),
                            M::Item( _( "Raise" ), A::zonePriorityRaise, onlyZones ),
                            M::Item( _( "Lower" ), A::zonePriorityLower, onlyZones ),
                            M::Item( _( "Send to Back" ), A::zonePriorityMoveToBottom, onlyZones ),
                            M::Item( _( "Polygon Manager" ), A::zonesManager ),
                            M::Item( _( "Add Cutout" ), A::drawZoneCutout, onlyZones ),
                            M::Item( _( "Add Similar Polygon" ), A::drawSimilarZone, onlyZones ),
                            M::Item( _( "Duplicate onto Layer" ), A::zoneDuplicate, onlyZones ),
                            M::Item( _( "Combine Selected Polygons" ), A::zoneMerge, onlyZones ) } ),
              M::Submenu( _( "Net Actions" ), onNet,
                          { M::Item( _( "Show Nets" ), A::showNetInRatsnest ),
                            M::Item( _( "Hide Nets" ), A::hideNetInRatsnest ),
                            M::Item( _( "Highlight Net" ), A::highlightNetSelection ),
                            M::Item( _( "Select Net" ), A::selectNet ), M::Item( _( "Unselect Net" ), A::deselectNet ),
                            M::Item( _( "Assign Net Class" ), A::assignNetClass ),
                            M::Item( _( "Net Inspector" ), A::showNetInspector ),
                            M::Item( _( "Clear Stitching Via Exclusions" ), A::clearStitchViaExclusions, track ),
                            slotChains } ),
              M::Submenu( _( "Pad Actions" ), pad,
                          { M::Item( _( "Copy Pad Properties" ), A::copyPadSettings ),
                            M::Item( _( "Apply Pad Properties" ), A::applyPadSettings ),
                            M::Item( _( "Push Pad Properties" ), A::pushPadSettings ),
                            M::Item( _( "Swap Pad Nets" ), A::swapPadNets ), M::Item( _( "Pad Table" ), A::padTable ),
                            M::Item( _( "Enumerate Pads" ), A::enumeratePads ),
                            M::Item( _( "Recombine Pad" ), A::recombinePad ),
                            M::Item( _( "Explode Pad" ), A::explodePad ) } ),
              M::Submenu( _( "Track Actions" ), track,
                          { M::Item( _( "Drag (45 Degree)" ), A::drag45Degree ),
                            M::Item( _( "Drag (Free Angle)" ), A::dragFreeAngle ),
                            M::Item( _( "Break Track" ), A::breakTrack ),
                            M::Item( _( "Change Track Width" ), A::changeTrackWidth ),
                            M::Item( _( "Fillet Tracks" ), A::filletTracks ),
                            M::Item( _( "Select Connection" ), A::selectConnection ),
                            M::Item( _( "Unroute Selected" ), A::unrouteSelected ),
                            M::Item( _( "Unroute Segment" ), A::unrouteSegment ),
                            M::Item( _( "Route Selected" ), A::routerRouteSelected ),
                            M::Item( _( "Route Selected from Other End" ), A::routerRouteSelectedFromEnd ),
                            M::Item( _( "Autoroute Selected" ), A::routerAutorouteSelected ),
                            M::Item( _( "Optimize Selected" ), A::routerOptimizeSelected ),
                            M::Item( _( "Regenerate Tuning" ), A::regenerateAllTuning ) } ),
              M::Submenu( _( "Locking" ), anything,
                          { M::Item( _( "Lock" ), A::lock, P::HasUnlockedItems ),
                            M::Item( _( "Unlock" ), A::unlock, P::HasLockedItems ),
                            M::Item( _( "Toggle Lock" ), A::toggleLock ) } ),
              M::Submenu( _( "Convert" ), anything,
                          { M::Item( _( "Convert to Polygon" ), A::convertToPoly ),
                            M::Item( _( "Convert to Zone" ), A::convertToZone ),
                            M::Item( _( "Convert to Keepout" ), A::convertToKeepout ),
                            M::Item( _( "Convert to Lines" ), A::convertToLines ),
                            M::Item( _( "Convert to Tracks" ), A::convertToTracks ),
                            M::Item( _( "Convert to Arc" ), A::convertToArc ),
                            M::Item( _( "Outset Items" ), A::outsetItems ),
                            M::Item( _( "Convert Image to Outlines" ), A::convertImageToOutlines, image ) } ),
              M::Submenu( _( "Transform" ), anything,
                          { M::Item( _( "Rotate Counterclockwise" ), A::rotateCcw ),
                            M::Item( _( "Rotate Clockwise" ), A::rotateCw ), M::Item( _( "Flip" ), A::flip ),
                            M::Item( _( "Mirror Horizontally" ), A::mirrorH ),
                            M::Item( _( "Mirror Vertically" ), A::mirrorV ),
                            M::Item( _( "Mirror Interactively" ), A::mirrorInteractive ),
                            M::Item( _( "Move Exactly" ), A::moveExact ),
                            M::Item( _( "Move with Reference" ), A::moveWithReference ),
                            M::Item( _( "Move Individually" ), A::moveIndividually ),
                            M::Item( _( "Position Relative" ), A::positionRelative ),
                            M::Item( _( "Offset Tool" ), A::interactiveOffsetTool ),
                            M::Item( _( "Create Array" ), A::createArray ),
                            M::Item( _( "Match Properties" ), A::matchProperties ) } ),
              M::Submenu( _( "Graphics Actions" ), graphics,
                          { M::Item( _( "Heal Shapes" ), A::healShapes ),
                            M::Item( _( "Simplify Polygons" ), A::simplifyPolygons ),
                            M::Item( _( "Fillet Lines" ), A::filletLines ),
                            M::Item( _( "Chamfer Lines" ), A::chamferLines ),
                            M::Item( _( "Dogbone Corners" ), A::dogboneCorners ),
                            M::Item( _( "Extend Lines" ), A::extendLines ),
                            M::Item( _( "Extend Graphic" ), A::extendGraphic ),
                            M::Item( _( "Trim Graphic" ), A::trimGraphic ),
                            M::Item( _( "Merge Polygons" ), A::mergePolygons ),
                            M::Item( _( "Subtract Polygons" ), A::subtractPolygons ),
                            M::Item( _( "Intersect Polygons" ), A::intersectPolygons ),
                            M::Item( _( "Edit Vertices" ), A::editVertices ),
                            M::Item( _( "Move Corner" ), A::pointEditorMoveCorner ),
                            M::Item( _( "Move Midpoint" ), A::pointEditorMoveMidpoint ),
                            M::Item( _( "Add Corner" ), A::pointEditorAddCorner ),
                            M::Item( _( "Remove Corner" ), A::pointEditorRemoveCorner ),
                            M::Item( _( "Chamfer Corner" ), A::pointEditorChamferCorner ),
                            M::Item( _( "Cycle Arc Edit Mode" ), ACTIONS::cycleArcEditMode ) } ),
              M::Submenu( _( "Constraints" ), anything,
                          { M::Item( _( "Remove Constraints" ), A::removeConstraints ),
                            M::Item( _( "Show Constraints" ), A::showConstraints ),
                            M::Item( _( "Hide Constraints" ), A::hideConstraints ),
                            M::Item( _( "Manage Constraints" ), A::manageConstraints ) } ),
              M::Submenu( _( "Table Actions" ), tableCells,
                          { M::Item( _( "Edit Table..." ), ACTIONS::editTable ),
                            M::Item( _( "Export Table as CSV..." ), ACTIONS::exportTableCSV ),
                            M::Item( _( "Add Row Above" ), ACTIONS::addRowAbove ),
                            M::Item( _( "Add Row Below" ), ACTIONS::addRowBelow ),
                            M::Item( _( "Add Column Before" ), ACTIONS::addColBefore ),
                            M::Item( _( "Add Column After" ), ACTIONS::addColAfter ),
                            M::Item( _( "Delete Rows" ), ACTIONS::deleteRows ),
                            M::Item( _( "Delete Columns" ), ACTIONS::deleteColumns ),
                            M::Item( _( "Merge Cells" ), ACTIONS::mergeCells ),
                            M::Item( _( "Unmerge Cells" ), ACTIONS::unmergeCells ),
                            M::Item( _( "Select Columns" ), ACTIONS::selectColumns ),
                            M::Item( _( "Select Rows" ), ACTIONS::selectRows ),
                            M::Item( _( "Select Table" ), ACTIONS::selectTable ) } ),
              align,
              M::Item( _( "Cross Probe" ), A::selectOnSchematic ),
              M::Submenu( _( "Select" ), S::ShowAlways,
                          { M::Item( _( "Select All" ), ACTIONS::selectAll ),
                            M::Item( _( "Unselect All" ), ACTIONS::unselectAll ),
                            M::Item( _( "Filter Selection" ), A::filterSelection ),
                            M::Item( _( "Select Connection" ), A::selectConnection ),
                            M::Item( _( "Select Net" ), A::selectNet ),
                            M::Item( _( "Select Net Chain" ), A::selectNetChain ),
                            M::Item( _( "Select Items on Same Sheet" ), A::selectSameSheet ),
                            M::Item( _( "Select Unconnected" ), A::selectUnconnected ),
                            M::Item( _( "Grab Unconnected" ), A::grabUnconnected ),
                            M::Item( _( "Highlight Net Chain" ), A::highlightNetChain ) } ),
              M::Separator(),
              M::Item( _( "Cut" ), ACTIONS::cut ),
              M::Item( _( "Copy" ), ACTIONS::copy ),
              M::Item( _( "Copy with Reference" ), A::copyWithReference, anything ),
              M::Item( _( "Copy as Text" ), ACTIONS::copyAsText, anything ),
              M::Item( _( "Paste" ), ACTIONS::paste ),
              M::Item( _( "Paste Special" ), ACTIONS::pasteSpecial ),
              M::Item( _( "Duplicate" ), ACTIONS::duplicate, anything ),
              M::Item( _( "Delete" ), ACTIONS::doDelete, anything ),
              M::Separator(),
              M::Item( _( "Applicable Unary Rules..." ), A::inspectConstraints, S::Count( 1 ) ),
              M::Item( _( "Applicable Binary Rules..." ), A::inspectClearance, S::Count( 2 ) ),
              M::Separator(),
              M::Submenu( _( "Violations" ), S::MoreThan( 0 ) && S::OnlyTypes( markerTypes ),
                          { M::Item( _( "Exclude Marker" ), ACTIONS::excludeMarker ),
                            M::Item( _( "Next Marker" ), ACTIONS::nextMarker ),
                            M::Item( _( "Previous Marker" ), ACTIONS::prevMarker ) } ),
              M::Item( _( "Preferences..." ), ACTIONS::openPreferences ),
              M::Separator(),
              M::Item( _( "Properties..." ), A::properties, anything ) } );
}

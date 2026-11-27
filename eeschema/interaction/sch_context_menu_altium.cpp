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

#include "sch_context_menu_altium.h"

#include "sch_menus_altium.h"

#include <eeschema_settings.h>
#include <sch_edit_frame.h>
#include <schematic.h>
#include <tool/actions.h>
#include <tool/selection_conditions.h>
#include <tools/sch_actions.h>
#include <tools/sch_edit_tool.h>
#include <tools/sch_line_wire_bus_tool.h>
#include <tools/sch_selection_tool.h>
#include <wx/translation.h>

namespace
{
using M = MENU_DEF;
using S = SELECTION_CONDITIONS;
using C = SCH_CONDITIONS;

// These type lists are the ones the tools' own Init functions use for the same actions
const std::vector<KICAD_T> connectedLineTypes = { SCH_ITEM_LOCATE_WIRE_T, SCH_ITEM_LOCATE_BUS_T };
const std::vector<KICAD_T> connectedTypes = { SCH_LINE_T,      SCH_BUS_WIRE_ENTRY_T,  SCH_BUS_BUS_ENTRY_T,
                                              SCH_LABEL_T,     SCH_HIER_LABEL_T,      SCH_GLOBAL_LABEL_T,
                                              SCH_SHEET_PIN_T, SCH_DIRECTIVE_LABEL_T, SCH_JUNCTION_T };
const std::vector<KICAD_T> sheetTypes = { SCH_SHEET_T };
const std::vector<KICAD_T> tableCellTypes = { SCH_TABLECELL_T };
const std::vector<KICAD_T> attributeTypes = { SCH_SYMBOL_T, SCH_SHEET_T, SCH_RULE_AREA_T };
const std::vector<KICAD_T> labelTypes = { SCH_LABEL_T, SCH_DIRECTIVE_LABEL_T, SCH_GLOBAL_LABEL_T, SCH_HIER_LABEL_T };
const std::vector<KICAD_T> textTypes = { SCH_LABEL_T, SCH_DIRECTIVE_LABEL_T, SCH_GLOBAL_LABEL_T, SCH_HIER_LABEL_T,
                                         SCH_TEXT_T,  SCH_TEXTBOX_T };
const std::vector<KICAD_T> shapeTypes = { SCH_SHAPE_T };
const std::vector<KICAD_T> pinTypes = { SCH_PIN_T };


/// A slot that shows the submenu KiCad's tools build at run time under \a aTitle, while \a aCondition holds
M slotOf( const char* aTitle, const SELECTION_CONDITION& aCondition )
{
    M slot = M::Slot( wxGetTranslation( wxString::FromUTF8( aTitle ) ), aTitle );
    slot.m_show = aCondition;
    return slot;
}
} // namespace


MENU_DEF SchAltiumContextMenu( SCH_EDIT_FRAME* aFrame, SCH_SELECTION_TOOL* aSelectionTool )
{
    const SELECTION_CONDITION belowRootSheet = [aFrame]( const SELECTION& )
    {
        return aFrame && aFrame->GetCurrentSheet().Last() != &aFrame->Schematic().Root();
    };

    const SELECTION_CONDITION inGroup = [aSelectionTool]( const SELECTION& )
    {
        return aSelectionTool && aSelectionTool->GetEnteredGroup() != nullptr;
    };

    const SELECTION_CONDITION noVariantActive = [aFrame]( const SELECTION& )
    {
        return !aFrame || aFrame->Schematic().GetCurrentVariant().IsEmpty();
    };

    const SELECTION_CONDITION variantActive = [aFrame]( const SELECTION& )
    {
        return aFrame && !aFrame->Schematic().GetCurrentVariant().IsEmpty();
    };

    const SELECTION_CONDITION allowPinSwaps = [aFrame]( const SELECTION& )
    {
        return aFrame && aFrame->eeconfig() && aFrame->eeconfig()->m_Input.allow_unconstrained_pin_swaps;
    };

    const SELECTION_CONDITION orientable = []( const SELECTION& aSelection )
    {
        return !SCH_LINE_WIRE_BUS_TOOL::IsDrawingLineWireOrBus( aSelection )
               && S::HasTypes( SCH_EDIT_TOOL::RotatableItems )( aSelection );
    };

    const SELECTION_CONDITION idle = C::Idle;
    const SELECTION_CONDITION wireOrPin = S::HasTypes( { SCH_ITEM_LOCATE_WIRE_T, SCH_ITEM_LOCATE_BUS_T, SCH_PIN_T } );
    const SELECTION_CONDITION symbol = S::HasTypes( { SCH_SYMBOL_T } );
    const SELECTION_CONDITION wireOrBus = S::Count( 1 ) && S::OnlyTypes( connectedLineTypes );
    const SELECTION_CONDITION anyWireOrBus = S::HasTypes( connectedLineTypes );
    const SELECTION_CONDITION lines = S::MoreThan( 0 ) && S::OnlyTypes( { SCH_LINE_T } );
    const SELECTION_CONDITION connected = S::MoreThan( 0 ) && S::OnlyTypes( connectedTypes );
    const SELECTION_CONDITION sheet = S::Count( 1 ) && S::OnlyTypes( sheetTypes );
    const SELECTION_CONDITION label = S::HasTypes( labelTypes );
    const SELECTION_CONDITION pins = S::MoreThan( 1 ) && S::OnlyTypes( pinTypes );
    const SELECTION_CONDITION pinOrSheetPin = C::AllPinsOrSheetPins;
    const SELECTION_CONDITION tableCells = S::MoreThan( 0 ) && S::OnlyTypes( tableCellTypes );
    const SELECTION_CONDITION changeable = S::OnlyTypes( textTypes );
    const SELECTION_CONDITION swappable = S::OnlyTypes( SCH_EDIT_TOOL::SwappableItems ) && S::MoreThan( 1 );
    const SELECTION_CONDITION image = S::Count( 1 ) && S::OnlyTypes( { SCH_BITMAP_T } );
    const SELECTION_CONDITION group = S::Count( 1 ) && S::HasType( SCH_GROUP_T );
    const SELECTION_CONDITION shapes = S::Count( 1 ) && S::OnlyTypes( shapeTypes );

    M place = SchAltiumPlaceMenu();
    place.m_label = _( "Place" );

    M align = SchAltiumAlignMenu();
    align.m_label = _( "Align" );
    align.m_show = S::MoreThan( 1 );

    // The items are shown by the conditions of the tools' own menus, so a right-click offers the same things in either
    // profile and only the arrangement differs
    return M::Submenu(
            wxEmptyString,
            { M::Item( _( "Find Text..." ), ACTIONS::find ),
              M::Item( _( "Clear Filter" ), SCH_ACTIONS::clearHighlight ),
              M::Separator(),
              M::Item( _( "Draw Wires" ), SCH_ACTIONS::drawWire, wireOrPin ),
              M::Item( _( "Finish" ), ACTIONS::finishInteractive, SCH_LINE_WIRE_BUS_TOOL::IsDrawingLineWireOrBus ),
              M::Separator(),
              place,
              M::Submenu(
                      _( "Part Actions" ), S::ShowAlways,
                      { M::Item( _( "Update Symbol" ), SCH_ACTIONS::updateSymbol, C::SingleSymbolOrPower ),
                        M::Item( _( "Update Symbols" ), SCH_ACTIONS::updateSymbols ),
                        M::Item( _( "Change Symbol" ), SCH_ACTIONS::changeSymbol,
                                 C::SingleSymbolOrPower && noVariantActive ),
                        M::Item( _( "Change Symbols" ), SCH_ACTIONS::changeSymbols,
                                 C::MultipleSymbolsOrPower && noVariantActive ),
                        M::Item( _( "Edit with Symbol Editor" ), SCH_ACTIONS::editWithLibEdit,
                                 C::SingleSymbolOrPower && idle ),
                        M::Item( _( "Set Variant Symbol" ), SCH_ACTIONS::setVariantSymbol,
                                 C::SingleSymbolOrPower && variantActive ),
                        M::Item( _( "Clear Variant Symbol" ), SCH_ACTIONS::clearVariantSymbol,
                                 C::SingleSymbolOrPower && variantActive ),
                        slotOf( SCH_CONTEXT_SLOT::SYMBOL_UNIT, C::SingleMultiUnitSymbol ),
                        slotOf( SCH_CONTEXT_SLOT::BODY_STYLE, C::SingleMultiBodyStyleSymbol ),
                        M::Submenu( _( "Edit Main Fields" ), C::SingleSymbol,
                                    { M::Item( _( "Edit Reference" ), SCH_ACTIONS::editReference ),
                                      M::Item( _( "Edit Value" ), SCH_ACTIONS::editValue ),
                                      M::Item( _( "Edit Footprint" ), SCH_ACTIONS::editFootprint ) } ),
                        M::Item( _( "Autoplace Fields" ), SCH_ACTIONS::autoplaceFields, symbol ),
                        M::Submenu( _( "Attributes" ), S::HasTypes( attributeTypes ),
                                    { M::CheckItem( _( "Do Not Populate" ), SCH_ACTIONS::setDNP ),
                                      M::CheckItem( _( "Exclude from Simulation" ), SCH_ACTIONS::setExcludeFromSim ),
                                      M::CheckItem( _( "Exclude from BOM" ), SCH_ACTIONS::setExcludeFromBOM ),
                                      M::CheckItem( _( "Exclude from Board" ), SCH_ACTIONS::setExcludeFromBoard ),
                                      M::CheckItem( _( "Exclude from Position Files" ),
                                                    SCH_ACTIONS::setExcludeFromPosFiles ) } ),
                        M::Item( _( "Show Datasheet" ), ACTIONS::showDatasheet, C::SingleSymbolOrPower ),
                        M::Item( _( "Select PCB Components" ), SCH_ACTIONS::selectOnPCB, symbol && idle ) } ),
              M::Submenu( _( "Sheet Symbol Actions" ), sheet,
                          { M::Item( _( "Open Sheet" ), SCH_ACTIONS::enterSheet, idle ),
                            M::Item( _( "Place Sheet Entry" ), SCH_ACTIONS::placeSheetPin, idle ),
                            M::Item( _( "Autoplace Sheet Pins" ), SCH_ACTIONS::autoplaceAllSheetPins, idle ),
                            M::Item( _( "Synchronize Sheet Pins" ), SCH_ACTIONS::syncSheetPins, idle ),
                            M::Item( _( "Cleanup Sheet Pins" ), SCH_ACTIONS::cleanupSheetPins ),
                            M::Item( _( "Edit Page Number" ), SCH_ACTIONS::editPageNumber ) } ),
              M::Submenu( _( "Sheet Actions" ), S::ShowAlways,
                          { M::Item( _( "Leave Sheet" ), SCH_ACTIONS::leaveSheet, belowRootSheet ),
                            M::Item( _( "Page Settings" ), ACTIONS::pageSettings ) } ),
              M::Submenu( _( "Pin Actions" ), pinOrSheetPin,
                          { slotOf( SCH_CONTEXT_SLOT::PIN_FUNCTION, C::SingleMultiFunctionPin ),
                            slotOf( SCH_CONTEXT_SLOT::PIN_HELPERS, C::AllPinsOrSheetPins ),
                            M::Item( _( "Swap Pin Labels" ), SCH_ACTIONS::swapPinLabels, pins && idle ),
                            M::Item( _( "Swap Unit Labels" ), SCH_ACTIONS::swapUnitLabels, idle ),
                            M::Item( _( "Swap Pins" ), SCH_ACTIONS::swapPins, pins && idle && allowPinSwaps ),
                            slotOf( SCH_CONTEXT_SLOT::NET_CHAIN, idle ) } ),
              M::Submenu( _( "Wire Actions" ), anyWireOrBus,
                          { M::Item( _( "Add Junction" ), SCH_ACTIONS::placeJunction, wireOrBus && idle ),
                            M::Item( _( "Add Net Label" ), SCH_ACTIONS::placeLabel, wireOrBus && idle ),
                            M::Item( _( "Add Directive Label" ), SCH_ACTIONS::placeClassLabel, wireOrBus && idle ),
                            M::Item( _( "Add Off-Sheet Connector" ), SCH_ACTIONS::placeGlobalLabel, wireOrBus && idle ),
                            M::Item( _( "Add Port" ), SCH_ACTIONS::placeHierLabel, wireOrBus && idle ),
                            M::Item( _( "Break Wire" ), SCH_ACTIONS::breakWire, lines && idle ),
                            M::Item( _( "Slice Wire" ), SCH_ACTIONS::slice, lines && idle ),
                            slotOf( SCH_CONTEXT_SLOT::BUS_UNFOLD, S::MoreThan( 0 ) && idle ),
                            M::Item( _( "Select Connection" ), SCH_ACTIONS::selectConnection, idle ),
                            M::Item( _( "Assign Netclass" ), SCH_ACTIONS::assignNetclass, connected && idle ),
                            M::Item( _( "Find Net in Inspector" ), SCH_ACTIONS::findNetInInspector, connected && idle ),
                            slotOf( SCH_CONTEXT_SLOT::NET_CHAIN, idle ) } ),
              M::Submenu( _( "Port Actions" ), label,
                          { M::Submenu( _( "Change To" ), changeable,
                                        { M::Item( _( "Net Label" ), SCH_ACTIONS::toLabel ),
                                          M::Item( _( "Directive Label" ), SCH_ACTIONS::toDLabel ),
                                          M::Item( _( "Port" ), SCH_ACTIONS::toHLabel ),
                                          M::Item( _( "Off-Sheet Connector" ), SCH_ACTIONS::toGLabel ),
                                          M::Item( _( "Text" ), SCH_ACTIONS::toText ),
                                          M::Item( _( "Text Box" ), SCH_ACTIONS::toTextBox ) } ) } ),
              M::Submenu( _( "Table Actions" ), tableCells && idle,
                          { M::Item( _( "Select Columns" ), ACTIONS::selectColumns ),
                            M::Item( _( "Select Rows" ), ACTIONS::selectRows ),
                            M::Item( _( "Select Table" ), ACTIONS::selectTable ) } ),
              M::Submenu( _( "Graphics Actions" ), shapes || image || C::SingleNonExcludedMarker,
                          { M::Item( _( "Convert Image to Outlines" ), SCH_ACTIONS::convertImageToOutlines, image ),
                            M::Item( _( "Add Corner" ), SCH_ACTIONS::pointEditorAddCorner, shapes ),
                            M::Item( _( "Remove Corner" ), SCH_ACTIONS::pointEditorRemoveCorner, shapes ),
                            M::Item( _( "Cycle Arc Edit Mode" ), ACTIONS::cycleArcEditMode, shapes ),
                            M::Item( _( "Exclude Marker" ), ACTIONS::excludeMarker, C::SingleNonExcludedMarker ) } ),
              M::Submenu( _( "Transform" ), S::NotEmpty,
                          { M::Item( _( "Rotate Counterclockwise" ), SCH_ACTIONS::rotateCCW, orientable ),
                            M::Item( _( "Rotate Clockwise" ), SCH_ACTIONS::rotateCW, orientable ),
                            M::Item( _( "Mirror Vertically" ), SCH_ACTIONS::mirrorV, orientable ),
                            M::Item( _( "Mirror Horizontally" ), SCH_ACTIONS::mirrorH, orientable ),
                            M::Item( _( "Swap" ), SCH_ACTIONS::swap, swappable ) } ),
              M::Submenu(
                      _( "Unions" ), S::NotEmpty,
                      { M::Item( _( "Group" ), ACTIONS::group ), M::Item( _( "Ungroup" ), ACTIONS::ungroup ),
                        M::Item( _( "Add to Group" ), ACTIONS::addToGroup ),
                        M::Item( _( "Remove from Group" ), ACTIONS::removeFromGroup ),
                        M::Item( _( "Enter Group" ), ACTIONS::groupEnter, group ),
                        M::Item( _( "Leave Group" ), ACTIONS::groupLeave, inGroup ),
                        M::Item( _( "Place Linked Design Block" ), SCH_ACTIONS::placeLinkedDesignBlock, group ),
                        M::Item( _( "Save to Linked Design Block" ), SCH_ACTIONS::saveToLinkedDesignBlock, group ) } ),
              M::Submenu( _( "Locking" ), S::NotEmpty,
                          { M::Item( _( "Lock" ), SCH_ACTIONS::lock, C::HasUnlockedItems ),
                            M::Item( _( "Unlock" ), SCH_ACTIONS::unlock, C::HasLockedItems ),
                            M::Item( _( "Toggle Lock" ), SCH_ACTIONS::toggleLock ) } ),
              M::Submenu( _( "Select" ), S::ShowAlways,
                          { M::Item( _( "Select All" ), ACTIONS::selectAll ),
                            M::Item( _( "Unselect All" ), ACTIONS::unselectAll ) } ),
              align,
              M::Item( _( "Cross Probe" ), SCH_ACTIONS::selectOnPCB ),
              M::Separator(),
              M::Item( _( "Cut" ), ACTIONS::cut ),
              M::Item( _( "Copy" ), ACTIONS::copy ),
              M::Item( _( "Copy as Text" ), ACTIONS::copyAsText, C::IdleSelection ),
              M::Item( _( "Paste" ), ACTIONS::paste, idle ),
              M::Item( _( "Paste Special" ), ACTIONS::pasteSpecial, idle ),
              M::Item( _( "Duplicate" ), ACTIONS::duplicate, C::IdleSelection ),
              M::Item( _( "Delete" ), ACTIONS::doDelete, S::NotEmpty ),
              M::Separator(),
              M::Item( _( "Project Options..." ), SCH_ACTIONS::schematicSetup ),
              M::Item( _( "Preferences..." ), ACTIONS::openPreferences ),
              M::Separator(),
              M::Item( _( "Properties..." ), SCH_ACTIONS::properties, S::NotEmpty ) } );
}

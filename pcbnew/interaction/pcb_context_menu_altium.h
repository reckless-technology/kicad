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

#ifndef PCB_CONTEXT_MENU_ALTIUM_H
#define PCB_CONTEXT_MENU_ALTIUM_H

#include <menu_def.h>

class PCB_EDIT_FRAME;
class PCB_SELECTION_TOOL;

/**
 * The PCB editor's idle right-click menu for the Altium-style interaction profile, as a tree (interaction-profile note,
 * 5.4 and 6.6, as Altium's own menu was captured). It has two forms, told apart by the selection: with nothing under
 * the cursor, routing, Place and the clipboard; over an object, a fixed order of submenus whose entries come and go
 * with the kind of object. Each node's condition is the one KiCad's own menu uses for the same action. A slot names a
 * submenu that KiCad's tools build at run time: the selection tool fills it with that menu, so the two profiles
 * share it.
 *
 * Built on each call, so the labels follow the language. Both arguments may be null, as in a test: conditions that read
 * the frame or the tool then answer false.
 */
MENU_DEF PcbAltiumContextMenu( PCB_EDIT_FRAME* aFrame, PCB_SELECTION_TOOL* aSelectionTool );

/// The titles of the submenus KiCad's tools build at run time that the tree's slots show, which are the slots' names.
namespace PCB_CONTEXT_SLOT
{
constexpr const char* NET_CHAINS = "Net Chains...";
} // namespace PCB_CONTEXT_SLOT

#endif // PCB_CONTEXT_MENU_ALTIUM_H

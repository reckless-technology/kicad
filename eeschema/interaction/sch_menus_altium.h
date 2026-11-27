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

#ifndef SCH_MENUS_ALTIUM_H
#define SCH_MENUS_ALTIUM_H

#include <utility>
#include <vector>

#include <menu_def.h>

/**
 * The schematic editor's menu bar for the Altium-style interaction profile, as a tree (interaction-profile note,
 * 5.4 and 6.1). Built on each call, so the labels follow the language.
 */
MENU_DEF SchAltiumMenuBar();

/// The Place menu of the tree (the `P` sequence), which the right-click menu's Place submenu is too.
MENU_DEF SchAltiumPlaceMenu();

/// The Align menu of the tree (the `A` sequence), which the right-click menu's Align submenu is too.
MENU_DEF SchAltiumAlignMenu();

/**
 * What the note's schematic table has that the tree leaves out on purpose: the entries marked "new" or "-", and the
 * aliases of an entry the tree already has.
 */
const std::vector<MENU_OMISSION>& SchAltiumMenuOmissions();

/// The key-sequence roots of the schematic editor: each root action and the mnemonic of the menu it opens
const std::vector<std::pair<const TOOL_ACTION*, wxUniChar>>& SchAltiumSequenceRoots();

/// The slots in the tree that the frame fills in (see MENU_SLOT_FILLER).
namespace SCH_MENU_SLOT
{
constexpr const char* OPEN_RECENT = "open-recent";
constexpr const char* EXIT = "exit";
constexpr const char* PLUGINS = "plugins";
constexpr const char* LANGUAGES = "languages";
} // namespace SCH_MENU_SLOT

#endif // SCH_MENUS_ALTIUM_H

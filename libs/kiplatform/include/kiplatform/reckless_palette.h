/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef KIPLATFORM_RECKLESS_PALETTE_H_
#define KIPLATFORM_RECKLESS_PALETTE_H_

/**
 * @file
 * The Reckless window colors, as 0xRRGGBB, shared by every platform's native chrome code.
 *
 * Text colors are chosen by luminance, never by hue alone: DISABLED_TEXT measures 3.08:1 on WINDOW, 3.35:1 on PLATE
 * and 3.64:1 on PANEL.
 */
namespace KIPLATFORM::RECKLESS_PALETTE
{
constexpr unsigned WINDOW = 0x29282c;        // frames, dialogs, menu bar
constexpr unsigned PANEL = 0x1b1a1d;         // edit, list, tree, grid backgrounds
constexpr unsigned PLATE = 0x232125;         // toolbars, button faces, panes
constexpr unsigned RULE = 0x3b3936;          // 3D shadow, separators
constexpr unsigned INK = 0xe8dcc8;           // text
constexpr unsigned MUTED = 0x9c9483;         // secondary text
constexpr unsigned DISABLED_TEXT = 0x78726a; // grayed text
constexpr unsigned AMBER = 0xe0a038;         // accent
constexpr unsigned AMBER_DIM = 0x6e5428;     // static box border, default-button edge
constexpr unsigned SELECT = 0x5d4423;        // selection background
} // namespace KIPLATFORM::RECKLESS_PALETTE

#endif // KIPLATFORM_RECKLESS_PALETTE_H_

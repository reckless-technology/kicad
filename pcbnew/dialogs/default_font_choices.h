/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

#pragma once

#include <wx/arrstr.h>
#include <wx/string.h>

class wxGridCellAttr;

/**
 * The default font face cells of the text defaults grids (Board Setup > Text & Graphics Defaults and
 * Preferences > Footprint Editor > Defaults).
 *
 * A default font face has three states: not set, the empty string, shown as "(not set)"; #KICAD_FONT_NAME, the
 * stroke font; or the name of a face.
 */

/**
 * @return "(not set)", the KiCad stroke font, then every installed family in alphabetical order.
 */
wxArrayString DefaultFontChoices();

/**
 * @return the attributes of a default font face cell, a choice of @a aChoices that also takes other names, so a
 *         face that is not installed on this machine but named by the settings is kept.
 */
wxGridCellAttr* DefaultFontCellAttr( const wxArrayString& aChoices );

/**
 * @return the cell text for the default font face setting @a aFontName.
 */
wxString DefaultFontToCell( const wxString& aFontName );

/**
 * @return the default font face setting for the cell text @a aCellText.
 */
wxString DefaultFontFromCell( const wxString& aCellText );

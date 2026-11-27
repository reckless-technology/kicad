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

#ifndef KIPLATFORM_SHARED_STATE_H_
#define KIPLATFORM_SHARED_STATE_H_

#include <wx/string.h>

/**
 * @file
 * Values the platform layer sets in one part of the program and reads in another.
 *
 * kiplatform is a static library, so kicommon, each kiface and each program carry their own copy of it, and a
 * static variable set in one copy is not seen by the others: a value pgm_base.cpp (kicommon) sets would read as
 * unset in EDA_BASE_FRAME (common, linked into each kiface). These values live in wx's system options instead,
 * which belong to the one wx library every module shares.
 */
namespace KIPLATFORM::SHARED_STATE
{
/// The interface font family UI::SetInterfaceFontFamily() resolved
inline const wxString INTERFACE_FONT_FAMILY = wxS( "kicad.interface-font-family" );

/// Set (to "1") once APP::EnableDarkMode() has forced the house dark look, so APP::GetCaptionColors() has colors
inline const wxString HOUSE_CAPTION = wxS( "kicad.house-caption" );

/// Set a value for the whole process, or clear it with an empty string.
void Set( const wxString& aName, const wxString& aValue );

/// @return the value set for the process, or an empty string if none is.
wxString Get( const wxString& aName );
} // namespace KIPLATFORM::SHARED_STATE

#endif // KIPLATFORM_SHARED_STATE_H_

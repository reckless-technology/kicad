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

#include "default_font_choices.h"

#include <font/fontconfig.h>
#include <font/kicad_font_name.h>
#include <pgm_base.h>

#include <wx/grid.h>
#include <wx/intl.h>

#include <string>
#include <vector>


static wxString notSetText()
{
    return _( "(not set)" );
}


wxArrayString DefaultFontChoices()
{
    std::vector<std::string> families;
    Fontconfig()->ListFonts( families, std::string( Pgm().GetLanguageTag().utf8_str() ) );

    wxArrayString fontNames;

    for( const std::string& family : families )
        fontNames.Add( wxString::FromUTF8( family ) );

    fontNames.Sort();
    fontNames.Insert( KICAD_FONT_NAME, 0 );
    fontNames.Insert( notSetText(), 0 );

    return fontNames;
}


wxGridCellAttr* DefaultFontCellAttr( const wxArrayString& aChoices )
{
    wxGridCellAttr* attr = new wxGridCellAttr;
    attr->SetEditor( new wxGridCellChoiceEditor( aChoices, true ) );
    return attr;
}


wxString DefaultFontToCell( const wxString& aFontName )
{
    return aFontName.IsEmpty() ? notSetText() : aFontName;
}


wxString DefaultFontFromCell( const wxString& aCellText )
{
    wxString fontName = aCellText;
    fontName.Trim().Trim( false );

    return fontName == notSetText() ? wxString() : fontName;
}

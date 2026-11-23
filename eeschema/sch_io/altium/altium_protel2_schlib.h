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

#ifndef ALTIUM_PROTEL2_SCHLIB_H
#define ALTIUM_PROTEL2_SCHLIB_H

#include <cstdint>
#include <map>
#include <vector>

#include <wx/string.h>


/// The first line of a schematic library in the Protel for Windows 1.2 - 2.0 format
#define ALTIUM_PROTEL2_SCHLIB_MAGIC "Protel for Windows - Schematic Library Editor Binary File Version 1.2 - 2.0"


/**
 * One symbol of a Protel for Windows 1.2 - 2.0 schematic library, as the records a binary
 * Altium SchLib would hold for it.
 *
 * Each record is a map of upper-case Altium property names to values, as ALTIUM_BINARY_PARSER
 * reads them from an Altium SchLib, so the symbol is built by the same code.  The component
 * record comes first.  Parts are numbered from 1 in OWNERPARTID and display modes from 0 in
 * OWNERPARTDISPLAYMODE.
 */
struct ALTIUM_PROTEL2_SYMBOL
{
    wxString m_Name;
    int      m_PartCount = 0;
    int      m_DisplayModeCount = 0;

    std::vector<std::map<wxString, wxString>> m_Records;
};


/**
 * Reader for the binary schematic library format of Protel for Windows 1.2 to 2.0, which is not
 * an OLE compound file and which no other part of KiCad reads.
 *
 * A file holds a header, a font table, a directory of symbol names and then one record per
 * symbol: its name, description, footprint and designator, followed by its primitives (pins,
 * lines, polylines, polygons, rectangles, ellipses and arcs) part by part and display mode by
 * display mode.  Coordinates are 16-bit integers in units of 10 mil with y up, as in a binary
 * Altium SchLib; colours are 32-bit BGR values and arc angles six-byte Turbo Pascal reals.
 */
class ALTIUM_PROTEL2_SCHLIB
{
public:
    /**
     * @throw IO_ERROR when the data is not a library in this format or a record cannot be read.
     */
    explicit ALTIUM_PROTEL2_SCHLIB( const std::vector<uint8_t>& aData );

    /**
     * @return whether @p aFileName starts with the format's header.
     */
    static bool IsProtel2SchLib( const wxString& aFileName );

    /**
     * @return the symbols in the order of the library's directory.  A name the directory repeats
     *         is given the suffix " (Altium duplicate n)" from its second occurrence on.
     */
    const std::vector<ALTIUM_PROTEL2_SYMBOL>& GetSymbols() const { return m_symbols; }

    /**
     * Decode a six-byte Turbo Pascal real.
     */
    static double DecodeReal48( const uint8_t* aBytes );

private:
    void readSymbol( const wxString& aName, size_t aStart, size_t aEnd );

    const std::vector<uint8_t>&        m_data;
    std::vector<ALTIUM_PROTEL2_SYMBOL> m_symbols;
};

#endif // ALTIUM_PROTEL2_SCHLIB_H

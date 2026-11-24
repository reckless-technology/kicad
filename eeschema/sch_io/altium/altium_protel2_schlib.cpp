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

#include "altium_protel2_schlib.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>

#include <io/io_utils.h>
#include <ki_exception.h>
#include <wx/arrstr.h>
#include <wx/translation.h>


namespace
{

// Altium SchLib record numbers of the primitives this format holds
constexpr int RECORD_COMPONENT = 1;
constexpr int RECORD_PIN = 2;
constexpr int RECORD_POLYLINE = 6;
constexpr int RECORD_POLYGON = 7;
constexpr int RECORD_ELLIPSE = 8;
constexpr int RECORD_ELLIPTICAL_ARC = 11;
constexpr int RECORD_ARC = 12;
constexpr int RECORD_LINE = 13;
constexpr int RECORD_RECTANGLE = 14;
constexpr int RECORD_DESIGNATOR = 34;
constexpr int RECORD_PARAMETER = 41;
constexpr int RECORD_IMPLEMENTATION = 45;

// Primitive type bytes in the file
constexpr uint8_t TYPE_PIN = 0x02;
constexpr uint8_t TYPE_POLYLINE = 0x06;
constexpr uint8_t TYPE_POLYGON = 0x07;
constexpr uint8_t TYPE_ELLIPSE = 0x08;
constexpr uint8_t TYPE_ARC = 0x0B;
constexpr uint8_t TYPE_LINE = 0x0D;
constexpr uint8_t TYPE_RECTANGLE = 0x0E;

// Ends a display mode; two or more in a row end a part
constexpr uint8_t TERMINATOR = 0xFF;


wxString number( long aValue )
{
    return wxString::Format( wxT( "%ld" ), aValue );
}


wxString boolean( bool aValue )
{
    return aValue ? wxString( wxT( "T" ) ) : wxString( wxT( "F" ) );
}


/// A primitive as read, before it becomes a record
struct PRIMITIVE
{
    std::map<wxString, wxString>     m_Props;
    std::vector<std::pair<int, int>> m_Points; ///< the points that place the designator
};

} // namespace


namespace
{

/**
 * Little-endian reads with a bounds check on every one.
 */
struct PRIMITIVE_READER
{
    const std::vector<uint8_t>& m_Data;
    size_t                      m_Pos;

    void need( size_t aCount ) const
    {
        if( m_Pos + aCount > m_Data.size() )
            THROW_IO_ERROR( _( "The Protel schematic library ends inside a record." ) );
    }

    uint8_t U8()
    {
        need( 1 );
        return m_Data[m_Pos++];
    }

    uint16_t U16()
    {
        need( 2 );
        uint16_t value = m_Data[m_Pos] | ( m_Data[m_Pos + 1] << 8 );
        m_Pos += 2;
        return value;
    }

    int16_t I16() { return static_cast<int16_t>( U16() ); }

    uint32_t U32()
    {
        uint32_t low = U16();
        return low | ( static_cast<uint32_t>( U16() ) << 16 );
    }

    void Skip( size_t aCount )
    {
        need( aCount );
        m_Pos += aCount;
    }

    /// A string prefixed by its length in one byte, in Latin-1
    wxString PascalString()
    {
        size_t length = U8();
        need( length );
        wxString text( reinterpret_cast<const char*>( m_Data.data() + m_Pos ), wxConvISO8859_1, length );
        m_Pos += length;
        return text;
    }

    double Real48()
    {
        need( 6 );
        double value = ALTIUM_PROTEL2_SCHLIB::DecodeReal48( m_Data.data() + m_Pos );
        m_Pos += 6;
        return value;
    }
};

} // namespace


double ALTIUM_PROTEL2_SCHLIB::DecodeReal48( const uint8_t* aBytes )
{
    if( aBytes[0] == 0 )
        return 0.0;

    // Exponent biased by 129, then a 39-bit mantissa with an implied leading one and a sign bit
    int    exponent = static_cast<int>( aBytes[0] ) - 129;
    double mantissa = ( static_cast<double>( aBytes[1] ) + static_cast<double>( aBytes[2] ) * 256.0
                        + static_cast<double>( aBytes[3] ) * 65536.0 + static_cast<double>( aBytes[4] ) * 16777216.0
                        + static_cast<double>( aBytes[5] & 0x7F ) * 4294967296.0 )
                      / 549755813888.0; // 2^39

    double value = std::ldexp( 1.0 + mantissa, exponent );
    return ( aBytes[5] & 0x80 ) ? -value : value;
}


bool ALTIUM_PROTEL2_SCHLIB::IsProtel2SchLib( const wxString& aFileName )
{
    std::string          magic = ALTIUM_PROTEL2_SCHLIB_MAGIC;
    std::vector<uint8_t> header{ static_cast<uint8_t>( magic.size() ) };

    header.insert( header.end(), magic.begin(), magic.end() );
    return IO_UTILS::fileHasBinaryHeader( aFileName, header );
}


static PRIMITIVE readPrimitive( PRIMITIVE_READER& aReader, uint8_t aType )
{
    PRIMITIVE                     primitive;
    std::map<wxString, wxString>& props = primitive.m_Props;

    auto readPoints = [&]( uint16_t aCount )
    {
        props[wxT( "LOCATIONCOUNT" )] = number( aCount );

        for( int i = 1; i <= aCount; i++ )
        {
            int x = aReader.I16();
            int y = aReader.I16();

            props[wxString::Format( wxT( "X%d" ), i )] = number( x );
            props[wxString::Format( wxT( "Y%d" ), i )] = number( y );
            primitive.m_Points.emplace_back( x, y );
        }
    };

    // The border and fill that rectangles and ellipses end with: width, line colour, area colour,
    // a byte not understood, and whether the shape is filled
    auto readBorderAndFill = [&]()
    {
        props[wxT( "LINEWIDTH" )] = number( aReader.U8() );
        props[wxT( "COLOR" )] = number( aReader.U32() );
        props[wxT( "AREACOLOR" )] = number( aReader.U32() );
        aReader.Skip( 1 );
        props[wxT( "ISSOLID" )] = boolean( aReader.U8() != 0 );
    };

    switch( aType )
    {
    case TYPE_PIN:
    {
        uint8_t flags[6];

        for( uint8_t& flag : flags )
            flag = aReader.U8();

        int length = aReader.I16();
        int x = aReader.I16();
        int y = aReader.I16();
        int rotation = aReader.U8() & 0x03;

        props[wxT( "RECORD" )] = number( RECORD_PIN );
        props[wxT( "PINLENGTH" )] = number( length );
        props[wxT( "LOCATION.X" )] = number( x );
        props[wxT( "LOCATION.Y" )] = number( y );
        props[wxT( "COLOR" )] = number( aReader.U32() );
        props[wxT( "ELECTRICAL" )] = number( flags[2] );

        // Bits as Altium packs them: orientation, then 0x08 to show the name, 0x10 the number
        props[wxT( "PINCONGLOMERATE" )] = number( rotation | ( flags[4] ? 0x08 : 0 ) | ( flags[5] ? 0x10 : 0 ) );

        props[wxT( "NAME" )] = aReader.PascalString();
        props[wxT( "DESIGNATOR" )] = aReader.PascalString();

        primitive.m_Points.emplace_back( x, y );
        break;
    }

    case TYPE_POLYLINE:
    {
        props[wxT( "RECORD" )] = number( RECORD_POLYLINE );
        props[wxT( "LINEWIDTH" )] = number( aReader.U8() );
        aReader.Skip( 1 );
        props[wxT( "COLOR" )] = number( aReader.U32() );
        aReader.Skip( 1 );
        readPoints( aReader.U16() );
        break;
    }

    case TYPE_POLYGON:
    {
        props[wxT( "RECORD" )] = number( RECORD_POLYGON );
        props[wxT( "LINEWIDTH" )] = number( aReader.U8() );
        props[wxT( "COLOR" )] = number( aReader.U32() );
        props[wxT( "AREACOLOR" )] = number( aReader.U32() );
        props[wxT( "ISSOLID" )] = boolean( aReader.U8() != 0 );
        aReader.Skip( 1 );
        readPoints( aReader.U16() );
        break;
    }

    case TYPE_LINE:
    {
        int x1 = aReader.I16();
        int y1 = aReader.I16();
        int x2 = aReader.I16();
        int y2 = aReader.I16();

        props[wxT( "RECORD" )] = number( RECORD_LINE );
        props[wxT( "LOCATION.X" )] = number( x1 );
        props[wxT( "LOCATION.Y" )] = number( y1 );
        props[wxT( "CORNER.X" )] = number( x2 );
        props[wxT( "CORNER.Y" )] = number( y2 );
        props[wxT( "LINEWIDTH" )] = number( aReader.U8() );
        aReader.Skip( 1 );
        props[wxT( "COLOR" )] = number( aReader.U32() );
        aReader.Skip( 1 );

        primitive.m_Points = { { x1, y1 }, { x2, y2 } };
        break;
    }

    case TYPE_RECTANGLE:
    {
        int x1 = aReader.I16();
        int y1 = aReader.I16();
        int x2 = aReader.I16();
        int y2 = aReader.I16();

        props[wxT( "RECORD" )] = number( RECORD_RECTANGLE );
        props[wxT( "LOCATION.X" )] = number( x1 );
        props[wxT( "LOCATION.Y" )] = number( y1 );
        props[wxT( "CORNER.X" )] = number( x2 );
        props[wxT( "CORNER.Y" )] = number( y2 );
        readBorderAndFill();

        primitive.m_Points = { { x1, y1 }, { x2, y2 } };
        break;
    }

    case TYPE_ELLIPSE:
    {
        // the centre, then the x and y radii
        props[wxT( "RECORD" )] = number( RECORD_ELLIPSE );
        props[wxT( "LOCATION.X" )] = number( aReader.I16() );
        props[wxT( "LOCATION.Y" )] = number( aReader.I16() );
        props[wxT( "RADIUS" )] = number( aReader.I16() );
        props[wxT( "SECONDARYRADIUS" )] = number( aReader.I16() );
        readBorderAndFill();
        break;
    }

    case TYPE_ARC:
    {
        props[wxT( "LOCATION.X" )] = number( aReader.I16() );
        props[wxT( "LOCATION.Y" )] = number( aReader.I16() );

        // the x and y radii; a circular arc stays an arc rather than an elliptical one
        int radius = aReader.I16();
        int secondaryRadius = aReader.I16();

        props[wxT( "RECORD" )] = number( radius == secondaryRadius ? RECORD_ARC : RECORD_ELLIPTICAL_ARC );
        props[wxT( "RADIUS" )] = number( radius );
        props[wxT( "SECONDARYRADIUS" )] = number( secondaryRadius );
        props[wxT( "LINEWIDTH" )] = number( aReader.U8() );

        // Counterclockwise from the start angle to the end; equal angles go all the way round,
        // which Altium writes as 0 to 360
        double start = std::fmod( aReader.Real48(), 360.0 );
        double end = std::fmod( aReader.Real48(), 360.0 );

        start += start < 0 ? 360.0 : 0.0;
        end += end < 0 ? 360.0 : 0.0;

        if( start == end )
        {
            start = 0.0;
            end = 360.0;
        }

        props[wxT( "STARTANGLE" )] = wxString::FromCDouble( start, 3 );
        props[wxT( "ENDANGLE" )] = wxString::FromCDouble( end, 3 );
        props[wxT( "COLOR" )] = number( aReader.U32() );
        aReader.Skip( 1 );
        break;
    }

    default:
        THROW_IO_ERROR( wxString::Format( _( "Unknown primitive type 0x%02x at offset %zu in a Protel schematic "
                                             "library." ),
                                          aType, aReader.m_Pos - 1 ) );
    }

    return primitive;
}


ALTIUM_PROTEL2_SCHLIB::ALTIUM_PROTEL2_SCHLIB( const std::vector<uint8_t>& aData ) :
        m_data( aData )
{
    PRIMITIVE_READER reader{ m_data, 0 };

    if( reader.PascalString() != wxString( ALTIUM_PROTEL2_SCHLIB_MAGIC ) )
        THROW_IO_ERROR( _( "Not a Protel for Windows 1.2 - 2.0 schematic library." ) );

    // The header: a number not understood, the library's description and eleven reserved bytes
    reader.Skip( 4 );
    reader.PascalString();
    reader.Skip( 11 );

    // The fonts: each a size, six bytes not understood and the face name
    for( uint16_t fonts = reader.U16(); fonts > 0; fonts-- )
    {
        reader.Skip( 8 );
        reader.PascalString();
    }

    // The directory: the symbol names, each with a number and an offset not understood
    std::vector<wxString> names;

    uint16_t count = reader.U16();
    reader.Skip( 4 );

    for( uint16_t i = 0; i < count; i++ )
    {
        reader.Skip( 2 );
        names.push_back( reader.PascalString() );
        reader.Skip( 4 );
    }

    // Each symbol record starts with 01 00, its name and a zero byte, in the order of the directory
    std::vector<size_t> starts;
    size_t              searchFrom = reader.m_Pos;

    for( const wxString& name : names )
    {
        wxCharBuffer         latin1 = name.mb_str( wxConvISO8859_1 );
        std::vector<uint8_t> marker{ 0x01, 0x00, static_cast<uint8_t>( name.length() ) };

        marker.insert( marker.end(), latin1.data(), latin1.data() + std::strlen( latin1.data() ) );
        marker.push_back( 0x00 );

        auto it = std::search( m_data.begin() + searchFrom, m_data.end(), marker.begin(), marker.end() );

        if( it == m_data.end() )
        {
            THROW_IO_ERROR( wxString::Format( _( "Symbol '%s' is listed in the Protel schematic library but not "
                                                 "found in it." ),
                                              name ) );
        }

        starts.push_back( it - m_data.begin() );
        searchFrom = starts.back() + 1;
    }

    starts.push_back( m_data.size() );

    std::map<wxString, int> seen;

    for( size_t i = 0; i < names.size(); i++ )
    {
        readSymbol( names[i], starts[i], starts[i + 1] );

        ALTIUM_PROTEL2_SYMBOL& symbol = m_symbols.back();
        int                    occurrence = ++seen[symbol.m_Name];

        if( occurrence > 1 )
        {
            symbol.m_Name = wxString::Format( wxT( "%s (Altium duplicate %d)" ), symbol.m_Name, occurrence );
            symbol.m_Records[0][wxT( "LIBREFERENCE" )] = symbol.m_Name;
        }
    }
}


void ALTIUM_PROTEL2_SCHLIB::readSymbol( const wxString& aName, size_t aStart, size_t aEnd )
{
    PRIMITIVE_READER reader{ m_data, aStart + 2 };

    wxString name = reader.PascalString();
    reader.Skip( 1 );
    wxString description = reader.PascalString();
    wxString footprint = reader.PascalString();
    reader.Skip( 11 );
    wxString designator = reader.PascalString();
    reader.Skip( 7 );

    // The record ends at its version string, "V 1.5" after its length byte
    static const uint8_t version[] = { 'V', ' ', '1', '.', '5' };
    auto versionIt = std::search( m_data.begin() + reader.m_Pos, m_data.begin() + aEnd, std::begin( version ),
                                  std::end( version ) );

    if( versionIt == m_data.begin() + aEnd )
    {
        THROW_IO_ERROR( wxString::Format( _( "Symbol '%s' in a Protel schematic library has no end." ), aName ) );
    }

    const size_t end = ( versionIt - m_data.begin() ) - 1;

    auto atEnd = [&]( size_t aPos )
    {
        return aPos >= end
               || ( m_data[aPos] == 0x05 && aPos + 6 <= m_data.size()
                    && std::equal( std::begin( version ), std::end( version ), m_data.begin() + aPos + 1 ) );
    };

    // Primitives part by part and display mode by display mode.  One terminator ends a display
    // mode and another of the same part follows.  Two or more end a part, and the next part
    // follows four bytes on; at the very end of the record they end the symbol.
    std::vector<std::vector<std::vector<PRIMITIVE>>> parts;
    std::vector<std::vector<PRIMITIVE>>              modes;
    std::vector<PRIMITIVE>                           current;
    bool                                             ended = false;

    while( reader.m_Pos < end )
    {
        uint8_t type = reader.U8();

        if( type != TERMINATOR )
        {
            current.push_back( readPrimitive( reader, type ) );
            continue;
        }

        modes.push_back( std::move( current ) );
        current.clear();

        int terminators = 1;

        while( reader.m_Pos < end && m_data[reader.m_Pos] == TERMINATOR )
        {
            reader.m_Pos++;
            terminators++;
        }

        if( atEnd( reader.m_Pos ) )
        {
            parts.push_back( std::move( modes ) );
            ended = true;
            break;
        }

        if( terminators == 1 )
            continue;

        parts.push_back( std::move( modes ) );
        modes.clear();
        reader.Skip( 4 );
    }

    if( !ended )
    {
        modes.push_back( std::move( current ) );
        parts.push_back( std::move( modes ) );
    }

    ALTIUM_PROTEL2_SYMBOL symbol;
    symbol.m_Name = name;
    symbol.m_PartCount = static_cast<int>( parts.size() );

    for( const auto& part : parts )
        symbol.m_DisplayModeCount = std::max( symbol.m_DisplayModeCount, static_cast<int>( part.size() ) );

    std::map<wxString, wxString> component;
    component[wxT( "RECORD" )] = number( RECORD_COMPONENT );
    component[wxT( "LIBREFERENCE" )] = name;
    component[wxT( "COMPONENTDESCRIPTION" )] = description;
    component[wxT( "PARTCOUNT" )] = number( symbol.m_PartCount + 1 ); // Altium counts one more
    component[wxT( "DISPLAYMODECOUNT" )] = number( symbol.m_DisplayModeCount );
    component[wxT( "CURRENTPARTID" )] = wxT( "1" );
    symbol.m_Records.push_back( component );

    // The format stores no place for the designator and the value, so they go above and below
    // the first display mode's outline, at its left
    std::optional<int> left, top, bottom;

    for( size_t partIndex = 0; partIndex < parts.size(); partIndex++ )
    {
        for( size_t mode = 0; mode < parts[partIndex].size(); mode++ )
        {
            for( const PRIMITIVE& primitive : parts[partIndex][mode] )
            {
                std::map<wxString, wxString> record = primitive.m_Props;
                record[wxT( "OWNERPARTID" )] = number( static_cast<long>( partIndex + 1 ) );
                record[wxT( "OWNERPARTDISPLAYMODE" )] = number( static_cast<long>( mode ) );
                symbol.m_Records.push_back( record );

                if( mode != 0 )
                    continue;

                for( const auto& [x, y] : primitive.m_Points )
                {
                    left = std::min( left.value_or( x ), x );
                    top = std::max( top.value_or( y ), y );
                    bottom = std::min( bottom.value_or( y ), y );
                }
            }
        }
    }

    std::map<wxString, wxString> designatorRecord;
    designatorRecord[wxT( "RECORD" )] = number( RECORD_DESIGNATOR );
    designatorRecord[wxT( "OWNERPARTID" )] = wxT( "-1" );
    designatorRecord[wxT( "NAME" )] = wxT( "Designator" );
    designatorRecord[wxT( "TEXT" )] = designator;
    designatorRecord[wxT( "LOCATION.X" )] = number( left.value_or( 0 ) );
    designatorRecord[wxT( "LOCATION.Y" )] = number( top.value_or( 0 ) + 10 );
    symbol.m_Records.push_back( designatorRecord );

    std::map<wxString, wxString> commentRecord;
    commentRecord[wxT( "RECORD" )] = number( RECORD_PARAMETER );
    commentRecord[wxT( "OWNERPARTID" )] = wxT( "-1" );
    commentRecord[wxT( "NAME" )] = wxT( "Comment" );
    commentRecord[wxT( "TEXT" )] = name;
    commentRecord[wxT( "LOCATION.X" )] = number( left.value_or( 0 ) );
    commentRecord[wxT( "LOCATION.Y" )] = number( bottom.value_or( 0 ) - 10 );
    symbol.m_Records.push_back( commentRecord );

    if( !footprint.IsEmpty() )
    {
        std::map<wxString, wxString> implementation;
        implementation[wxT( "RECORD" )] = number( RECORD_IMPLEMENTATION );
        implementation[wxT( "MODELNAME" )] = footprint;
        implementation[wxT( "MODELTYPE" )] = wxT( "PCBLIB" );
        implementation[wxT( "ISCURRENT" )] = wxT( "T" );
        symbol.m_Records.push_back( implementation );
    }

    m_symbols.push_back( std::move( symbol ) );
}

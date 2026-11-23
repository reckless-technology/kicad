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

/**
 * @file test_altium_protel2_schlib.cpp
 * Schematic libraries in the Protel for Windows 1.2 - 2.0 binary format.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <eeschema/sch_io/altium/altium_protel2_schlib.h>
#include <eeschema/sch_io/altium/sch_io_altium.h>

#include <ki_exception.h>
#include <lib_symbol.h>
#include <sch_pin.h>
#include <sch_shape.h>

#include <wx/ffile.h>
#include <wx/filename.h>

#include <cmath>
#include <cstring>


namespace
{

/**
 * Writes a library in the format, field by field, so the test owns every byte of it.
 */
class PROTEL2_WRITER
{
public:
    void U8( uint8_t aValue ) { m_bytes.push_back( aValue ); }

    void I16( int aValue )
    {
        U8( aValue & 0xFF );
        U8( ( aValue >> 8 ) & 0xFF );
    }

    void U32( uint32_t aValue )
    {
        I16( aValue & 0xFFFF );
        I16( aValue >> 16 );
    }

    void Zeros( size_t aCount ) { m_bytes.insert( m_bytes.end(), aCount, 0 ); }

    void String( const std::string& aText )
    {
        U8( aText.size() );
        m_bytes.insert( m_bytes.end(), aText.begin(), aText.end() );
    }

    /// A Turbo Pascal real; only whole numbers are needed here
    void Real48( double aValue )
    {
        if( aValue == 0.0 )
            return Zeros( 6 );

        int    exponent;
        double fraction = std::frexp( aValue, &exponent ); // value = fraction * 2^exponent, 0.5 <= fraction < 1
        double mantissa = fraction * 2.0 - 1.0;            // value = (1 + mantissa) * 2^(exponent - 1)
        auto   bits = static_cast<uint64_t>( std::llround( mantissa * 549755813888.0 ) );

        U8( exponent - 1 + 129 );

        for( int i = 0; i < 5; i++ )
            U8( ( bits >> ( 8 * i ) ) & 0xFF );
    }

    void Header( const std::vector<std::string>& aNames )
    {
        String( ALTIUM_PROTEL2_SCHLIB_MAGIC );
        U32( 0 );
        String( "test library" );
        Zeros( 11 );
        I16( 1 ); // one font
        I16( 10 );
        Zeros( 6 );
        String( "Times New Roman" );
        I16( aNames.size() );
        Zeros( 4 );

        for( const std::string& name : aNames )
        {
            I16( 1 );
            String( name );
            U32( 0 );
        }
    }

    void SymbolStart( const std::string& aName, const std::string& aDescription, const std::string& aFootprint,
                      const std::string& aDesignator, int aParts )
    {
        I16( 1 );
        String( aName );
        U8( 0 );
        String( aDescription );
        String( aFootprint );
        Zeros( 11 );
        String( aDesignator );
        U8( 0 );
        I16( aParts );
        Zeros( 4 );
    }

    void SymbolEnd( const std::string& aName )
    {
        U8( 0xFF );
        U8( 0xFF );
        U8( 0xFF );
        String( "V 1.5" );
        String( aName );
        Zeros( 16 );
    }

    /// A pin: orientation 0 right, 1 up, 2 left, 3 down from its body end at (x, y)
    void Pin( int aX, int aY, int aLength, int aOrientation, int aElectrical, const std::string& aName,
              const std::string& aNumber )
    {
        U8( 0x02 );
        U8( 0 );
        U8( 0 );
        U8( aElectrical );
        U8( 0 );
        U8( 1 ); // show the name
        U8( 1 ); // show the number
        I16( aLength );
        I16( aX );
        I16( aY );
        U8( aOrientation );
        U32( 0 );
        String( aName );
        String( aNumber );
    }

    void Rectangle( int aX1, int aY1, int aX2, int aY2, int aWidth, bool aFilled )
    {
        U8( 0x0E );
        I16( aX1 );
        I16( aY1 );
        I16( aX2 );
        I16( aY2 );
        U8( aWidth );
        U32( 0x000080 ); // line colour
        U32( 0xB0FFFF ); // area colour
        U8( 0 );
        U8( aFilled ? 1 : 0 );
    }

    void Polyline( const std::vector<std::pair<int, int>>& aPoints, int aWidth )
    {
        U8( 0x06 );
        U8( aWidth );
        U8( 0 );
        U32( 0xFF0000 );
        U8( 0 );
        I16( aPoints.size() );

        for( const auto& [x, y] : aPoints )
        {
            I16( x );
            I16( y );
        }
    }

    void Arc( int aX, int aY, int aRadius, double aStart, double aEnd )
    {
        U8( 0x0B );
        I16( aX );
        I16( aY );
        I16( aRadius );
        I16( aRadius );
        U8( 1 );
        Real48( aStart );
        Real48( aEnd );
        U32( 0xFF0000 );
        U8( 0 );
    }

    std::vector<uint8_t> m_bytes;
};


std::vector<uint8_t> makeLibrary()
{
    PROTEL2_WRITER writer;
    writer.Header( { "BUFFER", "LINK" } );

    // two parts: a box with an input and an output pin, then a lone power pin
    writer.SymbolStart( "BUFFER", "Buffer gate", "SO14", "U?", 2 );
    writer.Rectangle( -20, -20, 20, 20, 0, true );
    writer.Pin( -20, 0, 10, 2, 0, "IN", "1" );
    writer.Pin( 20, 0, 10, 0, 2, "OUT", "2" );
    writer.U8( 0xFF );
    writer.U8( 0xFF );
    writer.Zeros( 4 );
    writer.Pin( 0, 20, 10, 1, 7, "VCC", "14" );
    writer.SymbolEnd( "BUFFER" );

    // one part, with a polyline and an arc and no designator letter
    writer.SymbolStart( "LINK", "Wire link", "", "", 1 );
    writer.Polyline( { { -10, 0 }, { 10, 0 } }, 2 );
    writer.Arc( 0, 0, 5, 90.0, 270.0 );
    writer.Pin( -10, 0, 10, 2, 4, "", "1" );
    writer.SymbolEnd( "LINK" );

    return writer.m_bytes;
}


const std::map<wxString, wxString>* recordOf( const ALTIUM_PROTEL2_SYMBOL& aSymbol, int aRecord, int aIndex = 0 )
{
    for( const std::map<wxString, wxString>& record : aSymbol.m_Records )
    {
        if( record.at( wxT( "RECORD" ) ) == wxString::Format( wxT( "%d" ), aRecord ) && aIndex-- == 0 )
            return &record;
    }

    return nullptr;
}

} // namespace


BOOST_AUTO_TEST_SUITE( AltiumProtel2SchLib )


BOOST_AUTO_TEST_CASE( Real48 )
{
    // 360 is 1.40625 * 2^8
    const uint8_t threeSixty[] = { 0x89, 0x00, 0x00, 0x00, 0x00, 0x34 };
    const uint8_t minusOne[] = { 0x81, 0x00, 0x00, 0x00, 0x00, 0x80 };
    const uint8_t zero[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    BOOST_CHECK_EQUAL( ALTIUM_PROTEL2_SCHLIB::DecodeReal48( threeSixty ), 360.0 );
    BOOST_CHECK_EQUAL( ALTIUM_PROTEL2_SCHLIB::DecodeReal48( minusOne ), -1.0 );
    BOOST_CHECK_EQUAL( ALTIUM_PROTEL2_SCHLIB::DecodeReal48( zero ), 0.0 );
}


/**
 * Each primitive becomes the record a binary Altium SchLib holds for it.
 */
BOOST_AUTO_TEST_CASE( RecordsMatchTheBinaryFormat )
{
    std::vector<uint8_t>  data = makeLibrary();
    ALTIUM_PROTEL2_SCHLIB library( data );

    BOOST_REQUIRE_EQUAL( library.GetSymbols().size(), 2 );

    const ALTIUM_PROTEL2_SYMBOL& buffer = library.GetSymbols()[0];

    BOOST_CHECK_EQUAL( buffer.m_Name, wxT( "BUFFER" ) );
    BOOST_CHECK_EQUAL( buffer.m_PartCount, 2 );
    BOOST_CHECK_EQUAL( buffer.m_DisplayModeCount, 1 );

    const std::map<wxString, wxString>& component = buffer.m_Records[0];

    BOOST_CHECK_EQUAL( component.at( wxT( "RECORD" ) ), wxT( "1" ) );
    BOOST_CHECK_EQUAL( component.at( wxT( "PARTCOUNT" ) ), wxT( "3" ) );
    BOOST_CHECK_EQUAL( component.at( wxT( "COMPONENTDESCRIPTION" ) ), wxT( "Buffer gate" ) );

    const std::map<wxString, wxString>* rectangle = recordOf( buffer, 14 );

    BOOST_REQUIRE( rectangle );
    BOOST_CHECK_EQUAL( rectangle->at( wxT( "CORNER.X" ) ), wxT( "20" ) );
    BOOST_CHECK_EQUAL( rectangle->at( wxT( "AREACOLOR" ) ), wxString::Format( wxT( "%d" ), 0xB0FFFF ) );
    BOOST_CHECK_EQUAL( rectangle->at( wxT( "ISSOLID" ) ), wxT( "T" ) );

    const std::map<wxString, wxString>* output = recordOf( buffer, 2, 1 );

    BOOST_REQUIRE( output );
    BOOST_CHECK_EQUAL( output->at( wxT( "NAME" ) ), wxT( "OUT" ) );
    BOOST_CHECK_EQUAL( output->at( wxT( "DESIGNATOR" ) ), wxT( "2" ) );
    BOOST_CHECK_EQUAL( output->at( wxT( "ELECTRICAL" ) ), wxT( "2" ) );
    BOOST_CHECK_EQUAL( output->at( wxT( "PINCONGLOMERATE" ) ), wxString::Format( wxT( "%d" ), 0 | 0x08 | 0x10 ) );

    const std::map<wxString, wxString>* power = recordOf( buffer, 2, 2 );

    BOOST_REQUIRE( power );
    BOOST_CHECK_EQUAL( power->at( wxT( "OWNERPARTID" ) ), wxT( "2" ) );

    const ALTIUM_PROTEL2_SYMBOL&        link = library.GetSymbols()[1];
    const std::map<wxString, wxString>* arc = recordOf( link, 12 );

    BOOST_REQUIRE( arc );
    BOOST_CHECK_EQUAL( arc->at( wxT( "STARTANGLE" ) ), wxT( "90.000" ) );
    BOOST_CHECK_EQUAL( arc->at( wxT( "ENDANGLE" ) ), wxT( "270.000" ) );
}


BOOST_AUTO_TEST_CASE( NotThisFormat )
{
    std::vector<uint8_t> data = makeLibrary();
    data[5] = 'X';

    BOOST_CHECK_THROW( ALTIUM_PROTEL2_SCHLIB library( data ), IO_ERROR );

    // truncated in the middle of a symbol
    data = makeLibrary();
    data.resize( data.size() / 2 );

    BOOST_CHECK_THROW( ALTIUM_PROTEL2_SCHLIB library( data ), IO_ERROR );
}


/**
 * The Altium importer reads such a library as it reads a binary SchLib.
 */
BOOST_AUTO_TEST_CASE( ImporterReadsTheLibrary )
{
    wxFileName path( wxFileName::CreateTempFileName( wxT( "protel2" ) ) );
    wxRemoveFile( path.GetFullPath() );
    path.SetExt( wxT( "SchLib" ) );

    {
        std::vector<uint8_t> data = makeLibrary();
        wxFFile              file( path.GetFullPath(), wxT( "wb" ) );

        BOOST_REQUIRE( file.IsOpened() );
        BOOST_REQUIRE( file.Write( data.data(), data.size() ) == data.size() );
    }

    SCH_IO_ALTIUM plugin;

    BOOST_CHECK( plugin.CanReadLibrary( path.GetFullPath() ) );
    BOOST_CHECK( !plugin.CanReadSchematicFile( path.GetFullPath() ) );

    wxArrayString names;
    plugin.EnumerateSymbolLib( names, path.GetFullPath() );

    std::set<wxString> nameSet( names.begin(), names.end() );
    BOOST_CHECK( nameSet == std::set<wxString>( { wxT( "BUFFER" ), wxT( "LINK" ) } ) );

    LIB_SYMBOL* buffer = plugin.LoadSymbol( path.GetFullPath(), wxT( "BUFFER" ) );

    BOOST_REQUIRE( buffer );
    BOOST_CHECK_EQUAL( buffer->GetUnitCount(), 2 );
    BOOST_CHECK_EQUAL( buffer->GetReferenceField().GetText(), wxT( "U" ) );
    BOOST_CHECK_EQUAL( buffer->GetValueField().GetText(), wxT( "BUFFER" ) );
    BOOST_CHECK_EQUAL( buffer->GetDescription(), wxT( "Buffer gate" ) );
    BOOST_CHECK_EQUAL( buffer->GetFootprintField().GetText(), wxT( "SO14" ) );

    std::map<wxString, const SCH_PIN*> pins;

    for( const SCH_PIN* pin : buffer->GetGraphicalPins() )
        pins[pin->GetNumber()] = pin;

    BOOST_REQUIRE_EQUAL( pins.size(), 3 );

    // 1 unit = 10 mil; the pin's position is its connection end, its length from there to the body
    BOOST_CHECK_EQUAL( pins[wxT( "1" )]->GetName(), wxT( "IN" ) );
    BOOST_CHECK( pins[wxT( "1" )]->GetType() == ELECTRICAL_PINTYPE::PT_INPUT );
    BOOST_CHECK_EQUAL( pins[wxT( "1" )]->GetPosition(), VECTOR2I( schIUScale.MilsToIU( -300 ), 0 ) );
    BOOST_CHECK_EQUAL( pins[wxT( "1" )]->GetLength(), schIUScale.MilsToIU( 100 ) );
    BOOST_CHECK( pins[wxT( "2" )]->GetType() == ELECTRICAL_PINTYPE::PT_OUTPUT );
    BOOST_CHECK_EQUAL( pins[wxT( "2" )]->GetPosition(), VECTOR2I( schIUScale.MilsToIU( 300 ), 0 ) );
    BOOST_CHECK_EQUAL( pins[wxT( "14" )]->GetUnit(), 2 );
    BOOST_CHECK( pins[wxT( "14" )]->GetType() == ELECTRICAL_PINTYPE::PT_POWER_IN );
    BOOST_CHECK_EQUAL( pins[wxT( "14" )]->GetPosition(), VECTOR2I( 0, schIUScale.MilsToIU( -300 ) ) );

    int rectangles = 0;

    for( const SCH_ITEM& item : buffer->GetDrawItems() )
    {
        if( item.Type() == SCH_SHAPE_T && static_cast<const SCH_SHAPE&>( item ).GetShape() == SHAPE_T::RECTANGLE )
            rectangles++;
    }

    BOOST_CHECK_EQUAL( rectangles, 1 );

    LIB_SYMBOL* link = plugin.LoadSymbol( path.GetFullPath(), wxT( "LINK" ) );

    BOOST_REQUIRE( link );

    // Altium's own default for a symbol without a designator letter
    BOOST_CHECK_EQUAL( link->GetReferenceField().GetText(), wxT( "X" ) );

    int arcs = 0;

    for( const SCH_ITEM& item : link->GetDrawItems() )
    {
        if( item.Type() == SCH_SHAPE_T && static_cast<const SCH_SHAPE&>( item ).GetShape() == SHAPE_T::ARC )
            arcs++;
    }

    BOOST_CHECK_EQUAL( arcs, 1 );

    wxRemoveFile( path.GetFullPath() );
}


BOOST_AUTO_TEST_SUITE_END()

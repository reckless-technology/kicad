/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <boost/test/unit_test.hpp>

#include <board.h>
#include <footprint.h>
#include <richio.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>


BOOST_AUTO_TEST_SUITE( FootprintModelFormat )


// A 3D model's offset or rotation that is a negative zero, as a sign flip of zero leaves it, is written as 0
// so that two files of one design do not differ in text
BOOST_AUTO_TEST_CASE( NegativeZeroIsWrittenAsZero )
{
    BOARD     board;
    FOOTPRINT footprint( &board );

    FP_3DMODEL model;
    model.m_Filename = wxS( "${KIPRJMOD}/part.step" );
    model.m_Offset = VECTOR3D( 0.0, -0.0, 0.25 );
    model.m_Rotation = VECTOR3D( -180.0, -0.0, -0.0 );
    footprint.Models().push_back( model );

    STRING_FORMATTER   formatter;
    PCB_IO_KICAD_SEXPR io;
    io.SetOutputFormatter( &formatter );
    io.Format( static_cast<const BOARD_ITEM*>( &footprint ) );

    const std::string output = formatter.GetString();

    BOOST_CHECK( output.find( "(offset (xyz 0 0 0.25))" ) != std::string::npos );
    BOOST_CHECK( output.find( "(rotate (xyz -180 0 0))" ) != std::string::npos );
    BOOST_CHECK( output.find( "-0 " ) == std::string::npos );
    BOOST_CHECK( output.find( "-0)" ) == std::string::npos );
}


BOOST_AUTO_TEST_SUITE_END()

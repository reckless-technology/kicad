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

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <board.h>
#include <footprint.h>
#include <pad.h>

struct PAD_FIXTURE
{
    PAD_FIXTURE() :
            m_board(),
            m_footprint( &m_board )
    {
    }

    PAD* MakePTH( const wxString& aNumber )
    {
        PAD* pad = new PAD( &m_footprint );

        pad->SetNumber( aNumber );
        pad->SetAttribute( PAD_ATTRIB::PTH );
        pad->SetLayerSet( PAD::UnplatedHoleMask() );

        m_footprint.Add( pad );
        return pad;
    }

    PAD* MakeNPTH()
    {
        PAD* pad = new PAD( &m_footprint );

        pad->SetAttribute( PAD_ATTRIB::NPTH );
        pad->SetLayerSet( PAD::UnplatedHoleMask() );

        m_footprint.Add( pad );
        return pad;
    }

    PAD* MakeAperture()
    {
        PAD* pad = new PAD( &m_footprint );

        pad->SetAttribute( PAD_ATTRIB::PTH );
        pad->SetLayerSet( PAD::ApertureMask() );

        m_footprint.Add( pad );
        return pad;
    }

    PAD* MakeSmd( const wxString& aNumber = wxEmptyString )
    {
        PAD* pad = new PAD( &m_footprint );

        pad->SetNumber( aNumber );
        pad->SetAttribute( PAD_ATTRIB::SMD );
        pad->SetLayerSet( PAD::SMDMask() );

        m_footprint.Add( pad );
        return pad;
    }

    BOARD     m_board;
    FOOTPRINT m_footprint;
};


BOOST_FIXTURE_TEST_SUITE( PadNumbering, PAD_FIXTURE )

/**
 * Check what gets names and what doesn't
 */
BOOST_AUTO_TEST_CASE( CanNumber )
{
    PAD* npth = MakeNPTH();
    BOOST_CHECK_EQUAL( false, npth->CanHaveNumber() );

    PAD* aperture = MakeAperture();
    BOOST_CHECK_EQUAL( false, aperture->CanHaveNumber() );

    PAD* smd = MakeSmd();
    BOOST_CHECK_EQUAL( true, smd->CanHaveNumber() );
}


BOOST_AUTO_TEST_CASE( TestPadCounting )
{
    MakeSmd( "1" );
    MakeSmd( "2" );
    MakeSmd( "33" );
    MakeSmd( "AA12" );
    MakeSmd( "BC35" );
    MakeSmd( "AAA1" );
    MakePTH( "MP" );
    MakeSmd( "MP1a" );

    BOOST_CHECK_EQUAL( m_footprint.GetNumberedPadCount(), 5 );
}


BOOST_AUTO_TEST_CASE( TestUSBcPadCounting )
{
    MakeSmd( "A1_B12" );
    MakeSmd( "A4_B9" );
    MakeSmd( "B8" );
    MakeSmd( "A5" );
    MakeSmd( "B7" );
    MakeSmd( "A6" );
    MakeSmd( "A7" );
    MakeSmd( "B6" );
    MakeSmd( "A8" );
    MakeSmd( "B5" );
    MakeSmd( "B4_A9" );
    MakeSmd( "B1_A12" );
    MakeSmd( "S1" );
    MakeSmd( "S1" );
    MakeSmd( "S1" );

    BOOST_CHECK_EQUAL( m_footprint.GetNumberedPadCount(), 17 );
}


BOOST_AUTO_TEST_SUITE_END()

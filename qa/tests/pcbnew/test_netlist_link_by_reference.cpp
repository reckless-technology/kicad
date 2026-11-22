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

/*
 * Importing a non-KiCad project gives the schematic's symbols and the board's footprints unrelated
 * ids, so the board is linked to the schematic by reference designator afterwards.
 *
 * Regression test for https://gitlab.com/kicad/code/kicad/-/issues/26132
 */

#include <netlist_reader/pcb_netlist.h>
#include <netlist_reader/netlist_reader.h>
#include <footprint.h>
#include <richio.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <deque>
#include <memory>
#include <vector>


namespace
{

// R1 and the two-unit U1 once each, R2 on two symbols
const char* IMPORTED_NETLIST =
        "(export (version \"E\")\n"
        "  (components\n"
        "    (comp (ref \"R1\")\n"
        "      (value \"10k\")\n"
        "      (property (name \"Sheetname\") (value \"Amp\"))\n"
        "      (property (name \"Sheetfile\") (value \"amp.kicad_sch\"))\n"
        "      (sheetpath (names \"/Amp/\") (tstamps \"/aaaaaaaa-1111-4aaa-8aaa-aaaaaaaaaaaa/\"))\n"
        "      (tstamps \"11111111-1111-4111-8111-111111111111\"))\n"
        "    (comp (ref \"U1\")\n"
        "      (value \"TL072\")\n"
        "      (property (name \"Sheetfile\") (value \"amp.kicad_sch\"))\n"
        "      (sheetpath (names \"/Amp/\") (tstamps \"/aaaaaaaa-1111-4aaa-8aaa-aaaaaaaaaaaa/\"))\n"
        "      (tstamps \"22222222-2222-4222-8222-222222222222\"\n"
        "               \"33333333-3333-4333-8333-333333333333\"))\n"
        "    (comp (ref \"R2\")\n"
        "      (value \"1k\")\n"
        "      (sheetpath (names \"/\") (tstamps \"/\"))\n"
        "      (tstamps \"44444444-4444-4444-8444-444444444444\"))\n"
        "    (comp (ref \"R2\")\n"
        "      (value \"1k\")\n"
        "      (sheetpath (names \"/\") (tstamps \"/\"))\n"
        "      (tstamps \"55555555-5555-4555-8555-555555555555\"))))\n";


void loadNetlist( NETLIST& aNetlist, const char* aText )
{
    try
    {
        // The netlist reader owns and deletes the line reader
        KICAD_NETLIST_READER netlistReader( new STRING_LINE_READER( aText, wxS( "test netlist" ) ),
                                            &aNetlist );
        netlistReader.LoadNetlist();
    }
    catch( const IO_ERROR& ioe )
    {
        BOOST_FAIL( "netlist parse failed: " + ioe.What().ToStdString() );
    }
}

} // namespace


BOOST_AUTO_TEST_SUITE( NetlistLinkByReference )


BOOST_AUTO_TEST_CASE( Issue26132_FootprintsLinkToSymbols )
{
    NETLIST netlist;
    loadNetlist( netlist, IMPORTED_NETLIST );

    std::vector<std::unique_ptr<FOOTPRINT>> owned;
    std::deque<FOOTPRINT*>                  footprints;

    auto addFootprint =
            [&]( const wxString& aReference )
            {
                owned.push_back( std::make_unique<FOOTPRINT>( nullptr ) );
                owned.back()->SetReference( aReference );

                // What an importer leaves: a path that names nothing in the schematic
                KIID_PATH importedPath;
                importedPath.push_back( KIID() );
                importedPath.push_back( KIID() );
                owned.back()->SetPath( importedPath );

                footprints.push_back( owned.back().get() );
                return owned.back().get();
            };

    FOOTPRINT* r1 = addFootprint( wxS( "R1" ) );
    FOOTPRINT* u1 = addFootprint( wxS( "U1" ) );
    FOOTPRINT* r2 = addFootprint( wxS( "R2" ) );
    FOOTPRINT* j1 = addFootprint( wxS( "J1" ) );
    FOOTPRINT* c1a = addFootprint( wxS( "C1" ) );
    FOOTPRINT* c1b = addFootprint( wxS( "C1" ) );

    const KIID_PATH r2Before = r2->GetPath();
    const KIID_PATH j1Before = j1->GetPath();
    const KIID_PATH c1Before = c1a->GetPath();

    BOOST_CHECK_EQUAL( netlist.LinkFootprintsByReference( footprints ), 2 );

    const wxString sheet = wxS( "/aaaaaaaa-1111-4aaa-8aaa-aaaaaaaaaaaa" );

    BOOST_CHECK_EQUAL( r1->GetPath().AsString(),
                       sheet + wxS( "/11111111-1111-4111-8111-111111111111" ) );
    BOOST_CHECK_EQUAL( r1->GetSheetname(), wxS( "/Amp/" ) );
    BOOST_CHECK_EQUAL( r1->GetSheetfile(), wxS( "amp.kicad_sch" ) );

    // A multi-unit symbol links to its first unit, as "Update PCB from Schematic" links it
    BOOST_CHECK_EQUAL( u1->GetPath().AsString(),
                       sheet + wxS( "/22222222-2222-4222-8222-222222222222" ) );

    // A reference on two symbols, on no symbol or on two footprints names nothing for certain
    BOOST_CHECK( r2->GetPath() == r2Before );
    BOOST_CHECK( j1->GetPath() == j1Before );
    BOOST_CHECK( c1a->GetPath() == c1Before );
    BOOST_CHECK( c1b->GetPath() != r1->GetPath() );
}


BOOST_AUTO_TEST_SUITE_END()

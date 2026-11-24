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

/**
 * @file test_pad_rectangular_hole.cpp
 * A pad's rectangular (and square) hole: the file format, its shape for DRC and the zone filler,
 * and the routed path the drill files write for it.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <qa_utils/file_utils.h>
#include <pcbnew_utils/board_test_utils.h>

#include <base_units.h>
#include <board.h>
#include <board_design_settings.h>
#include <drc/drc_engine.h>
#include <drc/drc_item.h>
#include <footprint.h>
#include <netinfo.h>
#include <geometry/shape_simple.h>
#include <pad.h>
#include <pcb_track.h>
#include <richio.h>
#include <settings/settings_manager.h>
#include <zone.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr.h>
#include <pcb_io/kicad_sexpr/pcb_io_kicad_sexpr_parser.h>
#include <pcbnew/exporters/gendrill_excellon_writer.h>
#include <pcbnew/exporters/gendrill_gerber_writer.h>
#include <pcbnew/pcb_io/odbpp/pcb_io_odbpp.h>

#include <set>

#include <wx/ffile.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/tokenzr.h>


namespace
{
int mm( double aValue )
{
    return pcbIUScale.mmToIU( aValue );
}


int netCode( BOARD& aBoard, const wxString& aNetName )
{
    NETINFO_ITEM* net = aBoard.FindNet( aNetName );

    if( !net )
    {
        net = new NETINFO_ITEM( &aBoard, aNetName );
        aBoard.Add( net );
    }

    return net->GetNetCode();
}


PAD* addHolePad( BOARD& aBoard, const VECTOR2I& aPos, PAD_DRILL_SHAPE aDrillShape, const VECTOR2I& aDrill,
                 PAD_ATTRIB aAttrib, PAD_SHAPE aPadShape, const VECTOR2I& aPadSize, int aNetCode = 0 )
{
    FOOTPRINT* fp = new FOOTPRINT( &aBoard );
    fp->SetPosition( aPos );
    fp->SetReference( wxString::Format( wxT( "H%zu" ), aBoard.Footprints().size() + 1 ) );

    PAD* pad = new PAD( fp );
    pad->SetPadstackMode( PADSTACK::MODE::NORMAL );
    pad->SetPosition( aPos );
    pad->SetNumber( wxT( "1" ) );
    pad->SetAttribute( aAttrib );
    pad->SetShape( PADSTACK::ALL_LAYERS, aPadShape );
    pad->SetSize( PADSTACK::ALL_LAYERS, aPadSize );
    pad->SetDrillShape( aDrillShape );
    pad->SetDrillSize( aDrill );
    pad->SetLayerSet( aAttrib == PAD_ATTRIB::NPTH ? PAD::UnplatedHoleMask() : PAD::PTHMask() );
    fp->Add( pad );

    aBoard.Add( fp );
    pad->SetNetCode( aNetCode );
    return pad;
}


wxString readFile( const wxString& aDir, const wxString& aName )
{
    wxFileName file( aDir, aName );
    BOOST_REQUIRE_MESSAGE( file.FileExists(), "missing " << aName );

    wxFFile  stream( file.GetFullPath(), wxT( "rb" ) );
    wxString contents;
    BOOST_REQUIRE( stream.ReadAll( &contents ) );
    return contents;
}


struct RECT_HOLE_DRC_FIXTURE
{
    RECT_HOLE_DRC_FIXTURE()
    {
        m_board = std::make_unique<BOARD>();
        m_board->BuildConnectivity();

        BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();
        auto                   drcEngine = std::make_shared<DRC_ENGINE>( m_board.get(), &bds );
        drcEngine->InitEngine( wxFileName() );
        bds.m_DRCEngine = drcEngine;
    }

    int countViolations( int aErrorCode )
    {
        int                    count = 0;
        BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();

        m_board->BuildConnectivity();
        bds.m_DRCEngine->InitEngine( wxFileName() );
        bds.m_DRCEngine->SetViolationHandler(
                [&]( const std::shared_ptr<DRC_ITEM>& aItem, const VECTOR2I& aPos [[maybe_unused]],
                     int                                       aLayer [[maybe_unused]],
                     const std::function<void( PCB_MARKER* )>& aPathGenerator [[maybe_unused]] )
                {
                    if( aItem->GetErrorCode() == aErrorCode )
                        count++;
                } );

        bds.m_DRCEngine->RunTests( EDA_UNITS::MM, true, false );
        bds.m_DRCEngine->ClearViolationHandler();

        return count;
    }

    std::unique_ptr<BOARD> m_board;
};

} // namespace


BOOST_AUTO_TEST_SUITE( PadRectangularHole )


// A square hole is written "(drill rect <side>)" and a rectangle "(drill rect <x> <y>)", and both
// read back as they were; an oval beside them is unchanged
BOOST_AUTO_TEST_CASE( FormatRoundTrip )
{
    BOARD     board;
    FOOTPRINT footprint( &board );

    auto addPad = [&]( const wxString& aNumber, PAD_DRILL_SHAPE aShape, const VECTOR2I& aDrill )
    {
        PAD* pad = new PAD( &footprint );
        pad->SetNumber( aNumber );
        pad->SetAttribute( PAD_ATTRIB::PTH );
        pad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE );
        pad->SetSize( PADSTACK::ALL_LAYERS, VECTOR2I( mm( 3 ), mm( 3 ) ) );
        pad->SetDrillShape( aShape );
        pad->SetDrillSize( aDrill );
        pad->SetLayerSet( PAD::PTHMask() );
        footprint.Add( pad );
    };

    addPad( wxT( "1" ), PAD_DRILL_SHAPE::RECTANGLE, VECTOR2I( mm( 1.2 ), mm( 1.2 ) ) );
    addPad( wxT( "2" ), PAD_DRILL_SHAPE::RECTANGLE, VECTOR2I( mm( 1.6 ), mm( 0.8 ) ) );
    addPad( wxT( "3" ), PAD_DRILL_SHAPE::OBLONG, VECTOR2I( mm( 1.6 ), mm( 0.8 ) ) );

    STRING_FORMATTER   formatter;
    PCB_IO_KICAD_SEXPR io;
    io.SetOutputFormatter( &formatter );
    io.Format( static_cast<const BOARD_ITEM*>( &footprint ) );

    const std::string output = formatter.GetString();

    BOOST_CHECK( output.find( "(drill rect 1.2)" ) != std::string::npos );
    BOOST_CHECK( output.find( "(drill rect 1.6 0.8)" ) != std::string::npos );
    BOOST_CHECK( output.find( "(drill oval 1.6 0.8)" ) != std::string::npos );

    STRING_LINE_READER        reader( output, wxT( "rectangular hole round trip" ) );
    PCB_IO_KICAD_SEXPR_PARSER parser( &reader, nullptr, nullptr );

    std::unique_ptr<BOARD_ITEM> parsed( parser.Parse() );
    FOOTPRINT*                  readBack = dynamic_cast<FOOTPRINT*>( parsed.get() );

    BOOST_REQUIRE( readBack );
    BOOST_REQUIRE_EQUAL( readBack->Pads().size(), 3u );

    for( const PAD* expected : footprint.Pads() )
    {
        const PAD* pad = readBack->FindPadByNumber( expected->GetNumber() );

        BOOST_REQUIRE( pad );
        BOOST_CHECK( pad->GetDrillShape() == expected->GetDrillShape() );
        BOOST_CHECK_EQUAL( pad->GetDrillSize(), expected->GetDrillSize() );
    }
}


// The hole is the rectangle itself, turned with the pad, not the oval or circle inside it
BOOST_AUTO_TEST_CASE( EffectiveHoleShapeIsTheRectangle )
{
    BOARD board;
    PAD*  pad = addHolePad( board, VECTOR2I( 0, 0 ), PAD_DRILL_SHAPE::RECTANGLE, VECTOR2I( mm( 2 ), mm( 1 ) ),
                            PAD_ATTRIB::NPTH, PAD_SHAPE::RECTANGLE, VECTOR2I( mm( 2 ), mm( 1 ) ) );

    std::shared_ptr<SHAPE> hole = pad->GetEffectiveHoleShape();

    BOOST_REQUIRE( hole );
    BOOST_REQUIRE( hole->Type() == SH_SIMPLE );
    BOOST_CHECK_EQUAL( static_cast<SHAPE_SIMPLE*>( hole.get() )->PointCount(), 4 );

    // Near the corner: inside the rectangle, outside the 2 x 1 oval
    const VECTOR2I nearCorner( mm( 0.95 ), mm( 0.45 ) );
    BOOST_CHECK( hole->Collide( nearCorner ) );

    pad->SetDrillShape( PAD_DRILL_SHAPE::OBLONG );
    pad->SetDrillSize( VECTOR2I( mm( 2 ), mm( 1 ) ) );
    BOOST_CHECK( !pad->GetEffectiveHoleShape()->Collide( nearCorner ) );

    // Turned a quarter, the long side runs along Y
    pad->SetDrillShape( PAD_DRILL_SHAPE::RECTANGLE );
    pad->SetDrillSize( VECTOR2I( mm( 2 ), mm( 1 ) ) );
    pad->SetOrientation( ANGLE_90 );
    BOOST_CHECK( !pad->GetEffectiveHoleShape()->Collide( nearCorner ) );
    BOOST_CHECK( pad->GetEffectiveHoleShape()->Collide( VECTOR2I( mm( 0.45 ), mm( 0.95 ) ) ) );

    // The clearance outline is the rectangle grown by the clearance, its corners rounded to it
    SHAPE_POLY_SET outline;
    pad->TransformHoleToPolygon( outline, mm( 0.1 ), mm( 0.001 ), ERROR_OUTSIDE );

    const double expectedArea = 2.2 * 1.2 - ( 4.0 - M_PI ) * 0.1 * 0.1;
    BOOST_CHECK_CLOSE( outline.Area() / ( (double) mm( 1 ) * mm( 1 ) ), expectedArea, 0.1 );
}


// Two 1 mm holes 1.5 mm apart along each axis: as circles their web is 1.12 mm, as squares their
// facing corners are 0.71 mm apart, under a 0.9 mm minimum
BOOST_FIXTURE_TEST_CASE( HoleToHoleUsesTheSquare, RECT_HOLE_DRC_FIXTURE )
{
    m_board->GetDesignSettings().m_HoleToHoleMin = mm( 0.9 );

    const VECTOR2I drill( mm( 1 ), mm( 1 ) );

    PAD* a = addHolePad( *m_board, VECTOR2I( mm( 10 ), mm( 10 ) ), PAD_DRILL_SHAPE::RECTANGLE, drill, PAD_ATTRIB::NPTH,
                         PAD_SHAPE::RECTANGLE, drill );
    PAD* b = addHolePad( *m_board, VECTOR2I( mm( 11.5 ), mm( 11.5 ) ), PAD_DRILL_SHAPE::RECTANGLE, drill,
                         PAD_ATTRIB::NPTH, PAD_SHAPE::RECTANGLE, drill );

    BOOST_CHECK_EQUAL( countViolations( DRCE_DRILLED_HOLES_TOO_CLOSE ), 1 );

    for( PAD* pad : { a, b } )
    {
        pad->SetDrillShape( PAD_DRILL_SHAPE::CIRCLE );
        pad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::CIRCLE );
    }

    BOOST_CHECK_EQUAL( countViolations( DRCE_DRILLED_HOLES_TOO_CLOSE ), 0 );
}


// A track along a diagonal passes 0.32 mm from a 1 mm square hole's corner, but 0.53 mm from a
// 1 mm round hole, against a 0.4 mm hole clearance
BOOST_FIXTURE_TEST_CASE( HoleClearanceUsesTheSquare, RECT_HOLE_DRC_FIXTURE )
{
    m_board->GetDesignSettings().m_HoleClearance = mm( 0.4 );
    m_board->GetDesignSettings().m_MinClearance = 0;

    const VECTOR2I centre( mm( 10 ), mm( 10 ) );
    const VECTOR2I drill( mm( 1 ), mm( 1 ) );

    PAD* pad = addHolePad( *m_board, centre, PAD_DRILL_SHAPE::RECTANGLE, drill, PAD_ATTRIB::NPTH, PAD_SHAPE::RECTANGLE,
                           drill );

    // The line x + y = 1.6 mm about the hole's center
    PCB_TRACK* track = new PCB_TRACK( m_board.get() );
    track->SetStart( centre + VECTOR2I( mm( -1.4 ), mm( 3.0 ) ) );
    track->SetEnd( centre + VECTOR2I( mm( 3.0 ), mm( -1.4 ) ) );
    track->SetWidth( mm( 0.2 ) );
    track->SetLayer( F_Cu );
    m_board->Add( track );

    // Items of one net are not tested against each other
    track->SetNetCode( netCode( *m_board, wxT( "TRACK" ) ) );

    BOOST_CHECK_EQUAL( countViolations( DRCE_HOLE_CLEARANCE ), 1 );

    pad->SetDrillShape( PAD_DRILL_SHAPE::CIRCLE );
    pad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::CIRCLE );

    BOOST_CHECK_EQUAL( countViolations( DRCE_HOLE_CLEARANCE ), 0 );
}


// A 1 mm square hole in a 2 mm round pad leaves 0.29 mm of copper at its corners, where a round
// hole leaves 0.5 mm; in a 2 mm square pad the square hole leaves 0.5 mm all round
BOOST_FIXTURE_TEST_CASE( AnnularWidthUsesTheSquare, RECT_HOLE_DRC_FIXTURE )
{
    m_board->GetDesignSettings().m_ViasMinAnnularWidth = mm( 0.4 );

    const VECTOR2I drill( mm( 1 ), mm( 1 ) );
    const VECTOR2I padSize( mm( 2 ), mm( 2 ) );

    PAD* pad = addHolePad( *m_board, VECTOR2I( mm( 10 ), mm( 10 ) ), PAD_DRILL_SHAPE::RECTANGLE, drill, PAD_ATTRIB::PTH,
                           PAD_SHAPE::CIRCLE, padSize, netCode( *m_board, wxT( "PAD" ) ) );

    BOOST_CHECK_EQUAL( countViolations( DRCE_ANNULAR_WIDTH ), 1 );

    pad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE );
    BOOST_CHECK_EQUAL( countViolations( DRCE_ANNULAR_WIDTH ), 0 );

    pad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::CIRCLE );
    pad->SetDrillShape( PAD_DRILL_SHAPE::CIRCLE );
    BOOST_CHECK_EQUAL( countViolations( DRCE_ANNULAR_WIDTH ), 0 );
}


// The zone filler keeps its standoff from the square's corners, not from the circle inside it.
// The issue 24799 board has a round 3.2 mm NPTH hole in a zone with a 0.25 mm hole clearance.
BOOST_AUTO_TEST_CASE( ZoneFillClearsTheCorners )
{
    SETTINGS_MANAGER       settingsManager;
    std::unique_ptr<BOARD> board;

    KI_TEST::LoadBoard( settingsManager, "issue24799/issue24799", board );
    BOOST_REQUIRE( board );

    PAD* roundPad = nullptr;

    for( FOOTPRINT* footprint : board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
        {
            if( pad->GetAttribute() == PAD_ATTRIB::NPTH && pad->HasHole()
                && pad->GetDrillSize().x == pad->GetDrillSize().y )
            {
                roundPad = pad;
            }
        }
    }

    BOOST_REQUIRE( roundPad );

    roundPad->SetDrillShape( PAD_DRILL_SHAPE::RECTANGLE );
    roundPad->SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE );
    roundPad->SetSize( PADSTACK::ALL_LAYERS, roundPad->GetDrillSize() );

    KI_TEST::FillZones( board.get() );

    BOOST_REQUIRE_EQUAL( board->Zones().size(), 1 );

    const std::shared_ptr<SHAPE_POLY_SET>& fill = board->Zones().front()->GetFilledPolysList( F_Cu );
    BOOST_REQUIRE( fill && fill->OutlineCount() > 0 );

    int actual = 0;
    BOOST_REQUIRE( fill->Collide( roundPad->GetEffectiveHoleShape().get(), mm( 2 ), &actual ) );

    BOOST_TEST_MESSAGE( "Fill standoff from the square hole: " << pcbIUScale.IUTomm( actual ) << " mm" );
    BOOST_CHECK_GE( actual, mm( 0.2 ) );
    BOOST_CHECK_LE( actual, mm( 0.5 ) );

    board->SetProject( nullptr );
}


// Excellon has no square hole, so the drill file routes one: the bit (half the shorter side)
// goes once round the hole's outline, inset by its radius. Plated and unplated holes keep to their
// own files, and the Gerber drill file draws the same path with the bit as its aperture.
BOOST_AUTO_TEST_CASE( DrillFilesRouteTheRectangle )
{
    KI_TEST::SCOPED_TEMP_DIR tempDir( wxT( "kicad_rectangular_hole_drill" ) );
    wxFileName               boardFile( tempDir.PathStr(), wxT( "square_hole.kicad_pcb" ) );

    BOARD board;
    board.SetFileName( boardFile.GetFullPath() );

    // A plated 2 mm square at (10, 10) and an unplated 3 x 2 mm rectangle at (20, 10)
    addHolePad( board, VECTOR2I( mm( 10 ), mm( 10 ) ), PAD_DRILL_SHAPE::RECTANGLE, VECTOR2I( mm( 2 ), mm( 2 ) ),
                PAD_ATTRIB::PTH, PAD_SHAPE::RECTANGLE, VECTOR2I( mm( 3 ), mm( 3 ) ), netCode( board, wxT( "PAD" ) ) );
    addHolePad( board, VECTOR2I( mm( 20 ), mm( 10 ) ), PAD_DRILL_SHAPE::RECTANGLE, VECTOR2I( mm( 3 ), mm( 2 ) ),
                PAD_ATTRIB::NPTH, PAD_SHAPE::RECTANGLE, VECTOR2I( mm( 3 ), mm( 2 ) ) );

    EXCELLON_WRITER excellon( &board );
    excellon.SetOptions( false, false, VECTOR2I( 0, 0 ), false );
    excellon.SetFormat( true );
    BOOST_REQUIRE( excellon.CreateDrillandMapFilesSet( tempDir.PathStr(), true, false, nullptr ) );

    const wxString pth = readFile( tempDir.PathStr(), wxT( "square_hole-PTH.drl" ) );

    BOOST_TEST_MESSAGE( "PTH drill file:\n" << pth );
    BOOST_CHECK( pth.Contains( wxT( "T1C1.000\n" ) ) );
    BOOST_CHECK( pth.Contains( wxT( "G00X9.5Y-9.5\nM15\n"
                                    "G01X10.5Y-9.5\n"
                                    "G01X10.5Y-10.5\n"
                                    "G01X9.5Y-10.5\n"
                                    "G01X9.5Y-9.5\n"
                                    "M16\nG05\n" ) ) );
    BOOST_CHECK( !pth.Contains( wxT( "X20." ) ) );

    const wxString npth = readFile( tempDir.PathStr(), wxT( "square_hole-NPTH.drl" ) );

    BOOST_TEST_MESSAGE( "NPTH drill file:\n" << npth );
    BOOST_CHECK( npth.Contains( wxT( "T1C1.000\n" ) ) );
    BOOST_CHECK( npth.Contains( wxT( "G00X19.0Y-9.5\nM15\n"
                                     "G01X21.0Y-9.5\n"
                                     "G01X21.0Y-10.5\n"
                                     "G01X19.0Y-10.5\n"
                                     "G01X19.0Y-9.5\n"
                                     "M16\nG05\n" ) ) );
    BOOST_CHECK( !npth.Contains( wxT( "X10." ) ) );

    GERBER_WRITER gerber( &board );
    gerber.SetOptions( VECTOR2I( 0, 0 ) );
    gerber.SetFormat( 6 );
    BOOST_REQUIRE( gerber.CreateDrillandMapFilesSet( tempDir.PathStr(), true, false, false, nullptr ) );

    const wxString gerberPth = readFile( tempDir.PathStr(), wxT( "square_hole-PTH-drl.gbr" ) );

    BOOST_TEST_MESSAGE( "PTH Gerber drill file:\n" << gerberPth );
    BOOST_CHECK( gerberPth.Contains( wxT( "%ADD10C,1.000000*%" ) ) );
    BOOST_CHECK( gerberPth.Contains( wxT( "X9500000Y-9500000D02*\n"
                                          "X10500000Y-9500000D01*\n"
                                          "X10500000Y-10500000D01*\n"
                                          "X9500000Y-10500000D01*\n"
                                          "X9500000Y-9500000D01*\n" ) ) );
    BOOST_CHECK( gerberPth.Contains( wxT( ",Rout" ) ) );
}


// An unplated rectangular hole with a pad no bigger than itself has no copper, so the solder mask
// test does not see copper there; nor does it for the round hole beside it. The board has SMD pads
// 0.6 mm from a 2 mm square NPTH hole and from a 2 mm round one, each pair on two nets.
BOOST_AUTO_TEST_CASE( UnplatedRectangularHoleMakesNoMaskBridge )
{
    SETTINGS_MANAGER       settingsManager;
    std::unique_ptr<BOARD> board;

    KI_TEST::LoadBoard( settingsManager, "rectangular_holes/mask_bridge", board );
    BOOST_REQUIRE( board );

    for( FOOTPRINT* footprint : board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
        {
            if( pad->GetAttribute() == PAD_ATTRIB::NPTH && pad->GetSize( F_Cu ) == pad->GetDrillSize() )
            {
                BOOST_TEST_CONTEXT( footprint->GetReference() )
                {
                    BOOST_CHECK( pad->IsNPTHWithNoCopper() );
                    BOOST_CHECK( !pad->FlashLayer( F_Cu ) );
                }
            }
        }
    }

    BOARD_DESIGN_SETTINGS& bds = board->GetDesignSettings();
    BOOST_REQUIRE( bds.m_DRCEngine );

    int bridges = 0;

    bds.m_DRCEngine->SetViolationHandler(
            [&]( const std::shared_ptr<DRC_ITEM>& aItem, const VECTOR2I& aPos [[maybe_unused]],
                 int                                       aLayer [[maybe_unused]],
                 const std::function<void( PCB_MARKER* )>& aPathGenerator [[maybe_unused]] )
            {
                if( aItem->GetErrorCode() == DRCE_SOLDERMASK_BRIDGE )
                {
                    BOOST_TEST_MESSAGE( "Bridge: " << aItem->GetErrorMessage( false ) );
                    bridges++;
                }
            } );

    bds.m_DRCEngine->RunTests( EDA_UNITS::MM, true, false );
    bds.m_DRCEngine->ClearViolationHandler();

    BOOST_CHECK_EQUAL( bridges, 0 );

    board->SetProject( nullptr );
}


// A routed hole is not a drilled one, even when it is square, so the drilled-hole checks (the
// router's hole to hole, the footprint checker's, press-fit) leave it alone as they do a slot
BOOST_AUTO_TEST_CASE( RectangularHoleIsNotDrilled )
{
    BOARD board;
    PAD*  pad = addHolePad( board, VECTOR2I( 0, 0 ), PAD_DRILL_SHAPE::RECTANGLE, VECTOR2I( mm( 1 ), mm( 1 ) ),
                            PAD_ATTRIB::PTH, PAD_SHAPE::RECTANGLE, VECTOR2I( mm( 2 ), mm( 2 ) ) );

    BOOST_CHECK( pad->HasHole() );
    BOOST_CHECK( !pad->HasDrilledHole() );

    pad->SetDrillShape( PAD_DRILL_SHAPE::CIRCLE );
    BOOST_CHECK( pad->HasDrilledHole() );
}


// A backdrill wider than a rectangular hole's shorter side reaches outside it, so the hole used for
// hole-to-hole takes in its circle; one no wider than the shorter side stays inside the rectangle
BOOST_AUTO_TEST_CASE( BackdrillOutsideTheRectangleIsAdded )
{
    BOARD board;
    PAD*  pad = addHolePad( board, VECTOR2I( 0, 0 ), PAD_DRILL_SHAPE::RECTANGLE, VECTOR2I( mm( 2 ), mm( 1 ) ),
                            PAD_ATTRIB::PTH, PAD_SHAPE::RECTANGLE, VECTOR2I( mm( 3 ), mm( 2 ) ) );

    // On the X axis just past the short side: inside a 1.5 mm circle, outside the 2 x 1 rectangle's
    // top edge
    const VECTOR2I abovePad( 0, mm( 0.6 ) );

    pad->SetSecondaryDrillSize( VECTOR2I( mm( 1.5 ), mm( 1.5 ) ) );
    pad->SetSecondaryDrillStartLayer( F_Cu );
    pad->SetSecondaryDrillEndLayer( In1_Cu );

    std::shared_ptr<SHAPE> hole = pad->GetEffectiveHoleShape( UNDEFINED_LAYER, HOLE_TO_HOLE_CONSTRAINT );
    BOOST_CHECK( hole->Collide( abovePad ) );
    BOOST_CHECK( hole->Collide( VECTOR2I( mm( 0.95 ), mm( 0.45 ) ) ) );

    pad->SetSecondaryDrillSize( VECTOR2I( mm( 0.8 ), mm( 0.8 ) ) );
    hole = pad->GetEffectiveHoleShape( UNDEFINED_LAYER, HOLE_TO_HOLE_CONSTRAINT );
    BOOST_CHECK( hole->Type() == SH_SIMPLE );
    BOOST_CHECK( !hole->Collide( abovePad ) );
}


// ODB++ drill layers hold round hits and straight slots, whose symbols each need a tool. A
// rectangular hole is neither, so it is left off them and goes to the rout layer as its outline.
BOOST_AUTO_TEST_CASE( OdbDrillLayersAgreeWithTheirTools )
{
    std::unique_ptr<BOARD> board =
            KI_TEST::ReadBoardFromFile( KI_TEST::GetPcbnewTestDataDir() + "rectangular_holes/fab.kicad_pcb" );
    BOOST_REQUIRE( board );

    KI_TEST::SCOPED_TEMP_DIR tempDir( wxT( "kicad_rectangular_hole_odb" ) );
    wxFileName               odbRoot( tempDir.PathStr(), wxEmptyString );
    odbRoot.AppendDir( wxT( "odb" ) );
    BOOST_REQUIRE( odbRoot.Mkdir( wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL ) );

    PCB_IO_ODBPP                odb;
    std::map<std::string, UTF8> props = { { "units", "mm" }, { "sigfig", "4" } };
    BOOST_REQUIRE_NO_THROW( odb.SaveBoard( odbRoot.GetFullPath(), *board, &props ) );

    wxFileName layersDir( odbRoot.GetFullPath(), wxEmptyString );
    layersDir.AppendDir( wxT( "steps" ) );
    layersDir.AppendDir( wxT( "pcb" ) );
    layersDir.AppendDir( wxT( "layers" ) );

    wxDir    dir( layersDir.GetFullPath() );
    wxString layer;
    int      drillLayers = 0;
    int      drillFeatures = 0;

    BOOST_REQUIRE( dir.IsOpened() );

    for( bool more = dir.GetFirst( &layer, wxEmptyString, wxDIR_DIRS ); more; more = dir.GetNext( &layer ) )
    {
        if( !layer.StartsWith( wxT( "drill" ) ) )
            continue;

        drillLayers++;

        wxFileName layerDir( layersDir.GetFullPath(), wxEmptyString );
        layerDir.AppendDir( layer );

        std::set<wxString> tools;
        wxStringTokenizer  toolLines( readFile( layerDir.GetFullPath(), wxT( "tools" ) ), wxT( "\n" ) );

        while( toolLines.HasMoreTokens() )
        {
            wxString line = toolLines.GetNextToken().Trim().Trim( false );

            if( line.StartsWith( wxT( "DRILL_SIZE=" ) ) )
                tools.insert( line.AfterFirst( '=' ) );
        }

        wxStringTokenizer featureLines( readFile( layerDir.GetFullPath(), wxT( "features" ) ), wxT( "\n" ) );

        while( featureLines.HasMoreTokens() )
        {
            wxString line = featureLines.GetNextToken().Trim().Trim( false );

            // Symbols are "$<n> r<diameter in um>"
            if( line.StartsWith( wxT( "$" ) ) )
            {
                wxString symbol = line.AfterFirst( ' ' );

                BOOST_TEST_CONTEXT( layer << " symbol " << symbol )
                {
                    BOOST_CHECK( symbol.StartsWith( wxT( "r" ) ) );
                    BOOST_CHECK_MESSAGE( tools.count( symbol.Mid( 1 ) ), "no tool for " << symbol );
                }
            }
            else if( line.StartsWith( wxT( "L " ) ) || line.StartsWith( wxT( "P " ) ) )
            {
                drillFeatures++;
            }
        }
    }

    // The plated and the unplated drill layer; on them only the round hole and the oval slot
    BOOST_CHECK_EQUAL( drillLayers, 2 );
    BOOST_CHECK_EQUAL( drillFeatures, 2 );

    // The 2 mm square at (20, 20) is on the rout layer as its outline
    wxFileName routDir( layersDir.GetFullPath(), wxEmptyString );
    routDir.AppendDir( wxT( "rout" ) );

    const wxString rout = readFile( routDir.GetFullPath(), wxT( "features" ) );
    BOOST_TEST_MESSAGE( "Rout features:\n" << rout );
    BOOST_CHECK( rout.Contains( wxT( "L 19.0 -19.0 21.0 -19.0" ) ) );
}


BOOST_AUTO_TEST_SUITE_END()

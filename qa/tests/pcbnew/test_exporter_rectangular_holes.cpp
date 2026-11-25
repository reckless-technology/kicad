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
 * @file test_exporter_rectangular_holes.cpp
 * A pad's rectangular hole in the exporters that are not fabrication outputs: Specctra DSN and IDF
 * write the rectangle, HyperLynx and GenCAD, which have only a round drill, report it.  VRML is
 * checked by the qa_cli test that exports it, since its exporter needs a project.
 *
 * rectangular_holes/fab.kicad_pcb holds four rectangular holes (a plated 2 mm square, an unplated
 * 3 x 2 mm rectangle and two plated 2.5 x 1.5 mm rectangles turned 30 and 90 degrees), a round
 * hole and an oval slot.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <qa_utils/file_utils.h>
#include <pcbnew_utils/board_file_utils.h>

#include <base_units.h>
#include <board.h>
#include <board_design_settings.h>
#include <exporters/export_gencad_writer.h>
#include <exporters/export_hyperlynx.h>
#include <exporters/export_idf.h>
#include <filename_resolver.h>
#include <footprint.h>
#include <geometry/shape_simple.h>
#include <jobs/job_export_pcb_idf.h>
#include <locale_io.h>
#include <pad.h>
#include <reporter.h>
#include <specctra_import_export/specctra.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>


namespace
{
const char* const FAB_BOARD = "rectangular_holes/fab.kicad_pcb";

/// The number of rectangular holes in FAB_BOARD.
const int FAB_RECTANGULAR_HOLE_COUNT = 4;

/// The number of oval slots in FAB_BOARD.
const int FAB_SLOT_COUNT = 1;


std::string slurp( const std::filesystem::path& aPath )
{
    std::ifstream in( aPath );
    BOOST_REQUIRE_MESSAGE( in, "cannot open " << aPath.string() );

    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}


PAD* padOf( BOARD& aBoard, const wxString& aReference )
{
    FOOTPRINT* footprint = aBoard.FindFootprintByReference( aReference );
    BOOST_REQUIRE_MESSAGE( footprint && footprint->Pads().size() == 1, "no single-pad footprint " << aReference );
    return footprint->Pads().front();
}


int countOccurrences( const wxString& aText, const wxString& aNeedle )
{
    int    count = 0;
    size_t pos = aText.find( aNeedle );

    while( pos != wxString::npos )
    {
        count++;
        pos = aText.find( aNeedle, pos + aNeedle.length() );
    }

    return count;
}


std::vector<double> numbersIn( const std::string& aText )
{
    std::vector<double> numbers;
    std::istringstream  in( aText );

    for( double value; in >> value; )
        numbers.push_back( value );

    return numbers;
}


/// One keepout of a Specctra image: its shape, layer and coordinates.
struct DSN_KEEPOUT
{
    std::string         m_shape;
    std::string         m_layer;
    std::vector<double> m_numbers;
};


/// The keepouts of the image @p aImageId in Specctra DSN text, and whether the image has a pin.
std::vector<DSN_KEEPOUT> imageKeepouts( const std::string& aDsn, const std::string& aImageId, bool& aHasPin )
{
    size_t start = aDsn.find( "(image " + aImageId );

    if( start == std::string::npos )
        start = aDsn.find( "(image \"" + aImageId + "\"" );

    BOOST_REQUIRE_MESSAGE( start != std::string::npos, "no image " << aImageId );

    size_t      end = aDsn.find( "(image ", start + 1 );
    std::string image = aDsn.substr( start, end == std::string::npos ? std::string::npos : end - start );

    aHasPin = image.find( "(pin " ) != std::string::npos;

    std::vector<DSN_KEEPOUT> keepouts;
    std::regex               keepout( R"RX(\(keepout "" \((rect|polygon) "?([^" ]+)"? ([^)]*)\))RX" );

    for( auto it = std::sregex_iterator( image.begin(), image.end(), keepout ); it != std::sregex_iterator(); ++it )
        keepouts.push_back( { ( *it )[1].str(), ( *it )[2].str(), numbersIn( ( *it )[3].str() ) } );

    return keepouts;
}


/// The rectangle of half size @p aHalfX by @p aHalfY turned @p aDegrees counterclockwise, in a frame
/// whose Y axis points up, as Specctra's and IDF's do.
std::vector<VECTOR2D> turnedCorners( double aHalfX, double aHalfY, double aDegrees )
{
    const double          angle = aDegrees * M_PI / 180.0;
    std::vector<VECTOR2D> corners;

    for( const VECTOR2D& corner : { VECTOR2D( -aHalfX, -aHalfY ), VECTOR2D( aHalfX, -aHalfY ),
                                    VECTOR2D( aHalfX, aHalfY ), VECTOR2D( -aHalfX, aHalfY ) } )
    {
        corners.emplace_back( corner.x * std::cos( angle ) - corner.y * std::sin( angle ),
                              corner.x * std::sin( angle ) + corner.y * std::cos( angle ) );
    }

    return corners;
}


bool containsPoint( const std::vector<VECTOR2D>& aPoints, const VECTOR2D& aPoint, double aTolerance )
{
    for( const VECTOR2D& point : aPoints )
    {
        if( std::abs( point.x - aPoint.x ) < aTolerance && std::abs( point.y - aPoint.y ) < aTolerance )
            return true;
    }

    return false;
}


/// The distance from @p aPoint to the rectangle of half size @p aHalfX by @p aHalfY turned @p aDegrees
/// about the origin, zero inside it.
double distanceToTurnedRectangle( const VECTOR2D& aPoint, double aHalfX, double aHalfY, double aDegrees )
{
    const double angle = -aDegrees * M_PI / 180.0;
    const double x = aPoint.x * std::cos( angle ) - aPoint.y * std::sin( angle );
    const double y = aPoint.x * std::sin( angle ) + aPoint.y * std::cos( angle );

    return std::hypot( std::max( std::abs( x ) - aHalfX, 0.0 ), std::max( std::abs( y ) - aHalfY, 0.0 ) );
}


/// The keepouts of the unplated 3 x 2 mm rectangle's image, exported with that pad turned @p aDegrees
/// and the board's hole clearance set to @p aClearanceUm, or left as the board has it when that is
/// negative.  Returns the clearance used in @p aClearanceUm and the board's arc error in @p aMaxErrorUm.
std::vector<DSN_KEEPOUT> exportUnplatedRectangleKeepouts( double aDegrees, double& aClearanceUm, double& aMaxErrorUm,
                                                          bool& aHasPin )
{
    LOCALE_IO              toggle;
    std::unique_ptr<BOARD> board = KI_TEST::RequireBoardFromPcbnewQAData( FAB_BOARD );

    padOf( *board, wxT( "NR" ) )->SetFPRelativeOrientation( EDA_ANGLE( aDegrees, DEGREES_T ) );

    if( aClearanceUm >= 0.0 )
        board->GetDesignSettings().m_HoleClearance = pcbIUScale.mmToIU( aClearanceUm / 1000.0 );

    aClearanceUm = pcbIUScale.IUTomm( board->GetDesignSettings().m_HoleClearance ) * 1000.0;
    aMaxErrorUm = pcbIUScale.IUTomm( padOf( *board, wxT( "NR" ) )->GetMaxError() ) * 1000.0;

    KI_TEST::SCOPED_TEMP_DIR    outDir( wxS( "kicad_rect_hole_dsn" ) );
    const std::filesystem::path dsnPath = outDir.Path() / "fab.dsn";

    BOOST_REQUIRE_NO_THROW( DSN::ExportBoardToSpecctraFile( board.get(), KI_TEST::ToWxPath( dsnPath ) ) );

    // The image's keepouts must read back through the DSN parser
    DSN::SPECCTRA_DB db;

    try
    {
        db.LoadPCB( KI_TEST::ToWxPath( dsnPath ) );
    }
    catch( const IO_ERROR& ioe )
    {
        BOOST_FAIL( ioe.What().ToStdString() );
    }

    return imageKeepouts( slurp( dsnPath ), "probe:NR", aHasPin );
}


/// The BOARD_OUTLINE loops and the DRILLED_HOLES records of an IDF .emn file.
struct IDF_BOARD_FILE
{
    std::map<int, std::vector<VECTOR2D>>  m_loops;
    std::vector<std::vector<std::string>> m_drilledHoles;
};


IDF_BOARD_FILE readEmn( const std::filesystem::path& aPath )
{
    std::istringstream in( slurp( aPath ) );
    IDF_BOARD_FILE     file;
    std::string        section;

    for( std::string line; std::getline( in, line ); )
    {
        if( line.rfind( ".END_", 0 ) == 0 )
        {
            section.clear();
            continue;
        }

        if( line.rfind( ".", 0 ) == 0 )
        {
            section = line.substr( 0, line.find_first_of( " \r" ) );
            continue;
        }

        std::istringstream       fields( line );
        std::vector<std::string> tokens;

        for( std::string token; fields >> token; )
            tokens.push_back( token );

        // A loop point is "<loop> <x> <y> <angle>"; the owner and thickness lines are shorter
        if( section == ".BOARD_OUTLINE" && tokens.size() == 4 )
            file.m_loops[std::stoi( tokens[0] )].emplace_back( std::stod( tokens[1] ), std::stod( tokens[2] ) );
        else if( section == ".DRILLED_HOLES" && tokens.size() >= 7 )
            file.m_drilledHoles.push_back( tokens );
    }

    return file;
}

} // namespace


BOOST_AUTO_TEST_SUITE( ExporterRectangularHoles )


// Specctra's padstack describes copper and has no hole, so a hole reaches the router only as an
// image keepout.  A rectangle with no copper outside it is that keepout instead of a padstack of the
// copper inside the hole.  With no hole clearance it is the rectangle itself, a rect when square
// to the image.
BOOST_AUTO_TEST_CASE( SpecctraKeepsOutTheRectangle )
{
    double clearanceUm = 0.0;
    double maxErrorUm = 0.0;
    bool   hasPin = true;

    std::vector<DSN_KEEPOUT> keepouts = exportUnplatedRectangleKeepouts( 0.0, clearanceUm, maxErrorUm, hasPin );

    BOOST_CHECK_MESSAGE( !hasPin, "the unplated rectangle is still a pin" );
    BOOST_REQUIRE_EQUAL( keepouts.size(), 2 ); // one per copper layer
    BOOST_REQUIRE_EQUAL( clearanceUm, 0.0 );

    for( const DSN_KEEPOUT& keepout : keepouts )
    {
        BOOST_CHECK_EQUAL( keepout.m_shape, "rect" );
        BOOST_REQUIRE_EQUAL( keepout.m_numbers.size(), 4 );
        BOOST_CHECK_CLOSE( keepout.m_numbers[0], -1500.0, 1e-6 );
        BOOST_CHECK_CLOSE( keepout.m_numbers[1], -1000.0, 1e-6 );
        BOOST_CHECK_CLOSE( keepout.m_numbers[2], 1500.0, 1e-6 );
        BOOST_CHECK_CLOSE( keepout.m_numbers[3], 1000.0, 1e-6 );
    }

    BOOST_CHECK( keepouts[0].m_layer != keepouts[1].m_layer );
}


// A Specctra rect is square to its axes, so a rectangle turned within its footprint is a polygon
BOOST_AUTO_TEST_CASE( SpecctraKeepsOutTheTurnedRectangle )
{
    double clearanceUm = 0.0;
    double maxErrorUm = 0.0;
    bool   hasPin = true;

    std::vector<DSN_KEEPOUT> keepouts = exportUnplatedRectangleKeepouts( 30.0, clearanceUm, maxErrorUm, hasPin );

    BOOST_CHECK_MESSAGE( !hasPin, "the unplated rectangle is still a pin" );
    BOOST_REQUIRE_EQUAL( keepouts.size(), 2 );

    // KiCad's 30 degrees is counterclockwise as seen, so in the DSN's Y-up frame too
    std::vector<VECTOR2D> expected = turnedCorners( 1500.0, 1000.0, 30.0 );

    for( const DSN_KEEPOUT& keepout : keepouts )
    {
        BOOST_CHECK_EQUAL( keepout.m_shape, "polygon" );

        // The aperture width, then the four corners and the first again to close it
        BOOST_REQUIRE_EQUAL( keepout.m_numbers.size(), 11 );
        BOOST_CHECK_EQUAL( keepout.m_numbers[0], 0.0 );

        std::vector<VECTOR2D> vertices;

        for( size_t ii = 1; ii + 1 < keepout.m_numbers.size(); ii += 2 )
            vertices.emplace_back( keepout.m_numbers[ii], keepout.m_numbers[ii + 1] );

        BOOST_CHECK_SMALL( ( vertices.front() - vertices.back() ).EuclideanNorm(), 1e-9 );

        for( const VECTOR2D& corner : expected )
        {
            BOOST_CHECK_MESSAGE( containsPoint( vertices, corner, 0.05 ),
                                 "corner (" << corner.x << ", " << corner.y << ") missing from the keepout" );
        }
    }
}


// With a hole clearance the outline is the rectangle with its corners rounded to the clearance, written
// as a polygon whose arcs are approximated on the outside, within the board's arc error
BOOST_AUTO_TEST_CASE( SpecctraKeepsOutTheRectangleWithRoundedCorners )
{
    for( double degrees : { 0.0, 30.0 } )
    {
        double clearanceUm = -1.0; // as the board has it
        double maxErrorUm = 0.0;
        bool   hasPin = true;

        std::vector<DSN_KEEPOUT> keepouts = exportUnplatedRectangleKeepouts( degrees, clearanceUm, maxErrorUm, hasPin );

        BOOST_REQUIRE_GT( clearanceUm, 0.0 );
        BOOST_REQUIRE_GT( maxErrorUm, 0.0 );
        BOOST_CHECK_MESSAGE( !hasPin, "the unplated rectangle is still a pin" );
        BOOST_REQUIRE_EQUAL( keepouts.size(), 2 );

        for( const DSN_KEEPOUT& keepout : keepouts )
        {
            // Even square to the image it is no longer a rect: a rect has sharp corners
            BOOST_CHECK_EQUAL( keepout.m_shape, "polygon" );
            BOOST_REQUIRE_GT( keepout.m_numbers.size(), 1 + 2 * 12 );
            BOOST_CHECK_EQUAL( keepout.m_numbers[0], 0.0 );

            std::vector<VECTOR2D> vertices;

            for( size_t ii = 1; ii + 1 < keepout.m_numbers.size(); ii += 2 )
                vertices.emplace_back( keepout.m_numbers[ii], keepout.m_numbers[ii + 1] );

            BOOST_CHECK_SMALL( ( vertices.front() - vertices.back() ).EuclideanNorm(), 1e-9 );
            vertices.pop_back();

            // Every vertex is on the clearance, and the chords between them are outside it by at most
            // the arc error: the polygon covers the true outline and exceeds it by no more than that
            const double slack = 0.01;

            for( size_t ii = 0; ii < vertices.size(); ++ii )
            {
                const VECTOR2D& vertex = vertices[ii];
                const VECTOR2D& next = vertices[( ii + 1 ) % vertices.size()];
                const VECTOR2D  middle = ( vertex + next ) / 2.0;

                const double vertexDistance = distanceToTurnedRectangle( vertex, 1500.0, 1000.0, degrees );
                const double middleDistance = distanceToTurnedRectangle( middle, 1500.0, 1000.0, degrees );

                BOOST_CHECK_MESSAGE( vertexDistance >= clearanceUm - slack
                                             && vertexDistance <= clearanceUm + maxErrorUm + slack,
                                     "vertex (" << vertex.x << ", " << vertex.y << ") is " << vertexDistance
                                                << " um from the hole" );
                BOOST_CHECK_MESSAGE( middleDistance >= clearanceUm - slack
                                             && middleDistance <= clearanceUm + maxErrorUm + slack,
                                     "edge midpoint (" << middle.x << ", " << middle.y << ") is " << middleDistance
                                                       << " um from the hole" );
            }

            // The straight sides are at the clearance: the four sharp corners of the old outline,
            // which lay sqrt(2) times the clearance out, are not in the polygon
            for( const VECTOR2D& sharp : turnedCorners( 1500.0 + clearanceUm, 1000.0 + clearanceUm, degrees ) )
            {
                BOOST_CHECK_MESSAGE( !containsPoint( vertices, sharp, clearanceUm / 4.0 ),
                                     "the sharp corner (" << sharp.x << ", " << sharp.y << ") is still there" );
            }

            // Each side's two ends are there, where the arc meets the straight side
            for( const VECTOR2D& end : turnedCorners( 1500.0, 1000.0, degrees ) )
            {
                int near = 0;

                for( const VECTOR2D& vertex : vertices )
                    near += ( vertex - end ).EuclideanNorm() < clearanceUm + maxErrorUm + 0.01 ? 1 : 0;

                // Both ends of the arc and more than the one a bevel would add between them
                BOOST_CHECK_GE( near, 4 );
            }

            // The rounded corners trade the arcs' vertices for the sharp ones, so each corner has several
            BOOST_CHECK_MESSAGE( vertices.size() % 4 == 0 && vertices.size() / 4 >= 3,
                                 "vertex count " << vertices.size() << " is not at least three per corner" );
            BOOST_TEST_MESSAGE( "turned " << degrees << " degrees: " << vertices.size() << " vertices, "
                                          << vertices.size() / 4 << " per corner at a clearance of " << clearanceUm
                                          << " um and an arc error of " << maxErrorUm << " um" );
        }
    }
}


// IDF has no rectangular hole: as for a slot, the rectangle is a board cutout, a square included,
// and DRILLED_HOLES keeps only the round hole
BOOST_AUTO_TEST_CASE( IdfCutsOutTheRectangle )
{
    LOCALE_IO              toggle;
    std::unique_ptr<BOARD> board = KI_TEST::RequireBoardFromPcbnewQAData( FAB_BOARD );

    KI_TEST::SCOPED_TEMP_DIR    outDir( wxS( "kicad_rect_hole_idf" ) );
    const std::filesystem::path emnPath = outDir.Path() / "fab.emn";

    JOB_EXPORT_PCB_IDF job;
    job.m_originMode = IDF_SETTINGS::COORD_ORIGIN::USER;
    FILENAME_RESOLVER  resolver;
    WX_STRING_REPORTER reporter;

    IDF_EXPORTER exporter( board.get(), &resolver, &job, &reporter );
    BOOST_REQUIRE_MESSAGE( exporter.Export( KI_TEST::ToWxPath( emnPath ) ),
                           "IDF export failed: " << reporter.GetMessages() );

    IDF_BOARD_FILE emn = readEmn( emnPath );

    // The board, the oval slot and the four rectangles
    BOOST_CHECK_EQUAL( emn.m_loops.size(), 6 );

    BOOST_REQUIRE_EQUAL( emn.m_drilledHoles.size(), 1 );
    BOOST_CHECK_CLOSE( std::stod( emn.m_drilledHoles[0][0] ), 1.0, 1e-6 );

    for( const wxString& reference : { wxT( "SQ" ), wxT( "NR" ), wxT( "R30" ), wxT( "R90" ) } )
    {
        PAD*           pad = padOf( *board, reference );
        const VECTOR2I drill = pad->GetDrillSize();
        const VECTOR2D center( pcbIUScale.IUTomm( pad->GetPosition().x ), -pcbIUScale.IUTomm( pad->GetPosition().y ) );
        std::vector<VECTOR2D> expected =
                turnedCorners( pcbIUScale.IUTomm( drill.x ) / 2.0, pcbIUScale.IUTomm( drill.y ) / 2.0,
                               pad->GetOrientation().AsDegrees() );

        bool found = false;

        for( const auto& [index, points] : emn.m_loops )
        {
            if( index == 0 )
                continue;

            bool hasAllCorners = true;

            for( const VECTOR2D& corner : expected )
                hasAllCorners &= containsPoint( points, center + corner, 1e-3 );

            // Four corners and the first again: straight sides, no arcs
            if( hasAllCorners && points.size() == 5 )
                found = true;
        }

        BOOST_CHECK_MESSAGE( found, "no rectangular cutout for " << reference );
    }
}


// A HyperLynx padstack's drill is one diameter, so the rectangles and the slot are written round and
// reported once each for the export
BOOST_AUTO_TEST_CASE( HyperlynxReportsTheRectangleAndSlotOnce )
{
    LOCALE_IO              toggle;
    std::unique_ptr<BOARD> board = KI_TEST::RequireBoardFromPcbnewQAData( FAB_BOARD );

    KI_TEST::SCOPED_TEMP_DIR outDir( wxS( "kicad_rect_hole_hyp" ) );
    WX_STRING_REPORTER       reporter;

    BOOST_REQUIRE( ExportBoardToHyperlynxFile( board.get(), outDir.PathStr() + wxT( "/fab.hyp" ), &reporter ) );

    BOOST_CHECK_EQUAL( countOccurrences( reporter.GetMessages(), wxT( "Hyperlynx has no rectangular hole" ) ), 1 );
    BOOST_CHECK( reporter.GetMessages().Contains(
            wxString::Format( wxT( ": %d rectangular pad holes" ), FAB_RECTANGULAR_HOLE_COUNT ) ) );

    // The oval slot is written round too, and reported once, apart from the rectangles
    BOOST_CHECK_EQUAL( countOccurrences( reporter.GetMessages(), wxT( "Hyperlynx has no oval slot" ) ), 1 );
    BOOST_CHECK( reporter.GetMessages().Contains( wxString::Format( wxT( ": %d oval pad slots" ), FAB_SLOT_COUNT ) ) );

    // Without a rectangular hole there is nothing to report
    for( FOOTPRINT* footprint : board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
        {
            if( pad->GetDrillShape() == PAD_DRILL_SHAPE::RECTANGLE )
                pad->SetDrillShape( PAD_DRILL_SHAPE::OBLONG );
        }
    }

    WX_STRING_REPORTER roundReporter;

    BOOST_REQUIRE( ExportBoardToHyperlynxFile( board.get(), outDir.PathStr() + wxT( "/oval.hyp" ), &roundReporter ) );
    BOOST_CHECK( !roundReporter.GetMessages().Contains( wxT( "rectangular hole" ) ) );

    // The three rectangles that are not square are slots now, with the oval one; the square is a round hole
    BOOST_CHECK( roundReporter.GetMessages().Contains( wxString::Format( wxT( ": %d oval pad slots" ), 4 ) ) );

    // With only round holes there is nothing to report
    for( FOOTPRINT* footprint : board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
            pad->SetDrillShape( PAD_DRILL_SHAPE::CIRCLE );
    }

    WX_STRING_REPORTER silentReporter;

    BOOST_REQUIRE( ExportBoardToHyperlynxFile( board.get(), outDir.PathStr() + wxT( "/round.hyp" ), &silentReporter ) );
    BOOST_CHECK_MESSAGE( silentReporter.GetMessages().IsEmpty(), silentReporter.GetMessages() );
}


// GenCAD's pads and padstacks carry one drill size, so the rectangles and the slot are written round and
// reported once each for the export
BOOST_AUTO_TEST_CASE( GencadReportsTheRectangleAndSlotOnce )
{
    LOCALE_IO              toggle;
    std::unique_ptr<BOARD> board = KI_TEST::RequireBoardFromPcbnewQAData( FAB_BOARD );

    KI_TEST::SCOPED_TEMP_DIR outDir( wxS( "kicad_rect_hole_gencad" ) );
    WX_STRING_REPORTER       reporter;

    GENCAD_EXPORTER exporter( board.get() );
    exporter.SetReporter( &reporter );
    BOOST_REQUIRE( exporter.WriteFile( outDir.PathStr() + wxT( "/fab.cad" ) ) );

    BOOST_CHECK_EQUAL( countOccurrences( reporter.GetMessages(), wxT( "GenCAD has no rectangular hole" ) ), 1 );
    BOOST_CHECK( reporter.GetMessages().Contains(
            wxString::Format( wxT( ": %d rectangular pad holes" ), FAB_RECTANGULAR_HOLE_COUNT ) ) );

    // The oval slot is written round too, and reported once, apart from the rectangles
    BOOST_CHECK_EQUAL( countOccurrences( reporter.GetMessages(), wxT( "GenCAD has no oval slot" ) ), 1 );
    BOOST_CHECK( reporter.GetMessages().Contains( wxString::Format( wxT( ": %d oval pad slots" ), FAB_SLOT_COUNT ) ) );

    for( FOOTPRINT* footprint : board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
        {
            if( pad->GetDrillShape() == PAD_DRILL_SHAPE::RECTANGLE )
                pad->SetDrillShape( PAD_DRILL_SHAPE::OBLONG );
        }
    }

    WX_STRING_REPORTER roundReporter;
    GENCAD_EXPORTER    roundExporter( board.get() );
    roundExporter.SetReporter( &roundReporter );
    BOOST_REQUIRE( roundExporter.WriteFile( outDir.PathStr() + wxT( "/oval.cad" ) ) );
    BOOST_CHECK( !roundReporter.GetMessages().Contains( wxT( "rectangular hole" ) ) );

    // The three rectangles that are not square are slots now, with the oval one; the square is a round hole
    BOOST_CHECK( roundReporter.GetMessages().Contains( wxString::Format( wxT( ": %d oval pad slots" ), 4 ) ) );

    // With only round holes there is nothing to report
    for( FOOTPRINT* footprint : board->Footprints() )
    {
        for( PAD* pad : footprint->Pads() )
            pad->SetDrillShape( PAD_DRILL_SHAPE::CIRCLE );
    }

    WX_STRING_REPORTER silentReporter;
    GENCAD_EXPORTER    silentExporter( board.get() );
    silentExporter.SetReporter( &silentReporter );
    BOOST_REQUIRE( silentExporter.WriteFile( outDir.PathStr() + wxT( "/round.cad" ) ) );
    BOOST_CHECK_MESSAGE( silentReporter.GetMessages().IsEmpty(), silentReporter.GetMessages() );
}


BOOST_AUTO_TEST_SUITE_END()

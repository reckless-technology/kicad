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

#include <boost/test/unit_test.hpp>

#include <qa_utils/file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <cli/exit_codes.h>
#include <eeschema_jobs_handler.h>
#include <jobs/job_sch_import.h>
#include <locale_io.h>
#include <reporter.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>


namespace
{
namespace fs = std::filesystem;

/// Copy the issue12814 project, a root with two sub-sheets, into a fresh directory
fs::path copyHierarchy( const fs::path& aDestination )
{
    const fs::path source = fs::path( KI_TEST::GetTestDataRootDir() ) / "eeschema";

    for( const char* name :
         { "issue12814.kicad_pro", "issue12814.kicad_sch", "issue12814_1.kicad_sch", "issue12814_2.kicad_sch" } )
    {
        fs::copy_file( source / name, aDestination / name );
    }

    return aDestination / "issue12814.kicad_sch";
}


std::string readFile( const fs::path& aPath )
{
    std::ifstream stream( aPath, std::ios::binary );
    return std::string( std::istreambuf_iterator<char>( stream ), std::istreambuf_iterator<char>() );
}


/// The last step of a project import: U1, on the first sub-sheet, takes the board's footprint
int linkFootprints( const fs::path& aRoot, WX_STRING_REPORTER& aReporter )
{
    JOB_SCH_IMPORT job;
    job.m_inputFile = KI_TEST::ToWxPath( aRoot );
    job.m_footprintIds = { { wxS( "U1" ), wxS( "Package_DIP:DIP-8_W7.62mm" ) } };

    EESCHEMA_JOBS_HANDLER handler( nullptr );
    handler.SetReporter( &aReporter );

    return handler.JobImport( &job );
}
} // namespace


BOOST_AUTO_TEST_SUITE( JobImportLinkFootprints )


// With every sheet present the link is saved into the sheet that holds the symbol
BOOST_AUTO_TEST_CASE( CompleteHierarchyIsLinked )
{
    LOCALE_IO dummy;

    KI_TEST::SCOPED_TEMP_DIR directory( wxS( "kicad_import_link_complete" ) );
    const fs::path           root = copyHierarchy( directory.Path() );
    WX_STRING_REPORTER       reporter;

    BOOST_CHECK_EQUAL( linkFootprints( root, reporter ), CLI::EXIT_CODES::SUCCESS );
    BOOST_CHECK( readFile( directory.Path() / "issue12814_1.kicad_sch" ).find( "Package_DIP:DIP-8_W7.62mm" )
                 != std::string::npos );
}


// A sub-sheet whose file is gone is reported, and is not written back as an empty schematic
BOOST_AUTO_TEST_CASE( MissingSubsheetIsReportedNotWrittenEmpty )
{
    LOCALE_IO dummy;

    KI_TEST::SCOPED_TEMP_DIR directory( wxS( "kicad_import_link_missing" ) );
    const fs::path           root = copyHierarchy( directory.Path() );
    const fs::path           missing = directory.Path() / "issue12814_2.kicad_sch";
    const std::string        firstSheet = readFile( directory.Path() / "issue12814_1.kicad_sch" );
    WX_STRING_REPORTER       reporter;

    fs::remove( missing );

    BOOST_CHECK_EQUAL( linkFootprints( root, reporter ), CLI::EXIT_CODES::ERR_INVALID_INPUT_FILE );
    BOOST_CHECK_MESSAGE( !fs::exists( missing ), "the missing sub-sheet was written out" );
    BOOST_CHECK_MESSAGE( reporter.GetMessages().Contains( wxS( "issue12814_2.kicad_sch" ) ),
                         "the report does not name the missing sheet: " << reporter.GetMessages() );

    // Nothing is saved, so the sheets that did load are left as they were
    BOOST_CHECK( readFile( directory.Path() / "issue12814_1.kicad_sch" ) == firstSheet );
}


BOOST_AUTO_TEST_SUITE_END()

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
 * @file
 * Saving a board that has not been edited must write the same bytes every time.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <qa_utils/file_utils.h>

#include <board.h>
#include <board_tables/generated_table_refresh.h>
#include <pcbnew_utils/board_file_utils.h>
#include <pcbnew_utils/board_test_utils.h>
#include <project.h>
#include <settings/settings_manager.h>

#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>


namespace
{

std::string readFile( const std::filesystem::path& aPath )
{
    std::ifstream     stream( aPath, std::ios::binary );
    std::stringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}


/// Load aSource, save it to aTarget and return what was written.
std::string loadAndSave( const std::filesystem::path& aSource, const std::filesystem::path& aTarget )
{
    std::unique_ptr<BOARD> board = KI_TEST::ReadBoardFromFileOrStream( aSource.string() );
    BOOST_REQUIRE( board );

    KI_TEST::DumpBoardToFile( *board, aTarget );
    return readFile( aTarget );
}

/**
 * What pcbnew does to a board between loading it and saving it, as far as it can run without a window: the net list
 * and connectivity builds, the component class and net class synchronization, a refill of every zone, and the property
 * and generated table refreshes the save path makes (PCB_EDIT_FRAME::OpenProjectFiles, BOARD_LOADER::Load and
 * PCB_EDIT_FRAME::SavePcbCopy). The board repair action only changes boards with duplicate UUIDs or orphaned nets, and
 * rehatching only touches shapes, so neither reorders tracks and neither is run here.
 */
void runPostLoadSteps( BOARD& aBoard )
{
    aBoard.SynchronizeComponentClasses( std::unordered_set<wxString>() );
    aBoard.BuildListOfNets();
    aBoard.BuildConnectivity();
    aBoard.SynchronizeNetsAndNetClasses( true );

    KI_TEST::FillZones( &aBoard );

    aBoard.SynchronizeNetsAndNetClasses( false );
    aBoard.SynchronizeProperties();
    RefreshGeneratedTables( aBoard );
}


/// Load aSource into aProject, run the post-load steps, save it to aTarget and return what was written.
std::string openAndSave( PROJECT* aProject, const std::filesystem::path& aSource, const std::filesystem::path& aTarget )
{
    std::unique_ptr<BOARD> board = KI_TEST::ReadBoardFromFileOrStream( aSource.string() );
    BOOST_REQUIRE( board );

    board->SetProject( aProject );
    runPostLoadSteps( *board );

    KI_TEST::DumpBoardToFile( *board, aTarget );
    board->SetProject( nullptr );
    return readFile( aTarget );
}

} // namespace


BOOST_AUTO_TEST_SUITE( BoardSaveOrder )


/**
 * Net codes are handed out in the order the loader meets each net, so the first save of a board from a hand-made or
 * older file listed its tracks in an order that the next load shuffled. Each save must be the same bytes as the last.
 */
BOOST_AUTO_TEST_CASE( SavingAnUntouchedBoardIsStable )
{
    const std::filesystem::path original =
            std::filesystem::path( KI_TEST::GetPcbnewTestDataDir() ) / "time_calculations.kicad_pcb";

    KI_TEST::SCOPED_TEMP_DIR tempDir( "kicad_qa_save_order" );

    const std::string first = loadAndSave( original, tempDir.Path() / "first.kicad_pcb" );
    BOOST_REQUIRE( !first.empty() );

    // Across a reload, which is what a user's next session does
    const std::string second = loadAndSave( tempDir.Path() / "first.kicad_pcb", tempDir.Path() / "second.kicad_pcb" );
    BOOST_CHECK_MESSAGE( first == second, "A reload and save changed the file" );

    const std::string third = loadAndSave( tempDir.Path() / "second.kicad_pcb", tempDir.Path() / "third.kicad_pcb" );
    BOOST_CHECK_MESSAGE( second == third, "A second reload and save changed the file" );

    // Twice from one load, which is what saving twice in one session does
    std::unique_ptr<BOARD> board = KI_TEST::ReadBoardFromFileOrStream( original.string() );
    BOOST_REQUIRE( board );

    const std::filesystem::path again1 = tempDir.Path() / "again1.kicad_pcb";
    const std::filesystem::path again2 = tempDir.Path() / "again2.kicad_pcb";

    KI_TEST::DumpBoardToFile( *board, again1 );
    KI_TEST::DumpBoardToFile( *board, again2 );
    BOOST_CHECK_MESSAGE( readFile( again1 ) == readFile( again2 ), "Two saves of one loaded board differ" );
}


/**
 * The same, through the steps pcbnew takes after loading a board: net and connectivity builds and a zone refill. The
 * first save and each of three reload-and-save rounds must be the same bytes.
 */
BOOST_AUTO_TEST_CASE( SavingAfterThePostLoadStepsIsStable )
{
    const std::filesystem::path dataDir( KI_TEST::GetPcbnewTestDataDir() );
    const std::filesystem::path boards[] = { dataDir / "time_calculations.kicad_pcb",
                                             dataDir / ".." / ".." / ".." / "demos" / "video" / "video.kicad_pcb" };

    for( const std::filesystem::path& original : boards )
    {
        BOOST_TEST_CONTEXT( original.filename().string() )
        {
            KI_TEST::SCOPED_TEMP_DIR tempDir( "kicad_qa_save_order_gui" );

            // The board's own project, which the steps read the net classes and component classes from
            std::filesystem::path projectFile = original;
            projectFile.replace_extension( ".kicad_pro" );

            SETTINGS_MANAGER settingsManager;
            settingsManager.LoadProject( projectFile.string() );
            PROJECT* project = settingsManager.GetProject( projectFile.string() );
            BOOST_REQUIRE( project );

            std::filesystem::path previous = tempDir.Path() / "save0.kicad_pcb";
            const std::string     first = openAndSave( project, original, previous );
            BOOST_REQUIRE( !first.empty() );

            for( int round = 1; round <= 3; ++round )
            {
                const std::filesystem::path next = tempDir.Path() / ( "save" + std::to_string( round ) + ".kicad_pcb" );

                BOOST_CHECK_MESSAGE( openAndSave( project, previous, next ) == first,
                                     "Round " << round << " wrote different bytes" );
                previous = next;
            }
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()

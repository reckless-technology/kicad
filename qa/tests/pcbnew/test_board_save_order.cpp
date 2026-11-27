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
#include <pcbnew_utils/board_file_utils.h>

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


BOOST_AUTO_TEST_SUITE_END()

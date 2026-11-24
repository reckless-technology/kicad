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
 * @file test_import_local_footprint_libs.cpp
 *
 * A non-KiCad project import registers the footprint libraries the source project names.  One of
 * the source project's own libraries is converted into a KiCad library in the new project and
 * registered as that, so the new project depends on neither the source's files nor its format.
 */

#include <qa_utils/file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_test_utils.h>

#include <wx/dir.h>
#include <wx/filename.h>

#include <io/io_mgr.h>
#include <libraries/library_manager.h>
#include <libraries/library_table.h>
#include <pcb_io/pcb_io.h>
#include <pcb_io/pcb_io_mgr.h>
#include <pgm_base.h>
#include <project.h>
#include <reporter.h>
#include <settings/settings_manager.h>
#include <wildcards_and_files_ext.h>

#include <import_local_footprint_libs.h>


BOOST_AUTO_TEST_SUITE( ImportLocalFootprintLibs )


// fails on revert, since the library is then copied and registered in its Altium format
BOOST_AUTO_TEST_CASE( ProjectLibraryIsConvertedToKiCad )
{
    KI_TEST::SCOPED_TEMP_DIR sourceDir( wxS( "altium_locallib_source" ) );
    KI_TEST::SCOPED_TEMP_DIR projectDir( wxS( "altium_locallib" ) );

    wxFileName projectFn( projectDir.PathStr(), wxS( "altium_locallib" ), FILEEXT::ProjectFileExtension );
    Pgm().GetSettingsManager().LoadProject( projectFn.GetFullPath() );
    Pgm().GetLibraryManager().LoadProjectTables( Pgm().GetSettingsManager().Prj().GetProjectDirectory() );
    PROJECT& project = Pgm().GetSettingsManager().Prj();

    wxString original =
            wxString::FromUTF8( KI_TEST::GetPcbnewTestDataDir() + "plugins/altium/pcblib/Tracks.v6.PcbLib" );
    wxFileName libFn( sourceDir.PathStr(), wxS( "Tracks.PcbLib" ) );
    BOOST_REQUIRE( wxCopyFile( original, libFn.GetFullPath() ) );

    wxArrayString       sourceNames;
    IO_RELEASER<PCB_IO> altium( PCB_IO_MGR::FindPlugin( PCB_IO_MGR::ALTIUM_DESIGNER ) );
    altium->FootprintEnumerate( sourceNames, libFn.GetFullPath(), true );
    BOOST_REQUIRE_GT( sourceNames.size(), 0 );

    WX_STRING_REPORTER    reporter;
    std::vector<wxString> added =
            RegisterLocalFootprintLibraries( project, sourceDir.PathStr(), { libFn.GetFullPath() }, reporter );

    BOOST_TEST_MESSAGE( reporter.GetMessages() );
    BOOST_REQUIRE_EQUAL( added.size(), 1 );
    BOOST_CHECK_EQUAL( added.front(), wxS( "Tracks" ) );

    std::optional<LIBRARY_TABLE*> table =
            Pgm().GetLibraryManager().Table( LIBRARY_TABLE_TYPE::FOOTPRINT, LIBRARY_TABLE_SCOPE::PROJECT );
    BOOST_REQUIRE( table );

    std::optional<LIBRARY_TABLE_ROW*> row = ( *table )->Row( wxS( "Tracks" ) );
    BOOST_REQUIRE( row );
    BOOST_CHECK_EQUAL( ( *row )->Type(), PCB_IO_MGR::ShowType( PCB_IO_MGR::KICAD_SEXP ) );
    BOOST_CHECK_EQUAL( ( *row )->URI(),
                       wxString( wxS( "${KIPRJMOD}/Tracks." ) ) + FILEEXT::KiCadFootprintLibPathExtension );

    // every footprint of the source library, in KiCad's format, and no copy of the source file
    wxFileName pretty = wxFileName::DirName( project.GetProjectPath() );
    pretty.AppendDir( wxString( wxS( "Tracks." ) ) + FILEEXT::KiCadFootprintLibPathExtension );
    BOOST_REQUIRE( pretty.DirExists() );

    wxArrayString       kicadNames;
    IO_RELEASER<PCB_IO> kicad( PCB_IO_MGR::FindPlugin( PCB_IO_MGR::KICAD_SEXP ) );
    kicad->FootprintEnumerate( kicadNames, pretty.GetPath(), true );
    BOOST_CHECK_EQUAL( kicadNames.size(), sourceNames.size() );

    BOOST_CHECK( !wxFileName( project.GetProjectPath(), libFn.GetFullName() ).FileExists() );

    Pgm().GetSettingsManager().LoadProject( wxS( "" ) );
}


BOOST_AUTO_TEST_SUITE_END()

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
 * @file test_import_local_symbol_libs.cpp
 *
 * A non-KiCad project import registers the symbol libraries the source project names.  One of the
 * source project's own libraries is converted into a KiCad library in the new project and
 * registered as that, so the new project depends on neither the source's files nor its format.
 */

#include <qa_utils/file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <wx/filename.h>

#include <io/io_mgr.h>
#include <libraries/library_manager.h>
#include <libraries/library_table.h>
#include <pgm_base.h>
#include <project.h>
#include <reporter.h>
#include <sch_io/sch_io.h>
#include <sch_io/sch_io_mgr.h>
#include <settings/settings_manager.h>
#include <wildcards_and_files_ext.h>

#include <import_local_symbol_libs.h>


BOOST_AUTO_TEST_SUITE( ImportLocalSymbolLibs )


// fails on revert, since the library is then copied and registered in its Altium format
BOOST_AUTO_TEST_CASE( ProjectLibraryIsConvertedToKiCad )
{
    KI_TEST::SCOPED_TEMP_DIR sourceDir( wxS( "altium_localsymlib_source" ) );
    KI_TEST::SCOPED_TEMP_DIR projectDir( wxS( "altium_localsymlib" ) );

    wxFileName projectFn( projectDir.PathStr(), wxS( "altium_localsymlib" ), FILEEXT::ProjectFileExtension );
    Pgm().GetSettingsManager().LoadProject( projectFn.GetFullPath() );
    Pgm().GetLibraryManager().LoadProjectTables( Pgm().GetSettingsManager().Prj().GetProjectDirectory() );
    PROJECT& project = Pgm().GetSettingsManager().Prj();

    wxString original =
            wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir() + "plugins/altium/issue22943/mounting_holes.SchLib" );
    wxFileName libFn( sourceDir.PathStr(), wxS( "mounting_holes.SchLib" ) );
    BOOST_REQUIRE( wxCopyFile( original, libFn.GetFullPath() ) );

    wxArrayString       sourceNames;
    IO_RELEASER<SCH_IO> altium( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_ALTIUM ) );
    altium->EnumerateSymbolLib( sourceNames, libFn.GetFullPath() );
    BOOST_REQUIRE_GT( sourceNames.size(), 0 );

    WX_STRING_REPORTER    reporter;
    std::vector<wxString> added =
            RegisterLocalSymbolLibraries( project, sourceDir.PathStr(), { libFn.GetFullPath() }, reporter );

    BOOST_TEST_MESSAGE( reporter.GetMessages() );
    BOOST_REQUIRE_EQUAL( added.size(), 1 );
    BOOST_CHECK_EQUAL( added.front(), wxS( "mounting_holes" ) );

    std::optional<LIBRARY_TABLE*> table =
            Pgm().GetLibraryManager().Table( LIBRARY_TABLE_TYPE::SYMBOL, LIBRARY_TABLE_SCOPE::PROJECT );
    BOOST_REQUIRE( table );

    std::optional<LIBRARY_TABLE_ROW*> row = ( *table )->Row( wxS( "mounting_holes" ) );
    BOOST_REQUIRE( row );
    BOOST_CHECK_EQUAL( ( *row )->Type(), SCH_IO_MGR::ShowType( SCH_IO_MGR::SCH_KICAD ) );
    BOOST_CHECK_EQUAL( ( *row )->URI(), wxS( "${KIPRJMOD}/mounting_holes." ) + FILEEXT::KiCadSymbolLibFileExtension );

    // every symbol of the source library, in KiCad's format, and no copy of the source file
    wxFileName converted( project.GetProjectPath(), wxS( "mounting_holes" ), FILEEXT::KiCadSymbolLibFileExtension );
    BOOST_REQUIRE( converted.FileExists() );

    wxArrayString       kicadNames;
    IO_RELEASER<SCH_IO> kicad( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
    kicad->EnumerateSymbolLib( kicadNames, converted.GetFullPath() );
    BOOST_CHECK_EQUAL( kicadNames.size(), sourceNames.size() );

    BOOST_CHECK( !wxFileName( project.GetProjectPath(), libFn.GetFullName() ).FileExists() );

    Pgm().GetSettingsManager().LoadProject( wxS( "" ) );
}


BOOST_AUTO_TEST_SUITE_END()

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

#include <algorithm>

#include <io/io_mgr.h>
#include <libraries/library_manager.h>
#include <libraries/library_table.h>
#include <pgm_base.h>
#include <project.h>
#include <reporter.h>
#include <sch_io/sch_io.h>
#include <sch_io/sch_io_mgr.h>
#include <lib_symbol.h>
#include <sch_field.h>
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


namespace
{
/// The footprint field of every symbol of a converted library, empty fields left out
std::vector<wxString> convertedFootprintFields( const wxString& aLibraryPath,
                                                const SCH_IO_MGR::FOOTPRINT_RESOLVER& aResolver )
{
    KI_TEST::SCOPED_TEMP_DIR outDir( wxS( "altium_schlib_footprints" ) );
    wxFileName               out( outDir.PathStr(), wxS( "converted" ), FILEEXT::KiCadSymbolLibFileExtension );

    WX_STRING_REPORTER reporter;
    bool ok = SCH_IO_MGR::ConvertLibrary( nullptr, aLibraryPath, out.GetFullPath(), &reporter, aResolver );
    BOOST_TEST_MESSAGE( reporter.GetMessages() );
    BOOST_REQUIRE( ok );

    std::vector<LIB_SYMBOL*> symbols;
    IO_RELEASER<SCH_IO>      kicad( SCH_IO_MGR::FindPlugin( SCH_IO_MGR::SCH_KICAD ) );
    kicad->EnumerateSymbolLib( symbols, out.GetFullPath() );

    std::vector<wxString> fields;

    for( LIB_SYMBOL* symbol : symbols )
    {
        wxString text = symbol->GetField( FIELD_T::FOOTPRINT )->GetText();

        if( !text.IsEmpty() )
            fields.push_back( text );
    }

    return fields;
}
} // namespace


// fails on revert, since the footprint an Altium symbol names is the source PcbLib's, not an installed library
BOOST_AUTO_TEST_CASE( ConvertedSchLibQualifiesFootprintFields )
{
    wxString original =
            wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir() + "plugins/altium/issue22943/mounting_holes.SchLib" );

    // without a footprint library table to consult, the symbols keep what Altium names
    std::vector<wxString> plain = convertedFootprintFields( original, nullptr );
    BOOST_REQUIRE_GT( plain.size(), 0 );

    // one names the source library, the other no library at all; neither is an installed library
    BOOST_CHECK( std::any_of( plain.begin(), plain.end(), []( const wxString& f ) { return f.Contains( ':' ); } ) );
    BOOST_CHECK( std::any_of( plain.begin(), plain.end(), []( const wxString& f ) { return !f.Contains( ':' ); } ) );

    // with one, the installed library that holds the footprint is named
    std::vector<wxString> qualified = convertedFootprintFields(
            original,
            []( const wxString& aFootprint )
            {
                return wxS( "InstalledLib:" ) + aFootprint.AfterLast( ':' );
            } );

    BOOST_REQUIRE_EQUAL( qualified.size(), plain.size() );

    for( size_t i = 0; i < qualified.size(); ++i )
        BOOST_CHECK_EQUAL( qualified[i], wxS( "InstalledLib:" ) + plain[i].AfterLast( ':' ) );

    // a footprint no installed library holds is left as the source named it
    std::vector<wxString> unresolved =
            convertedFootprintFields( original, []( const wxString& ) { return wxString(); } );
    BOOST_CHECK( unresolved == plain );
}


BOOST_AUTO_TEST_SUITE_END()

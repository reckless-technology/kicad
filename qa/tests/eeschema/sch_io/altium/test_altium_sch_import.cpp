/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <boost/test/unit_test.hpp>
#include <qa_utils/file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <connection_graph.h>
#include <erc/erc.h>
#include <font/font.h>
#include <font/outline_font.h>
#include <lib_id.h>
#include <reporter.h>
#include <netclass.h>
#include <project.h>
#include <project/net_settings.h>
#include <project/project_file.h>
#include <lib_symbol.h>
#include <drawing_sheet/ds_data_item.h>
#include <drawing_sheet/ds_data_model.h>
#include <embedded_files.h>
#include <sch_junction.h>
#include <sch_pin.h>
#include <sch_shape.h>
#include <sch_text.h>
#include <sch_textbox.h>
#include <schematic.h>
#include <sch_io/altium/sch_io_altium.h>
#include <sch_label.h>
#include <sch_no_connect.h>
#include <sch_line.h>
#include <sch_screen.h>
#include <sch_sheet.h>
#include <sch_sheet_path.h>
#include <sch_symbol.h>
#include <settings/settings_manager.h>
#include <import_proj_properties.h>
#include <libraries/library_manager.h>
#include <pgm_base.h>

#include <wx/wfstream.h>

#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <vector>


namespace
{

struct ALTIUM_SCH_IMPORT_FIXTURE
{
    ALTIUM_SCH_IMPORT_FIXTURE() :
            m_loadInfoScope( &NULL_REPORTER::GetInstance() ),
            m_schematic( nullptr )
    {
        m_settingsManager.LoadProject( "" );
        m_schematic.SetProject( &m_settingsManager.Prj() );
        m_schematic.Reset();
    }

    ~ALTIUM_SCH_IMPORT_FIXTURE() { m_schematic.Reset(); }

    wxString dataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue22943/" )
               + aName;
    }

    wxString issue24861DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue24861/" )
               + aName;
    }

    wxString issue22908DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue22908/" )
               + aName;
    }

    wxString ticket1303DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/ticket1303/" )
               + aName;
    }

    wxString issue26127DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue26127/" )
               + aName;
    }

    wxString issue26126DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue26126/" )
               + aName;
    }

    wxString issue26009DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue26009/" )
               + aName;
    }

    wxString textPlacementDataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/text_placement/" )
               + aName;
    }

    wxString pinTextDataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/pin_text/" )
               + aName;
    }

    wxString symbolGraphicsDataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/symbol_graphics/" )
               + aName;
    }

    wxString issue26130DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue26130/" )
               + aName;
    }

    wxString issue26128DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue26128/" )
               + aName;
    }

    wxString wireStyleDataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/wire_style/" )
               + aName;
    }

    wxString textFramesDataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/text_frames/" )
               + aName;
    }

    wxString issue16903DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue16903/" )
               + aName;
    }

    wxString issue19DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir() + "/plugins/altium/issue19/" ) + aName;
    }

    wxString eDPAdapterDataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetTestDataRootDir()
                                   + "pcbnew/plugins/altium/eDP_adapter_dvt1_source/" )
               + aName;
    }

    // The fixture project has no symbol libraries, so every symbol link fails by design
    LOAD_INFO_REPORTER_SCOPE m_loadInfoScope;
    SETTINGS_MANAGER         m_settingsManager;
    SCHEMATIC                m_schematic;
};

} // namespace


BOOST_FIXTURE_TEST_SUITE( AltiumSchImport, ALTIUM_SCH_IMPORT_FIXTURE )


// https://gitlab.com/kicad/code/kicad/-/issues/22943
// A component placed from an external Altium library must be addressed by a well-formed library
// id (source library nickname + real library reference) so it resolves against the library that
// project import registers, instead of the importer's internal per-placement name.
BOOST_AUTO_TEST_CASE( Issue22943_SourceLibrarySymbolLibId )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( dataFile( "1_cover.SchDoc" ), &m_schematic );
    BOOST_REQUIRE( rootSheet );
    BOOST_REQUIRE( rootSheet->GetScreen() );

    // The real reference names of every symbol contained in the source library.
    wxArrayString libNames;
    plugin.EnumerateSymbolLib( libNames, dataFile( "mounting_holes.SchLib" ) );

    std::set<wxString> libSymbolNames( libNames.begin(), libNames.end() );
    BOOST_REQUIRE( libSymbolNames.count( wxT( "MH M3" ) ) );

    std::vector<SCH_SYMBOL*> fromMountingHoles;

    for( SCH_ITEM* item : rootSheet->GetScreen()->Items().OfType( SCH_SYMBOL_T ) )
    {
        SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );

        if( sym->GetLibId().GetUniStringLibNickname() == wxT( "mounting_holes" ) )
            fromMountingHoles.push_back( sym );
    }

    BOOST_REQUIRE( !fromMountingHoles.empty() );

    // Every symbol drawn from the source library must name an item that actually exists there.
    for( SCH_SYMBOL* sym : fromMountingHoles )
    {
        const LIB_ID& libId = sym->GetLibId();
        BOOST_CHECK( libId.IsValid() );
        BOOST_CHECK_MESSAGE( libSymbolNames.count( libId.GetUniStringLibItemName() ),
                             "Library id '" << libId.Format().wx_str()
                                            << "' does not resolve in mounting_holes.SchLib" );
    }
}


// A sheet may not take the file of the root or of a sheet the schematic already holds, or saving
// would write one over the other
BOOST_AUTO_TEST_CASE( SheetsClaimFilesNoOtherSheetSavesTo )
{
    KI_TEST::SCOPED_TEMP_DIR directory( "altium-sheet-files" );
    const wxString          projectFile = directory.PathStr() + "/2_cubesat_bus.kicad_pro";
    BOOST_REQUIRE( wxCopyFile( KI_TEST::GetEeschemaTestDataDir() + "issue13591.kicad_pro", projectFile ) );

    SETTINGS_MANAGER         settings;
    BOOST_REQUIRE( settings.LoadProject( projectFile ) );

    SCHEMATIC schematic( nullptr );
    schematic.SetProject( &settings.Prj() );
    schematic.Reset();

    auto sheetFiles =
            []( SCH_SHEET* aSheet )
            {
                std::set<wxString> files;

                for( SCH_ITEM* item : aSheet->GetScreen()->Items().OfType( SCH_SHEET_T ) )
                    files.insert( static_cast<SCH_SHEET*>( item )->GetFileName() );

                return files;
            };

    SCH_IO_ALTIUM firstImport;
    SCH_SHEET*    root = firstImport.LoadSchematicFile( dataFile( "1_cover.SchDoc" ), &schematic );
    BOOST_REQUIRE( root );

    const std::set<wxString> hostFiles = sheetFiles( root );
    BOOST_CHECK( hostFiles.contains( wxS( "2_cubesat_bus_1.kicad_sch" ) ) );

    std::map<std::string, UTF8> properties{ { "hierarchical_sheet_load", "1" } };
    auto                        appended = std::make_unique<SCH_SHEET>( &schematic );
    appended->SetFileName( dataFile( "1_cover.SchDoc" ) );

    SCH_IO_ALTIUM secondImport;
    BOOST_REQUIRE( secondImport.LoadSchematicFile( dataFile( "1_cover.SchDoc" ), &schematic, appended.get(),
                                                   &properties ) );

    for( const wxString& file : sheetFiles( appended.get() ) )
        BOOST_CHECK_MESSAGE( !hostFiles.contains( file ), file << " is already saved by the host" );
}


// https://gitlab.com/kicad/code/kicad/-/issues/26129
// When the library tables already hold a library of the Altium library's name (KiCad's own
// "Relay", say) that the import does not register, the imported symbols must not take that
// nickname, or updating the symbol links replaces them with that library's different parts.
BOOST_AUTO_TEST_CASE( Issue26129_ForeignLibraryNicknameIsNotTaken )
{
    // A project whose library table has a "mounting_holes" of its own
    KI_TEST::SCOPED_TEMP_DIR projectDir( "altium-foreign-library" );

    {
        wxFFileOutputStream out( projectDir.ChildPathStr( wxS( "sym-lib-table" ) ) );
        BOOST_REQUIRE( out.IsOk() );
        const char* table = "(sym_lib_table\n"
                            "  (version 7)\n"
                            "  (lib (name \"mounting_holes\") (type \"KiCad\")"
                            " (uri \"${KIPRJMOD}/other.kicad_sym\") (options \"\") (descr \"\"))\n"
                            ")\n";
        out.WriteAll( table, std::strlen( table ) );
    }

    LIBRARY_MANAGER& manager = Pgm().GetLibraryManager();
    manager.LoadProjectTables( projectDir.PathStr() );

    auto nicknamesOf =
            [&]( const std::map<std::string, UTF8>* aProperties )
            {
                SCHEMATIC schematic( &m_settingsManager.Prj() );
                schematic.Reset();

                SCH_IO_ALTIUM plugin;
                SCH_SHEET*    rootSheet = plugin.LoadSchematicFile( dataFile( "1_cover.SchDoc" ),
                                                                    &schematic, nullptr,
                                                                    aProperties );
                std::map<wxString, int> nicknames;

                if( rootSheet && rootSheet->GetScreen() )
                {
                    for( SCH_ITEM* item : rootSheet->GetScreen()->Items().OfType( SCH_SYMBOL_T ) )
                    {
                        SCH_SYMBOL* sym = static_cast<SCH_SYMBOL*>( item );

                        if( sym->GetLibId().GetUniStringLibItemName() == wxT( "MH M3" ) )
                        {
                            nicknames[sym->GetLibId().GetUniStringLibNickname()]++;

                            // The symbol is still the one the importer drew, not left unlinked
                            BOOST_CHECK( sym->GetLibSymbolRef() );
                        }
                    }
                }

                schematic.Reset();
                return nicknames;
            };

    // Not registered by the import: the symbols take the project's own nickname instead
    std::map<wxString, int> standalone = nicknamesOf( nullptr );
    BOOST_CHECK( !standalone.empty() );
    BOOST_CHECK( !standalone.count( wxT( "mounting_holes" ) ) );

    for( const auto& [nickname, count] : standalone )
        BOOST_CHECK_MESSAGE( nickname.EndsWith( wxT( "-altium-import" ) ), nickname );

    // Registered by the project import, the library's own nickname is the right one
    std::map<std::string, UTF8> properties;
    properties[IMPORT_PROJ_PROPS::SOURCE_SYM_LIBS] = "mounting_holes";

    std::map<wxString, int> registered = nicknamesOf( &properties );
    BOOST_CHECK_EQUAL( registered.size(), 1 );
    BOOST_CHECK( registered.count( wxT( "mounting_holes" ) ) );

    // Leave the library tables as the other tests expect them
    manager.LoadProjectTables( m_settingsManager.Prj().GetProjectDirectory() );
}


// https://gitlab.com/kicad/code/kicad/-/issues/24861
BOOST_AUTO_TEST_CASE( Issue24861_RepeatedSchematicChannels )
{
    SCH_IO_ALTIUM plugin;

    std::map<std::string, UTF8> properties;
    properties.emplace( "project_file", UTF8( issue24861DataFile( wxT( "Repeated_Schematic.PrjPcb" ) ) ) );
    properties.emplace( "sch0", UTF8( issue24861DataFile( wxT( "Repeated_Schematic.SchDoc" ) ) ) );
    properties.emplace( "sch1", UTF8( issue24861DataFile( wxT( "Channel.SchDoc" ) ) ) );

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( wxEmptyString, &m_schematic, nullptr, &properties );
    BOOST_REQUIRE( rootSheet );

    const std::vector<SCH_SHEET*> topLevelSheets = m_schematic.GetTopLevelSheets();
    BOOST_REQUIRE_EQUAL( topLevelSheets.size(), 1 );
    BOOST_CHECK_EQUAL( topLevelSheets.front()->GetName(), wxT( "Repeated_Schematic" ) );

    std::optional<SCH_SHEET_PATH> topLevelPath;
    std::map<wxString, SCH_SHEET_PATH> channelPaths;

    for( const SCH_SHEET_PATH& sheetPath : m_schematic.Hierarchy() )
    {
        SCH_SHEET* sheet = sheetPath.Last();

        if( sheet && sheet->GetName() == wxT( "Repeated_Schematic" ) )
            topLevelPath = sheetPath;
        else if( sheet && sheet->GetName().StartsWith( wxT( "CH" ) ) )
            channelPaths.emplace( sheet->GetName(), sheetPath );
    }

    BOOST_REQUIRE( topLevelPath );
    BOOST_CHECK_EQUAL( topLevelPath->GetPageNumber(), wxT( "1" ) );

    BOOST_REQUIRE_EQUAL( channelPaths.size(), 3 );
    BOOST_CHECK_EQUAL( channelPaths.at( wxT( "CH1" ) ).GetPageNumber(), wxT( "2" ) );
    BOOST_CHECK_EQUAL( channelPaths.at( wxT( "CH2" ) ).GetPageNumber(), wxT( "3" ) );
    BOOST_CHECK_EQUAL( channelPaths.at( wxT( "CH3" ) ).GetPageNumber(), wxT( "4" ) );

    std::set<wxString> ledReferences;
    std::set<wxString> resistorReferences;

    for( const auto& [channelName, sheetPath] : channelPaths )
    {
        for( SCH_ITEM* item : sheetPath.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );
            wxString    ref = symbol->GetRef( &sheetPath );

            if( ref.StartsWith( wxT( "LED" ) ) )
                ledReferences.insert( ref );
            else if( ref.StartsWith( wxT( "R" ) ) )
                resistorReferences.insert( ref );
        }
    }

    BOOST_CHECK( ledReferences == std::set<wxString>( { wxT( "LED1_CH1" ), wxT( "LED1_CH2" ),
                                                        wxT( "LED1_CH3" ) } ) );
    BOOST_CHECK( resistorReferences == std::set<wxString>( { wxT( "R1_CH1" ), wxT( "R1_CH2" ),
                                                             wxT( "R1_CH3" ) } ) );

    // The channel names are suffixes, so every channel keeps the base the annotator shares
    BOOST_CHECK( m_schematic.Settings().m_AnnotateReusedSheetSuffixes );

    for( const auto& [channelName, sheetPath] : channelPaths )
    {
        BOOST_CHECK_EQUAL( m_schematic.GetRefSuffix( sheetPath ), wxT( "_" ) + channelName );

        for( SCH_ITEM* item : sheetPath.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

            if( !symbol->GetRef( &sheetPath ).StartsWith( wxT( "#" ) ) )
                BOOST_CHECK( !symbol->GetBaseRef( &sheetPath ).Contains( wxT( "_CH" ) ) );
        }
    }
}


// Repeated channels save one file, so they must share one screen and the top-level sheet uuid must
// match its file uuid or the per-channel references are dropped when the project is reloaded
BOOST_AUTO_TEST_CASE( RepeatedChannelsShareOneSavedScreen )
{
    SCH_IO_ALTIUM plugin;

    std::map<std::string, UTF8> properties;
    properties.emplace( "project_file", UTF8( issue24861DataFile( wxT( "Repeated_Schematic.PrjPcb" ) ) ) );
    properties.emplace( "sch0", UTF8( issue24861DataFile( wxT( "Repeated_Schematic.SchDoc" ) ) ) );
    properties.emplace( "sch1", UTF8( issue24861DataFile( wxT( "Channel.SchDoc" ) ) ) );

    BOOST_REQUIRE( plugin.LoadSchematicFile( wxEmptyString, &m_schematic, nullptr, &properties ) );

    const std::vector<SCH_SHEET*> topLevelSheets = m_schematic.GetTopLevelSheets();
    BOOST_REQUIRE_EQUAL( topLevelSheets.size(), 1 );
    BOOST_CHECK( topLevelSheets.front()->m_Uuid == topLevelSheets.front()->GetScreen()->GetUuid() );

    std::set<SCH_SCREEN*> channelScreens;

    for( const SCH_SHEET_PATH& sheetPath : m_schematic.Hierarchy() )
    {
        if( sheetPath.Last()->GetName().StartsWith( wxT( "CH" ) ) )
            channelScreens.insert( sheetPath.LastScreen() );
    }

    BOOST_CHECK_EQUAL( channelScreens.size(), 1 );
}


// https://gitlab.com/kicad/code/kicad/-/issues/22908
// Only the 34 Generic No ERC crosses are no-connects, the four Specific No ERC checkboxes are not
BOOST_AUTO_TEST_CASE( Issue22908_SpecificNoErcIsNotNoConnect )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( issue22908DataFile( wxT( "3_mpcie.SchDoc" ) ), &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    // Altium places these in 10 mil units with the Y axis flipped about the page bottom
    static const std::vector<VECTOR2I> crosses = {
        { 360, 500 }, { 360, 540 }, { 360, 610 }, { 360, 600 }, { 360, 620 }, { 360, 530 }, { 360, 460 },
        { 360, 450 }, { 170, 550 }, { 170, 540 }, { 170, 520 }, { 170, 510 }, { 170, 610 }, { 170, 590 },
        { 170, 580 }, { 880, 500 }, { 880, 540 }, { 880, 610 }, { 880, 600 }, { 880, 620 }, { 880, 530 },
        { 880, 460 }, { 880, 450 }, { 690, 550 }, { 690, 540 }, { 690, 520 }, { 690, 510 }, { 690, 610 },
        { 690, 590 }, { 690, 580 }, { 480, 650 }, { 1000, 650 }, { 290, 340 }, { 810, 340 }
    };

    const int gridPitch = m_schematic.Settings().m_ConnectionGridSize;
    const int pageHeight = screen->GetPageSettings().GetHeightIU( schIUScale.IU_PER_MILS );
    const int flipY = ( pageHeight / gridPitch ) * gridPitch;

    std::set<std::pair<int, int>> expected;

    for( const VECTOR2I& cross : crosses )
        expected.emplace( schIUScale.MilsToIU( cross.x * 10 ), flipY - schIUScale.MilsToIU( cross.y * 10 ) );

    std::set<std::pair<int, int>> actual;

    size_t actualCount = 0;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_NO_CONNECT_T ) )
    {
        ++actualCount;
        actual.emplace( item->GetPosition().x, item->GetPosition().y );
    }

    BOOST_CHECK_EQUAL( actualCount, crosses.size() );
    BOOST_CHECK( actual == expected );
}


// Support ticket #1303: an OrCad-derived Altium sheet symbol references several source files
// through a single semicolon-separated filename ("pagea.SchDoc;pageb.SchDoc"), the two pages of one
// multi-page block. The pages cross-reference each other (and one references itself). Every page
// must be merged into the one sub-sheet screen, and the cyclic cross-references must not create a
// recursive hierarchy that trips SCH_SHEET_LIST::BuildSheetList.
BOOST_AUTO_TEST_CASE( Ticket1303_MultiPageBlock )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( ticket1303DataFile( wxT( "overview.SchDoc" ) ),
                                                     &m_schematic, nullptr, nullptr );
    BOOST_REQUIRE( rootSheet );
    BOOST_REQUIRE( rootSheet->GetScreen() );

    // Walking the hierarchy must not trip the recursion guard in SCH_SHEET_LIST::BuildSheetList.
    // Before the fix this raised a wxASSERT (the pages resolved to sub-sheets whose remapped
    // filenames collided in the ancestry).
    SCH_SHEET_LIST hierarchy = m_schematic.Hierarchy();

    // Both pages of the block land in a single merged sub-sheet screen; a regression drops one (or
    // both), leaving the sheet empty. The screen also holds no residual sub-sheet symbols, since the
    // cross-page references were pruned rather than descended into.
    bool foundMergedScreen = false;

    for( const SCH_SHEET_PATH& sheetPath : hierarchy )
    {
        SCH_SCREEN* screen = sheetPath.LastScreen();

        if( !screen || screen == rootSheet->GetScreen() )
            continue;

        std::optional<VECTOR2I> posA;
        std::optional<VECTOR2I> posB;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_LABEL_T ) )
        {
            SCH_LABEL* label = static_cast<SCH_LABEL*>( item );

            if( label->GetText() == wxT( "ONLY_A" ) )
                posA = label->GetPosition();
            else if( label->GetText() == wxT( "ONLY_B" ) )
                posB = label->GetPosition();
        }

        if( !posA || !posB )
            continue;

        foundMergedScreen = true;

        // The cross-page references were pruned, not descended into: the merged screen holds no
        // residual sub-sheet symbols.
        int subSheetCount = 0;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SHEET_T ) )
        {
            (void) item;
            subSheetCount++;
        }

        BOOST_CHECK_EQUAL( subSheetCount, 0 );

        // Page B is tiled below page A rather than superimposed, even though both labels sit at the
        // same coordinates in their source files.
        BOOST_CHECK_MESSAGE( *posA != *posB, "Merged pages must be tiled, not overlapping" );

        // The pruned cross-reference sheet's pin was converted to a hierarchical label so the wire
        // that terminated on it still connects by name.
        bool foundSigLabel = false;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_HIER_LABEL_T ) )
        {
            if( static_cast<SCH_HIERLABEL*>( item )->GetText() == wxT( "SIG" ) )
                foundSigLabel = true;
        }

        BOOST_CHECK_MESSAGE( foundSigLabel,
                             "Pruned sheet pin must survive as a hierarchical label" );
    }

    BOOST_CHECK_MESSAGE( foundMergedScreen,
                         "Both pages of the multi-file sheet symbol must load into one screen" );
}


// https://gitlab.com/kicad/code/kicad/-/issues/24843
// Altium encodes symbol rotation as quarter turns that map one-for-one onto KiCad's
// SYM_ORIENT_* angles. The importer must store the same angle Altium shows so a later
// "Update Symbols from Library" against a canonical upright symbol does not rotate the
// placement. The reference symbols below are non-mirrored so the stored angle reads back
// directly, without GetOrientation()'s mirror normalization.
BOOST_AUTO_TEST_CASE( Issue24843_SymbolOrientationMatchesAltium )
{
    SCH_IO_ALTIUM plugin;

    // The source embeds images by absolute Windows paths that cannot resolve here
    plugin.SetReporter( &NULL_REPORTER::GetInstance() );

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( eDPAdapterDataFile( "power.SchDoc" ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    m_schematic.RefreshHierarchy();

    // Reference designator -> Altium orientation angle read straight from the source records.
    const std::map<wxString, SYMBOL_ORIENTATION_PROP> expected = {
        { wxT( "L10P" ), SYMBOL_ORIENTATION_PROP::SYMBOL_ANGLE_0 },   // Altium ORIENTATION 0
        { wxT( "C10P" ), SYMBOL_ORIENTATION_PROP::SYMBOL_ANGLE_90 },  // Altium ORIENTATION 1
        { wxT( "R10P" ), SYMBOL_ORIENTATION_PROP::SYMBOL_ANGLE_180 }, // Altium ORIENTATION 2
        { wxT( "C13P" ), SYMBOL_ORIENTATION_PROP::SYMBOL_ANGLE_270 }, // Altium ORIENTATION 3
    };

    std::map<wxString, SYMBOL_ORIENTATION_PROP> actual;

    for( const SCH_SHEET_PATH& sheetPath : m_schematic.Hierarchy() )
    {
        for( SCH_ITEM* item : sheetPath.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );
            wxString    ref = symbol->GetRef( &sheetPath );

            if( expected.count( ref ) )
                actual[ref] = symbol->GetOrientationProp();
        }
    }

    for( const auto& [ref, angle] : expected )
    {
        BOOST_REQUIRE_MESSAGE( actual.count( ref ), "Symbol '" << ref << "' not found on import" );
        BOOST_CHECK_MESSAGE( actual.at( ref ) == angle,
                             "Symbol '" << ref << "' imported with orientation " << actual.at( ref )
                                        << ", expected " << angle );
    }
}


// https://gitlab.com/kicad/code/kicad/-/issues/16903
// Every connectable coordinate in this design sits on a 50 mil grid in Altium's own frame, so the
// importer's Y flip must preserve that grid phase rather than shift the sheet off grid.
BOOST_AUTO_TEST_CASE( Issue16903_ImportKeepsGeometryOnGrid )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet =
            plugin.LoadSchematicFile( issue16903DataFile( wxT( "hierarchical_schematic_top.SchDoc" ) ),
                                      &m_schematic );
    BOOST_REQUIRE( rootSheet );

    m_schematic.RefreshHierarchy();

    ERC_TESTER tester( &m_schematic );
    BOOST_CHECK_EQUAL( tester.TestOffGridEndpoints(), 0 );
}


// https://github.com/reckless-technology/kicad/issues/19
// A sheet is parsed on a path of its own and becomes a sub-sheet afterwards, so a symbol on it must be
// given its instance on the path it ends up on. Otherwise the symbol reads as unannotated there, and
// the automatic net names of its pins are built from its UUID, which differs on every import.
static void checkEverySymbolHasInstanceOnItsPath( SCHEMATIC& aSchematic, size_t aMinimumSubSheetSymbols )
{
    size_t symbolsOnSubSheets = 0;

    for( const SCH_SHEET_PATH& sheetPath : aSchematic.Hierarchy() )
    {
        SCH_SCREEN* screen = sheetPath.LastScreen();
        BOOST_REQUIRE( screen );

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL*         symbol = static_cast<SCH_SYMBOL*>( item );
            SCH_SYMBOL_INSTANCE instance;

            BOOST_CHECK_MESSAGE( symbol->GetInstance( instance, sheetPath.Path() ),
                                 "symbol " << symbol->GetField( FIELD_T::REFERENCE )->GetText()
                                           << " has no instance on " << sheetPath.PathHumanReadable() );

            const wxString reference = symbol->GetRef( &sheetPath );

            if( !reference.StartsWith( wxT( "#" ) ) )
            {
                BOOST_CHECK_MESSAGE( !reference.IsEmpty() && !reference.EndsWith( wxT( "?" ) ),
                                     "symbol reads as unannotated '" << reference << "' on "
                                                                     << sheetPath.PathHumanReadable() );
            }

            if( sheetPath.size() > 1 )
                symbolsOnSubSheets++;
        }
    }

    BOOST_CHECK_GE( symbolsOnSubSheets, aMinimumSubSheetSymbols );
}


BOOST_AUTO_TEST_CASE( SymbolsHaveInstancesOnSubSheets )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet =
            plugin.LoadSchematicFile( issue16903DataFile( wxT( "hierarchical_schematic_top.SchDoc" ) ), &m_schematic );
    BOOST_REQUIRE( rootSheet );

    m_schematic.RefreshHierarchy();

    checkEverySymbolHasInstanceOnItsPath( m_schematic, 1 );
}


// Each of the top sheet's three sheet symbols names a file of its own, so no sub-sheet is repeated
// and none is handled as a reused sheet. Sub-sheets listed before the sheet that shows them are
// parsed on their own first, as a project's documents are.
BOOST_AUTO_TEST_CASE( SymbolsHaveInstancesOnUniqueSubSheets )
{
    SCH_IO_ALTIUM plugin;

    std::map<std::string, UTF8> properties;
    properties.emplace( "project_file", UTF8( issue24861DataFile( wxT( "Repeated_Schematic.PrjPcb" ) ) ) );
    properties.emplace( "sch0", UTF8( issue19DataFile( wxT( "sub_sheet.SchDoc" ) ) ) );
    properties.emplace( "sch1", UTF8( issue19DataFile( wxT( "sub_shee2.SchDoc" ) ) ) );
    properties.emplace( "sch2", UTF8( issue19DataFile( wxT( "sub_shee3.SchDoc" ) ) ) );
    properties.emplace( "sch3", UTF8( issue19DataFile( wxT( "top.SchDoc" ) ) ) );

    BOOST_REQUIRE( plugin.LoadSchematicFile( wxEmptyString, &m_schematic, nullptr, &properties ) );

    checkEverySymbolHasInstanceOnItsPath( m_schematic, 3 );
}


BOOST_AUTO_TEST_CASE( SymbolsHaveInstancesOnProjectSheets )
{
    SCH_IO_ALTIUM plugin;

    std::map<std::string, UTF8> properties;
    properties.emplace( "project_file", UTF8( issue24861DataFile( wxT( "Repeated_Schematic.PrjPcb" ) ) ) );
    properties.emplace( "sch0", UTF8( issue24861DataFile( wxT( "Repeated_Schematic.SchDoc" ) ) ) );
    properties.emplace( "sch1", UTF8( issue24861DataFile( wxT( "Channel.SchDoc" ) ) ) );

    BOOST_REQUIRE( plugin.LoadSchematicFile( wxEmptyString, &m_schematic, nullptr, &properties ) );

    checkEverySymbolHasInstanceOnItsPath( m_schematic, 1 );
}


// https://gitlab.com/kicad/code/kicad/-/issues/26127
// Every text takes its face, style and size from the sheet's font table: free text, net labels,
// ports, power ports and symbol fields alike. Altium draws a font of N points with its ascent
// plus descent N * 10 mil tall, so an outline font's KiCad size comes from the font's own
// metrics. A text naming no font uses font 1.
BOOST_AUTO_TEST_CASE( Issue26127_AltiumFonts )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( issue26127DataFile( wxT( "fonts.SchDoc" ) ), &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    m_schematic.RefreshHierarchy();
    const SCH_SHEET_PATH& sheetPath = m_schematic.Hierarchy().front();

    struct ALTIUM_FONT
    {
        wxString name;
        int      points;
        bool     bold;
        bool     italic;
    };

    // The sheet's font table
    const std::map<int, ALTIUM_FONT> fonts = {
        { 1, { wxT( "Times New Roman" ), 10, false, false } },
        { 2, { wxT( "Arial" ), 12, true, false } },
        { 3, { wxT( "Courier New" ), 8, false, true } },
    };

    std::vector<std::pair<EDA_TEXT*, int>> texts;

    for( SCH_ITEM* item : screen->Items() )
    {
        switch( item->Type() )
        {
        case SCH_TEXT_T:         texts.emplace_back( static_cast<SCH_TEXT*>( item ), 2 );        break;
        case SCH_LABEL_T:        texts.emplace_back( static_cast<SCH_LABEL*>( item ), 3 );       break;
        case SCH_HIER_LABEL_T:   texts.emplace_back( static_cast<SCH_HIERLABEL*>( item ), 3 );   break;
        case SCH_GLOBAL_LABEL_T: texts.emplace_back( static_cast<SCH_GLOBALLABEL*>( item ), 3 ); break;

        case SCH_SYMBOL_T:
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

            if( symbol->IsPower() )
            {
                texts.emplace_back( symbol->GetField( FIELD_T::VALUE ), 2 );
            }
            else
            {
                BOOST_CHECK_EQUAL( symbol->GetRef( &sheetPath ), wxT( "R1" ) );
                texts.emplace_back( symbol->GetField( FIELD_T::REFERENCE ), 3 );
                texts.emplace_back( symbol->GetField( FIELD_T::VALUE ), 2 );
                texts.emplace_back( symbol->GetField( wxT( "ALTIUM_VALUE" ) ), 1 );
            }

            break;
        }

        default:
            break;
        }
    }

    // free text, net label, port, power port name and three fields
    BOOST_REQUIRE_EQUAL( texts.size(), 7 );

    for( const auto& [text, fontId] : texts )
    {
        BOOST_REQUIRE( text );

        const ALTIUM_FONT& font = fonts.at( fontId );

        BOOST_TEST_CONTEXT( text->GetText() )
        {
            BOOST_CHECK_EQUAL( text->GetFontName(), font.name );
            BOOST_CHECK_EQUAL( text->IsBold(), font.bold );
            BOOST_CHECK_EQUAL( text->IsItalic(), font.italic );

            const int cell = schIUScale.MilsToIU( font.points * 10 );
            int       size = cell / 2;

            KIFONT::FONT* kifont = KIFONT::FONT::GetFont( font.name, font.bold, font.italic );

            if( kifont && kifont->IsOutline() )
                size = KiROUND( static_cast<KIFONT::OUTLINE_FONT*>( kifont )->GetTextSizeForAscentPlusDescent( cell ) );

            BOOST_CHECK_EQUAL( text->GetTextHeight(), size );
            BOOST_CHECK_EQUAL( text->GetTextWidth(), size );
        }
    }
}


// https://gitlab.com/kicad/code/kicad/-/issues/26126
// Altium gives a field's text orientation and justification in page coordinates. KiCad stores them
// relative to the symbol and displays them through its rotation and mirroring, so the importer
// must store the values that display as Altium's. Each of the eight symbols in this sheet has one
// of the four rotations, mirrored or not, and three fields whose text orientations and
// justifications between them cover every value. Text Altium turns upside down is drawn upright
// in the same place, so its justification is reversed both ways.
BOOST_AUTO_TEST_CASE( Issue26126_FieldJustification )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile(
            issue26126DataFile( wxT( "field_justification.SchDoc" ) ), &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    struct DISPLAYED
    {
        EDA_ANGLE         angle;
        GR_TEXT_H_ALIGN_T hJustify;
        GR_TEXT_V_ALIGN_T vJustify;
    };

    // Altium text orientation (quarter turns) and justification (0 = bottom left ... 8 = top right)
    auto altiumDisplay =
            []( int aOrientation, int aJustification ) -> DISPLAYED
            {
                static const GR_TEXT_H_ALIGN_T h[] = { GR_TEXT_H_ALIGN_LEFT, GR_TEXT_H_ALIGN_CENTER,
                                                       GR_TEXT_H_ALIGN_RIGHT };
                static const GR_TEXT_V_ALIGN_T v[] = { GR_TEXT_V_ALIGN_BOTTOM, GR_TEXT_V_ALIGN_CENTER,
                                                       GR_TEXT_V_ALIGN_TOP };

                DISPLAYED shown{ aOrientation % 2 ? ANGLE_VERTICAL : ANGLE_HORIZONTAL,
                                 h[aJustification % 3], v[aJustification / 3] };

                if( aOrientation >= 2 )
                {
                    shown.hJustify = h[2 - aJustification % 3];
                    shown.vJustify = v[2 - aJustification / 3];
                }

                return shown;
            };

    m_schematic.RefreshHierarchy();
    const SCH_SHEET_PATH& sheetPath = m_schematic.Hierarchy().front();

    int checked = 0;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
    {
        SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );
        wxString    ref = symbol->GetRef( &sheetPath );
        long        number = 0;

        BOOST_REQUIRE( ref.Mid( 2 ).ToLong( &number ) );

        // Symbol RV(k + 1) has rotation k % 4 and is mirrored when k >= 4. Its field i (0 the
        // reference, 1 the value, 2 FUNCTION) has text orientation (k + i) % 4 and justification
        // (3k + 4i) % 9.
        const int k = static_cast<int>( number ) - 1;

        const std::vector<std::pair<SCH_FIELD*, int>> fields = {
            { symbol->GetField( FIELD_T::REFERENCE ), 0 },
            { symbol->GetField( FIELD_T::VALUE ), 1 },
            { symbol->GetField( wxT( "FUNCTION" ) ), 2 },
        };

        for( const auto& [field, i] : fields )
        {
            BOOST_REQUIRE( field );

            const DISPLAYED expected = altiumDisplay( ( k + i ) % 4, ( 3 * k + 4 * i ) % 9 );

            BOOST_TEST_CONTEXT( ref << " field " << i )
            {
                BOOST_CHECK( field->GetDrawRotation() == expected.angle );
                BOOST_CHECK_EQUAL( field->GetEffectiveHorizJustify(), expected.hJustify );
                BOOST_CHECK_EQUAL( field->GetEffectiveVertJustify(), expected.vJustify );
            }

            checked++;
        }
    }

    BOOST_CHECK_EQUAL( checked, 24 );
}


// https://gitlab.com/kicad/code/kicad/-/issues/26009
// An Altium Parameter Set whose ClassName parameter names a net class assigns that class to the
// net it sits on. Each becomes a net class flag with a Netclass field, pointing the way Altium's
// flag points, and the class is added to the project, inheriting the Default class. A set
// without a ClassName is left out.
BOOST_AUTO_TEST_CASE( Issue26009_ParameterSetsAsNetClassFlags )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( issue26009DataFile( wxT( "parameter_sets.SchDoc" ) ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    // Altium places items in 10 mil units with the Y axis flipped about the page bottom
    const int gridPitch = m_schematic.Settings().m_ConnectionGridSize;
    const int pageHeight = screen->GetPageSettings().GetHeightIU( schIUScale.IU_PER_MILS );
    const int flipY = ( pageHeight / gridPitch ) * gridPitch;

    auto altiumPoint =
            [&]( int aX, int aY )
            {
                return VECTOR2I( schIUScale.MilsToIU( aX * 10 ), flipY - schIUScale.MilsToIU( aY * 10 ) );
            };

    struct EXPECTED
    {
        VECTOR2I   position;
        SPIN_STYLE spin;
        VECTOR2I   textPosition;
    };

    // Texts are drawn in the sheet's system font, 10 point Times New Roman, and moved off Altium's
    // anchor so their glyphs land where Altium draws them (see AltiumTextPlacement)
    auto placed =
            []( const VECTOR2I& aAnchor, bool aField, int aVertical ) -> VECTOR2I
            {
                KIFONT::FONT* font = KIFONT::FONT::GetFont( wxT( "Times New Roman" ), false, false );

                if( !font || !font->IsOutline() )
                    return aAnchor;

                double ascent = 0.0;
                double descent = 0.0;
                static_cast<KIFONT::OUTLINE_FONT*>( font )->GetAscentAndDescent( ascent, descent );

                const double cell = schIUScale.MilsToIU( 100 );
                const double em = cell / ( ascent + descent );
                const double size = em / 1.4;
                const double faceHeight = 1.4 * ( ascent + descent );
                const double edge[3] = { -0.17, 0.415, 1.0 };
                const double beta = aField ? 0.415 + 0.33 * faceHeight * ( aVertical - 1 )
                                           : edge[aVertical] - 0.4 * ( faceHeight - 1.0 )
                                                     - schIUScale.mmToIU( 0.25 ) / size;
                const double up = em * descent - cell * aVertical / 2.0 + beta * size;

                return VECTOR2I( aAnchor.x, KiROUND( aAnchor.y - up ) );
            };

    // The flag on the upper wire points up (Altium orientation 1), its text bottom centre above
    // it; the other points right (0), its text centre left beside it
    std::map<wxString, EXPECTED> expected = {
        { wxT( "HT" ), { altiumPoint( 200, 400 ), SPIN_STYLE::RIGHT, placed( altiumPoint( 200, 420 ), true, 0 ) } },
        { wxT( "H" ), { altiumPoint( 300, 200 ), SPIN_STYLE::UP, placed( altiumPoint( 320, 200 ), true, 1 ) } },
    };

    std::map<wxString, SCH_DIRECTIVE_LABEL*> flags;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_DIRECTIVE_LABEL_T ) )
    {
        SCH_DIRECTIVE_LABEL* flag = static_cast<SCH_DIRECTIVE_LABEL*>( item );

        for( SCH_FIELD& field : flag->GetFields() )
        {
            if( field.GetUntranslatedName() == wxT( "Netclass" ) )
            {
                flags[field.GetText()] = flag;

                BOOST_TEST_CONTEXT( field.GetText() )
                {
                    BOOST_REQUIRE( expected.count( field.GetText() ) );
                    BOOST_CHECK( field.IsVisible() );
                    BOOST_CHECK_EQUAL( field.GetPosition(), expected[field.GetText()].textPosition );
                }
            }
        }
    }

    BOOST_REQUIRE_EQUAL( flags.size(), expected.size() );

    // Altium's circle holding an "i", drawn at the end of each flag's stem
    std::set<std::pair<int, int>> circles;
    std::set<std::pair<int, int>> infos;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_SHAPE_T ) )
    {
        SCH_SHAPE* shape = static_cast<SCH_SHAPE*>( item );

        if( shape->GetShape() == SHAPE_T::CIRCLE )
        {
            BOOST_CHECK_EQUAL( shape->GetRadius(), schIUScale.MilsToIU( 60 ) );
            circles.emplace( shape->GetCenter().x, shape->GetCenter().y );
        }
    }

    for( SCH_ITEM* item : screen->Items().OfType( SCH_TEXT_T ) )
    {
        if( static_cast<SCH_TEXT*>( item )->GetText() == wxT( "i" ) )
            infos.emplace( item->GetPosition().x, item->GetPosition().y );
    }

    const VECTOR2I htCenter = altiumPoint( 200, 412 );
    const VECTOR2I hCenter = altiumPoint( 312, 200 );
    const std::set<std::pair<int, int>> centers = { { htCenter.x, htCenter.y }, { hCenter.x, hCenter.y } };

    BOOST_CHECK( circles == centers );

    // The "i" is centred in its circle, placed as free text
    const VECTOR2I htInfo = placed( htCenter, false, 1 );
    const VECTOR2I hInfo = placed( hCenter, false, 1 );

    const std::set<std::pair<int, int>> infoPositions = { { htInfo.x, htInfo.y }, { hInfo.x, hInfo.y } };

    BOOST_CHECK( infos == infoPositions );

    for( const auto& [name, want] : expected )
    {
        BOOST_TEST_CONTEXT( name )
        {
            BOOST_CHECK_EQUAL( flags[name]->GetPosition(), want.position );
            BOOST_CHECK( flags[name]->GetSpinStyle() == want.spin );
            BOOST_CHECK( flags[name]->GetShape() == LABEL_FLAG_SHAPE::F_ROUND );
            BOOST_CHECK_EQUAL( flags[name]->GetPinLength(), schIUScale.MilsToIU( 60 ) );
        }
    }

    // The classes are added to the project, taking the Default class's settings until the board
    // or its rules give them their own
    std::shared_ptr<NET_SETTINGS>& netSettings = m_schematic.Project().GetProjectFile().NetSettings();

    for( const wxString& name : { wxT( "H" ), wxT( "HT" ) } )
    {
        BOOST_TEST_CONTEXT( name )
        {
            BOOST_REQUIRE( netSettings->HasNetclass( name ) );

            std::shared_ptr<NETCLASS> netclass = netSettings->GetNetclasses().at( name );
            BOOST_CHECK( !netclass->HasClearance() );
            BOOST_CHECK( !netclass->HasWireWidth() );
            BOOST_CHECK( !netclass->HasTrackWidth() );
        }
    }

    BOOST_CHECK_EQUAL( netSettings->GetNetclasses().size(), 2 );
}


// Altium anchors a text at a corner, edge or centre of the cell from the font's descender line to
// its ascender line. KiCad draws the baseline a distance from its anchor that depends on the kind
// of text, its vertical justification and the face's height (measured on plotted KiCad text).
// The importer must move each anchor so the glyphs land where Altium draws them, draw upside
// down text upright in the same cell, and turn a right-justified net label to read towards its
// anchor.
BOOST_AUTO_TEST_CASE( AltiumTextPlacement )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( textPlacementDataFile( wxT( "placement.SchDoc" ) ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    const int gridPitch = m_schematic.Settings().m_ConnectionGridSize;
    const int pageHeight = screen->GetPageSettings().GetHeightIU( schIUScale.IU_PER_MILS );
    const int flipY = ( pageHeight / gridPitch ) * gridPitch;

    auto altiumPoint =
            [&]( int aX, int aY )
            {
                return VECTOR2D( schIUScale.MilsToIU( aX * 10 ), flipY - schIUScale.MilsToIU( aY * 10 ) );
            };

    // The rule, restated: how far the baseline sits above Altium's anchor and below KiCad's
    auto expectedAnchor =
            []( bool aField, const VECTOR2D& aAltiumAnchor, const wxString& aFace, int aPoints, int aVertical,
                bool aReadsUp ) -> VECTOR2I
            {
                KIFONT::FONT* font = KIFONT::FONT::GetFont( aFace, false, false );
                BOOST_REQUIRE( font && font->IsOutline() );

                double ascent = 0.0;
                double descent = 0.0;
                static_cast<KIFONT::OUTLINE_FONT*>( font )->GetAscentAndDescent( ascent, descent );

                const double cell = schIUScale.MilsToIU( aPoints * 10 );
                const double em = cell / ( ascent + descent );
                const double size = em / 1.4;
                const double faceHeight = 1.4 * ( ascent + descent );
                const double baselineUp = em * descent - cell * aVertical / 2.0;
                const double edge[3] = { -0.17, 0.415, 1.0 };
                const double beta = aField ? 0.415 + 0.33 * faceHeight * ( aVertical - 1 )
                                           : edge[aVertical] - 0.4 * ( faceHeight - 1.0 )
                                                     - schIUScale.mmToIU( 0.25 ) / size;
                const double up = baselineUp + beta * size;

                VECTOR2D anchor = aAltiumAnchor;

                if( aReadsUp )
                {
                    anchor.x -= up;

                    if( !aField )
                    {
                        anchor.x -= schIUScale.mmToIU( 0.25 );
                        anchor.y += schIUScale.mmToIU( 0.25 );
                    }
                }
                else
                {
                    anchor.y -= up;
                }

                return VECTOR2I( KiROUND( anchor.x ), KiROUND( anchor.y ) );
            };

    std::map<wxString, SCH_TEXT*> texts;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_TEXT_T ) )
        texts[static_cast<SCH_TEXT*>( item )->GetText()] = static_cast<SCH_TEXT*>( item );

    BOOST_REQUIRE_EQUAL( texts.size(), 4 );

    SCH_TEXT* bottomLeft = texts.at( wxT( "BOTTOM LEFT" ) );
    BOOST_CHECK_EQUAL( bottomLeft->GetPosition(),
                       expectedAnchor( false, altiumPoint( 100, 700 ), wxT( "Arial" ), 12, 0, false ) );
    BOOST_CHECK( bottomLeft->GetTextAngle() == ANGLE_HORIZONTAL );
    BOOST_CHECK_EQUAL( bottomLeft->GetHorizJustify(), GR_TEXT_H_ALIGN_LEFT );
    BOOST_CHECK_EQUAL( bottomLeft->GetVertJustify(), GR_TEXT_V_ALIGN_BOTTOM );

    SCH_TEXT* topCentreUp = texts.at( wxT( "TOP CENTRE UP" ) );
    BOOST_CHECK_EQUAL( topCentreUp->GetPosition(),
                       expectedAnchor( false, altiumPoint( 300, 700 ), wxT( "Arial" ), 12, 2, true ) );
    BOOST_CHECK( topCentreUp->GetTextAngle() == ANGLE_VERTICAL );
    BOOST_CHECK_EQUAL( topCentreUp->GetHorizJustify(), GR_TEXT_H_ALIGN_CENTER );
    BOOST_CHECK_EQUAL( topCentreUp->GetVertJustify(), GR_TEXT_V_ALIGN_TOP );

    // Altium's bottom left of upside-down text is the top right of the same cell drawn upright
    SCH_TEXT* upsideDown = texts.at( wxT( "UPSIDE DOWN" ) );
    BOOST_CHECK_EQUAL( upsideDown->GetPosition(),
                       expectedAnchor( false, altiumPoint( 500, 700 ), wxT( "Times New Roman" ), 10, 2, false ) );
    BOOST_CHECK( upsideDown->GetTextAngle() == ANGLE_HORIZONTAL );
    BOOST_CHECK_EQUAL( upsideDown->GetHorizJustify(), GR_TEXT_H_ALIGN_RIGHT );
    BOOST_CHECK_EQUAL( upsideDown->GetVertJustify(), GR_TEXT_V_ALIGN_TOP );

    // Bold Cambria Math, a face with no bold weight, is drawn in Cambria's bold, sized by Cambria
    // Math's cell and squeezed to the width of Altium's synthetic bold
    SCH_TEXT* bold = texts.at( wxT( "GAIN" ) );
    BOOST_CHECK_EQUAL( bold->GetFontName(), wxT( "Cambria" ) );
    BOOST_CHECK( bold->IsBold() );

    {
        auto outline = []( const wxString& aFace, bool aBold )
        {
            return static_cast<KIFONT::OUTLINE_FONT*>( KIFONT::FONT::GetFont( aFace, aBold, false ) );
        };

        const int    cell = schIUScale.MilsToIU( 120 );
        const int    size = KiROUND( outline( wxT( "Cambria Math" ), true )->GetTextSizeForAscentPlusDescent( cell ) );
        const double wanted = outline( wxT( "Cambria Math" ), false )->GetAdvanceInEms( wxT( "GAIN" ) ) + 0.0111 * 4;
        const double have = outline( wxT( "Cambria" ), true )->GetAdvanceInEms( wxT( "GAIN" ) );

        BOOST_CHECK_EQUAL( bold->GetTextHeight(), size );
        BOOST_CHECK_EQUAL( bold->GetTextWidth(), KiROUND( size * wanted / have ) );
    }

    // A right-justified net label reads towards its anchor, which stays on the wire
    std::vector<SCH_LABEL*> labels;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_LABEL_T ) )
        labels.push_back( static_cast<SCH_LABEL*>( item ) );

    BOOST_REQUIRE_EQUAL( labels.size(), 1 );
    BOOST_CHECK_EQUAL( VECTOR2D( labels.front()->GetPosition() ), altiumPoint( 300, 500 ) );
    BOOST_CHECK( labels.front()->GetSpinStyle() == SPIN_STYLE::LEFT );

    // The designator of a symbol turned a quarter turn: a field, centre left, horizontal
    std::vector<SCH_SYMBOL*> symbols;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
        symbols.push_back( static_cast<SCH_SYMBOL*>( item ) );

    BOOST_REQUIRE_EQUAL( symbols.size(), 1 );

    SCH_FIELD* reference = symbols.front()->GetField( FIELD_T::REFERENCE );
    BOOST_CHECK_EQUAL( reference->GetPosition(),
                       expectedAnchor( true, altiumPoint( 630, 330 ), wxT( "Arial" ), 12, 1, false ) );
    BOOST_CHECK( reference->GetDrawRotation() == ANGLE_HORIZONTAL );
    BOOST_CHECK_EQUAL( reference->GetEffectiveHorizJustify(), GR_TEXT_H_ALIGN_LEFT );
    BOOST_CHECK_EQUAL( reference->GetEffectiveVertJustify(), GR_TEXT_V_ALIGN_CENTER );
}


// Altium draws pin names and numbers in the sheet's system font, here font 2, 12 point Arial, and
// a pin name 70 mil inside the body's edge. A pin whose name and number Altium hides keeps them
// hidden.
BOOST_AUTO_TEST_CASE( AltiumPinText )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( pinTextDataFile( wxT( "pin_text.SchDoc" ) ), &m_schematic );
    BOOST_REQUIRE( rootSheet );

    std::vector<SCH_SYMBOL*> symbols;

    for( SCH_ITEM* item : rootSheet->GetScreen()->Items().OfType( SCH_SYMBOL_T ) )
        symbols.push_back( static_cast<SCH_SYMBOL*>( item ) );

    BOOST_REQUIRE_EQUAL( symbols.size(), 1 );

    LIB_SYMBOL* libSymbol = symbols.front()->GetLibSymbolRef().get();
    BOOST_REQUIRE( libSymbol );
    BOOST_CHECK_EQUAL( libSymbol->GetPinNameOffset(), schIUScale.MilsToIU( 70 ) );

    int size = schIUScale.MilsToIU( 120 ) / 2;
    KIFONT::FONT* font = KIFONT::FONT::GetFont( wxT( "Arial" ), false, false );

    if( font && font->IsOutline() )
        size = KiROUND( static_cast<KIFONT::OUTLINE_FONT*>( font )->GetTextSizeForAscentPlusDescent( schIUScale.MilsToIU( 120 ) ) );

    int pins = 0;

    for( SCH_PIN* pin : libSymbol->GetGraphicalPins( 0, 0 ) )
    {
        BOOST_TEST_CONTEXT( pin->GetNumber() )
        {
            if( pin->GetNumber() == wxT( "3" ) )
            {
                BOOST_CHECK_EQUAL( pin->GetNameTextSize(), 0 );
                BOOST_CHECK_EQUAL( pin->GetNumberTextSize(), 0 );
            }
            else
            {
                BOOST_CHECK_EQUAL( pin->GetNameTextSize(), size );
                BOOST_CHECK_EQUAL( pin->GetNumberTextSize(), size );
            }
        }

        pins++;
    }

    BOOST_CHECK_EQUAL( pins, 3 );
}


// A symbol's graphics keep Altium's line width (one of four: 1, 10, 30 or 50 mil), line colour
// (blue by default) and fill: a solid shape is filled with its area colour (white by default)
// unless Altium draws it transparent, and an open one is not filled.
BOOST_AUTO_TEST_CASE( AltiumSymbolGraphicsStyle )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( symbolGraphicsDataFile( wxT( "symbol_graphics.SchDoc" ) ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    std::vector<SCH_SYMBOL*> symbols;

    for( SCH_ITEM* item : rootSheet->GetScreen()->Items().OfType( SCH_SYMBOL_T ) )
        symbols.push_back( static_cast<SCH_SYMBOL*>( item ) );

    BOOST_REQUIRE_EQUAL( symbols.size(), 1 );
    BOOST_REQUIRE( symbols.front()->GetLibSymbolRef() );

    auto altiumColour = []( int aRed, int aGreen, int aBlue )
    {
        return COLOR4D( aRed / 255.0, aGreen / 255.0, aBlue / 255.0, 1.0 );
    };

    int rectangles = 0;

    for( SCH_ITEM& item : symbols.front()->GetLibSymbolRef()->GetDrawItems() )
    {
        if( item.Type() != SCH_SHAPE_T )
            continue;

        SCH_SHAPE& shape = static_cast<SCH_SHAPE&>( item );

        switch( shape.GetShape() )
        {
        case SHAPE_T::RECTANGLE:
            if( shape.GetStroke().GetWidth() == schIUScale.MilsToIU( 10 ) )
            {
                BOOST_CHECK_EQUAL( shape.GetStroke().GetColor(), altiumColour( 128, 0, 0 ) );
                BOOST_CHECK( shape.GetFillMode() == FILL_T::FILLED_WITH_COLOR );
                BOOST_CHECK_EQUAL( shape.GetFillColor(), altiumColour( 255, 255, 176 ) );
            }
            else
            {
                BOOST_CHECK_EQUAL( shape.GetStroke().GetWidth(), schIUScale.MilsToIU( 50 ) );
                BOOST_CHECK_EQUAL( shape.GetStroke().GetColor(), altiumColour( 0, 0, 255 ) );
                BOOST_CHECK( shape.GetFillMode() == FILL_T::FILLED_WITH_COLOR );
                BOOST_CHECK_EQUAL( shape.GetFillColor(), altiumColour( 255, 255, 255 ) );
            }

            rectangles++;
            break;

        case SHAPE_T::POLY:
            BOOST_CHECK_EQUAL( shape.GetStroke().GetWidth(), schIUScale.MilsToIU( 30 ) );
            BOOST_CHECK_EQUAL( shape.GetStroke().GetColor(), altiumColour( 0, 128, 0 ) );
            BOOST_CHECK( shape.GetFillMode() == FILL_T::NO_FILL );
            break;

        case SHAPE_T::CIRCLE:
            BOOST_CHECK_EQUAL( shape.GetStroke().GetWidth(), schIUScale.MilsToIU( 1 ) );
            BOOST_CHECK_EQUAL( shape.GetStroke().GetColor(), altiumColour( 0, 0, 255 ) );
            BOOST_CHECK( shape.GetFillMode() == FILL_T::NO_FILL );
            break;

        default:
            BOOST_FAIL( "unexpected shape" );
        }
    }

    BOOST_CHECK_EQUAL( rectangles, 2 );
}


// A symbol read from an Altium library takes the library's font table as a placed symbol takes
// its sheet's: here the designator is in font 2, 9 point Consolas, Altium's dark blue, bottom left
// justified at (11596.8504, 5249.6080) mil, and placed so its glyphs land where Altium draws them.
BOOST_AUTO_TEST_CASE( AltiumLibraryText )
{
    SCH_IO_ALTIUM plugin;

    std::vector<LIB_SYMBOL*> symbols;
    plugin.EnumerateSymbolLib( symbols, dataFile( "mounting_holes.SchLib" ) );

    LIB_SYMBOL* symbol = nullptr;

    for( LIB_SYMBOL* candidate : symbols )
    {
        if( candidate->GetName() == wxT( "MH M2.5 Mounting hole" ) )
            symbol = candidate;
    }

    BOOST_REQUIRE( symbol );

    SCH_FIELD& reference = symbol->GetReferenceField();
    BOOST_CHECK_EQUAL( reference.GetFontName(), wxT( "Consolas" ) );
    BOOST_CHECK_EQUAL( reference.GetTextColor(), COLOR4D( 0.0, 0.0, 128.0 / 255.0, 1.0 ) );

    KIFONT::FONT* font = KIFONT::FONT::GetFont( wxT( "Consolas" ), false, false );
    BOOST_REQUIRE( font && font->IsOutline() );

    double ascent = 0.0;
    double descent = 0.0;
    static_cast<KIFONT::OUTLINE_FONT*>( font )->GetAscentAndDescent( ascent, descent );

    const double cell = schIUScale.MilsToIU( 90 );
    const double em = cell / ( ascent + descent );
    const double size = em / 1.4;
    const double up = em * descent + ( 0.415 - 0.33 * 1.4 * ( ascent + descent ) ) * size;

    BOOST_CHECK_EQUAL( reference.GetTextHeight(), KiROUND( size ) );
    BOOST_CHECK_EQUAL( reference.GetTextPos(),
                       VECTOR2I( KiROUND( 11596.8504 * schIUScale.IU_PER_MILS ),
                                 KiROUND( -5249.6080 * schIUScale.IU_PER_MILS - up ) ) );
}


// https://gitlab.com/kicad/code/kicad/-/issues/26130
// Altium joins every point on a wire where three or more connections meet and draws the junction
// itself, storing none in the file. In this sheet a vertical wire is met mid-span by a resistor's
// pin, by a wire ending on it, and by two collinear wires ending on it from either side; a fourth
// wire crosses it without ending and a polyline turns a corner. The import must add the three
// junctions Altium draws, and only those, so that each joins the vertical wire's net.
BOOST_AUTO_TEST_CASE( Issue26130_AutomaticJunctions )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( issue26130DataFile( wxT( "auto_junctions.SchDoc" ) ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    // Altium places items in 10 mil units with the Y axis flipped about the page bottom
    const int gridPitch = m_schematic.Settings().m_ConnectionGridSize;
    const int pageHeight = screen->GetPageSettings().GetHeightIU( schIUScale.IU_PER_MILS );
    const int flipY = ( pageHeight / gridPitch ) * gridPitch;

    auto altiumPoint =
            [&]( int aX, int aY )
            {
                return VECTOR2I( schIUScale.MilsToIU( aX * 10 ), flipY - schIUScale.MilsToIU( aY * 10 ) );
            };

    const VECTOR2I pinOnWire = altiumPoint( 300, 400 );
    const VECTOR2I wireOnWire = altiumPoint( 300, 300 );
    const VECTOR2I twoWiresOnWire = altiumPoint( 300, 500 );
    const VECTOR2I crossing = altiumPoint( 300, 550 );

    std::set<std::pair<int, int>> junctions;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_JUNCTION_T ) )
    {
        junctions.emplace( item->GetPosition().x, item->GetPosition().y );

        // Altium's dot is 60 mil across
        BOOST_CHECK_EQUAL( static_cast<SCH_JUNCTION*>( item )->GetDiameter(), schIUScale.MilsToIU( 60 ) );
    }

    const std::set<std::pair<int, int>> expected = { { pinOnWire.x, pinOnWire.y },
                                                     { wireOnWire.x, wireOnWire.y },
                                                     { twoWiresOnWire.x, twoWiresOnWire.y } };

    BOOST_CHECK( junctions == expected );

    m_schematic.RefreshHierarchy();

    CONNECTION_GRAPH graph;
    graph.SetSchematic( &m_schematic );
    graph.Recalculate( m_schematic.BuildSheetListSortedByPageNumbers(), true );

    SCH_ITEM* junctionAtPin = screen->GetItem( pinOnWire, 0, SCH_JUNCTION_T );
    BOOST_REQUIRE( junctionAtPin );

    CONNECTION_SUBGRAPH* netSubgraph = graph.GetSubgraphForItem( junctionAtPin );
    BOOST_REQUIRE( netSubgraph );
    const wxString netName = netSubgraph->GetNetName();

    auto netOf =
            [&]( SCH_ITEM* aItem ) -> wxString
            {
                CONNECTION_SUBGRAPH* subgraph = graph.GetSubgraphForItem( aItem );
                return subgraph ? subgraph->GetNetName() : wxString();
            };

    // The resistor's pin 2 ends on the vertical wire
    SCH_PIN* pin2 = nullptr;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_SYMBOL_T ) )
    {
        for( SCH_PIN* pin : static_cast<SCH_SYMBOL*>( item )->GetPins( &m_schematic.Hierarchy().front() ) )
        {
            if( pin->GetNumber() == wxT( "2" ) )
                pin2 = pin;
        }
    }

    BOOST_REQUIRE( pin2 );
    BOOST_CHECK_EQUAL( pin2->GetPosition(), pinOnWire );
    BOOST_CHECK_EQUAL( netOf( pin2 ), netName );

    // Every wire meeting the vertical wire at a junction is on its net; the crossing wire is not
    for( SCH_ITEM* item : screen->Items().OfType( SCH_LINE_T ) )
    {
        SCH_LINE* line = static_cast<SCH_LINE*>( item );

        if( !line->IsWire() )
            continue;

        bool meetsJunction = false;

        for( const VECTOR2I& point : { pinOnWire, wireOnWire, twoWiresOnWire } )
        {
            if( line->IsEndPoint( point ) )
                meetsJunction = true;
        }

        if( meetsJunction )
            BOOST_CHECK_EQUAL( netOf( line ), netName );
        else if( line->GetStartPoint().y == crossing.y )
            BOOST_CHECK( netOf( line ) != netName );
    }
}


// https://gitlab.com/kicad/code/kicad/-/issues/26131
// This project has Altium's Automatic net identifier scope and neither sheet entries nor ports,
// so Altium connects same-named net labels on its two sheets. The four touchscreen nets are
// labelled on both and must become global labels, joining JP10C on one sheet to the connectors
// on the other. Labels used on one sheet only stay local.
BOOST_AUTO_TEST_CASE( Issue26131_NetLabelsJoinAcrossSheets )
{
    SCH_IO_ALTIUM plugin;

    std::map<std::string, UTF8> properties;
    properties.emplace( "project_file", UTF8( eDPAdapterDataFile( wxT( "eDP_adapter_dvt1.PrjPcb" ) ) ) );
    properties.emplace( "sch0", UTF8( eDPAdapterDataFile( wxT( "it6251_core.SchDoc" ) ) ) );
    properties.emplace( "sch1", UTF8( eDPAdapterDataFile( wxT( "power.SchDoc" ) ) ) );

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( wxEmptyString, &m_schematic, nullptr, &properties );
    BOOST_REQUIRE( rootSheet );

    const std::set<wxString> shared = { wxT( "LCD_XM" ), wxT( "LCD_XP" ), wxT( "LCD_YM" ), wxT( "LCD_YP" ) };

    std::map<wxString, int>                     globalLabels;
    std::set<std::pair<wxString, std::string>>  globalStyles;
    std::set<std::pair<wxString, std::string>>  localStyles;
    int                                         localLabels = 0;

    auto styleOf = []( const SCH_LABEL_BASE* aLabel )
    {
        wxString face = aLabel->GetFont() ? aLabel->GetFont()->GetName() : wxString();
        return std::make_pair( face, aLabel->GetTextColor().ToCSSString().ToStdString() );
    };

    for( const SCH_SHEET_PATH& sheetPath : m_schematic.Hierarchy() )
    {
        for( SCH_ITEM* item : sheetPath.LastScreen()->Items().OfType( SCH_GLOBAL_LABEL_T ) )
        {
            globalLabels[static_cast<SCH_LABEL_BASE*>( item )->GetText()]++;
            globalStyles.insert( styleOf( static_cast<SCH_LABEL_BASE*>( item ) ) );
        }

        for( SCH_ITEM* item : sheetPath.LastScreen()->Items().OfType( SCH_LABEL_T ) )
        {
            BOOST_CHECK( !shared.count( static_cast<SCH_LABEL_BASE*>( item )->GetText() ) );
            localStyles.insert( styleOf( static_cast<SCH_LABEL_BASE*>( item ) ) );
            localLabels++;
        }
    }

    // A label made global keeps the font and colour it had as a net label
    BOOST_REQUIRE( !globalStyles.empty() );

    for( const auto& [face, colour] : globalStyles )
    {
        BOOST_CHECK( !face.IsEmpty() );
        BOOST_CHECK( localStyles.count( { face, colour } ) );
    }

    BOOST_CHECK_EQUAL( globalLabels.size(), shared.size() );

    for( const wxString& name : shared )
        BOOST_CHECK_EQUAL( globalLabels[name], 2 );

    BOOST_CHECK_EQUAL( localLabels, 82 );

    // Global labels' frames get a tight margin, as Altium's labels sit close together
    BOOST_CHECK_EQUAL( m_schematic.Settings().m_LabelSizeRatio, 0.1 );

    // JP10C pin 52 on one sheet and C26P pin 2 on the other are both on LCD_XM
    CONNECTION_GRAPH graph;
    graph.SetSchematic( &m_schematic );
    graph.Recalculate( m_schematic.BuildSheetListSortedByPageNumbers(), true );

    std::map<wxString, wxString> pinNets;

    for( const SCH_SHEET_PATH& sheetPath : m_schematic.Hierarchy() )
    {
        for( SCH_ITEM* item : sheetPath.LastScreen()->Items().OfType( SCH_SYMBOL_T ) )
        {
            SCH_SYMBOL* symbol = static_cast<SCH_SYMBOL*>( item );

            for( SCH_PIN* pin : symbol->GetPins( &sheetPath ) )
            {
                wxString key = symbol->GetRef( &sheetPath ) + wxT( "." ) + pin->GetNumber();

                if( CONNECTION_SUBGRAPH* subgraph = graph.GetSubgraphForItem( pin ) )
                    pinNets[key] = subgraph->GetNetName();
            }
        }
    }

    BOOST_REQUIRE( pinNets.count( wxT( "JP10C.52" ) ) );
    BOOST_REQUIRE( pinNets.count( wxT( "C26P.2" ) ) );
    BOOST_CHECK_EQUAL( pinNets[wxT( "JP10C.52" )], wxT( "LCD_XM" ) );
    BOOST_CHECK_EQUAL( pinNets[wxT( "C26P.2" )], wxT( "LCD_XM" ) );
}


// https://gitlab.com/kicad/code/kicad/-/issues/26128
// Altium's standard sheet styles are its own sizes: its "A3" is 1550 x 1110 in 10 mil units,
// not ISO A3, and the drawing fills it. The title block carries the document's own date, here
// its DocumentDate parameter, and the template's =CurrentDate shows that date rather than the
// day the import is opened.
BOOST_AUTO_TEST_CASE( Issue26128_AltiumSheetSizeAndDate )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( issue26128DataFile( wxT( "sheet_template.SchDoc" ) ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    const PAGE_INFO& page = screen->GetPageSettings();
    BOOST_CHECK( page.GetType() == PAGE_SIZE_TYPE::User );
    BOOST_CHECK_EQUAL( page.GetWidthMils(), 15500 );
    BOOST_CHECK_EQUAL( page.GetHeightMils(), 11100 );

    // Altium's y is flipped about the sheet's own height
    std::vector<SCH_LINE*> wires;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_LINE_T ) )
        wires.push_back( static_cast<SCH_LINE*>( item ) );

    BOOST_REQUIRE_EQUAL( wires.size(), 1 );
    BOOST_CHECK_EQUAL( wires.front()->GetStartPoint().y, schIUScale.MilsToIU( 11100 - 10000 ) );

    BOOST_CHECK_EQUAL( screen->GetTitleBlock().GetDate(), wxT( "4/22/2024" ) );

    std::vector<SCH_TEXT*> texts;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_TEXT_T ) )
        texts.push_back( static_cast<SCH_TEXT*>( item ) );

    BOOST_REQUIRE_EQUAL( texts.size(), 1 );
    BOOST_CHECK_EQUAL( texts.front()->GetText(), wxT( "${ISSUE_DATE}" ) );
}


// Altium gives each wire one of four line widths (Smallest, Small, Medium, Large: 1, 10, 30 and
// 50 mil) and a colour, dark blue by default. The importer must keep both.
BOOST_AUTO_TEST_CASE( AltiumWireWidthsAndColours )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( wireStyleDataFile( wxT( "wire_style.SchDoc" ) ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    // Altium y, in 10 mil units, to width in mil and colour
    const std::map<int, std::pair<int, COLOR4D>> expected = {
        { 100, { 1, COLOR4D( 1.0, 0.0, 0.0, 1.0 ) } },
        { 200, { 10, COLOR4D( 0.0, 0.0, 128.0 / 255.0, 1.0 ) } },
        { 300, { 30, COLOR4D( 0.0, 128.0 / 255.0, 0.0, 1.0 ) } },
        { 400, { 50, COLOR4D( 0.0, 0.0, 1.0, 1.0 ) } },
    };

    const int gridPitch = m_schematic.Settings().m_ConnectionGridSize;
    const int pageHeight = screen->GetPageSettings().GetHeightIU( schIUScale.IU_PER_MILS );
    const int flipY = ( pageHeight / gridPitch ) * gridPitch;

    int checked = 0;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_LINE_T ) )
    {
        SCH_LINE* wire = static_cast<SCH_LINE*>( item );
        int       altiumY = schIUScale.IUToMils( flipY - wire->GetStartPoint().y ) / 10;

        BOOST_TEST_CONTEXT( "wire at Altium y " << altiumY )
        {
            BOOST_REQUIRE( expected.count( altiumY ) );

            const auto& [widthMils, colour] = expected.at( altiumY );
            BOOST_CHECK_EQUAL( wire->GetStroke().GetWidth(), schIUScale.MilsToIU( widthMils ) );
            BOOST_CHECK_EQUAL( wire->GetStroke().GetColor(), colour );
        }

        checked++;
    }

    BOOST_CHECK_EQUAL( checked, 4 );
}


// An Altium text frame keeps its box with the text flowing from the top, its text margin on every
// side, its border's width (one of Altium's four) and colour when it shows one, and its fill,
// white by default, when it is solid.
BOOST_AUTO_TEST_CASE( AltiumTextFrames )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( textFramesDataFile( wxT( "text_frames.SchDoc" ) ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    std::map<wxString, SCH_TEXTBOX*> boxes;

    for( SCH_ITEM* item : rootSheet->GetScreen()->Items().OfType( SCH_TEXTBOX_T ) )
        boxes[static_cast<SCH_TEXTBOX*>( item )->GetText()] = static_cast<SCH_TEXTBOX*>( item );

    BOOST_REQUIRE_EQUAL( boxes.size(), 2 );

    SCH_TEXTBOX* boxed = boxes.at( wxT( "Boxed note" ) );
    BOOST_CHECK( boxed->GetStart().x < boxed->GetEnd().x );
    BOOST_CHECK( boxed->GetStart().y < boxed->GetEnd().y );
    BOOST_CHECK_EQUAL( boxed->GetVertJustify(), GR_TEXT_V_ALIGN_TOP );
    BOOST_CHECK_EQUAL( boxed->GetMarginLeft(), schIUScale.MilsToIU( 5 ) );
    BOOST_CHECK_EQUAL( boxed->GetMarginTop(), schIUScale.MilsToIU( 5 ) );
    BOOST_CHECK_EQUAL( boxed->GetStroke().GetWidth(), schIUScale.MilsToIU( 30 ) );
    BOOST_CHECK_EQUAL( boxed->GetStroke().GetColor(), COLOR4D( 128 / 255.0, 0.0, 0.0, 1.0 ) );
    BOOST_CHECK( boxed->GetFillMode() == FILL_T::FILLED_WITH_COLOR );
    BOOST_CHECK_EQUAL( boxed->GetFillColor(), COLOR4D( 1.0, 1.0, 1.0, 1.0 ) );

    SCH_TEXTBOX* plain = boxes.at( wxT( "Plain note" ) );
    BOOST_CHECK( plain->GetStroke().GetWidth() < 0 );
    BOOST_CHECK( !plain->IsSolidFill() );
}


// https://gitlab.com/kicad/code/kicad/-/issues/26128
// Altium draws a sheet's border and zone references itself and its template's lines and texts
// on every sheet; none of it is part of the drawing. They become the schematic's drawing sheet,
// embedded in it and set for the project, rather than items pasted onto the sheet. Altium's
// special strings become the title block's variables, and the title block is filled from the
// sheet's parameters.
BOOST_AUTO_TEST_CASE( Issue26128_TemplateAsDrawingSheet )
{
    SCH_IO_ALTIUM plugin;

    SCH_SHEET* rootSheet = plugin.LoadSchematicFile( issue26128DataFile( wxT( "drawing_sheet.SchDoc" ) ),
                                                     &m_schematic );
    BOOST_REQUIRE( rootSheet );

    SCH_SCREEN* screen = rootSheet->GetScreen();
    BOOST_REQUIRE( screen );

    // Only the sheet's own text is on the sheet
    std::vector<SCH_TEXT*> texts;

    for( SCH_ITEM* item : screen->Items().OfType( SCH_TEXT_T ) )
        texts.push_back( static_cast<SCH_TEXT*>( item ) );

    BOOST_REQUIRE_EQUAL( texts.size(), 1 );
    BOOST_CHECK_EQUAL( texts.front()->GetText(), wxT( "Free text" ) );
    BOOST_CHECK( screen->Items().OfType( SCH_LINE_T ).empty() );
    BOOST_CHECK( screen->Items().OfType( SCH_SHAPE_T ).empty() );

    // The title block takes the sheet's parameters, in the fields the special strings name
    const TITLE_BLOCK& titleBlock = screen->GetTitleBlock();
    BOOST_CHECK_EQUAL( titleBlock.GetTitle(), wxT( "Test board" ) );
    BOOST_CHECK_EQUAL( titleBlock.GetComment( 0 ), wxT( "CD001" ) );
    BOOST_CHECK_EQUAL( titleBlock.GetComment( 5 ), wxT( "J. Smith" ) );

    // The drawing sheet is embedded under the template's name and set for the project
    const wxString drawingSheetName = m_schematic.Settings().m_SchDrawingSheetFileName;
    BOOST_CHECK_EQUAL( drawingSheetName, wxT( "kicad-embed://House_A4.kicad_wks" ) );

    const EMBEDDED_FILES::EMBEDDED_FILE* file =
            m_schematic.GetEmbeddedFiles()->GetEmbeddedFile( wxT( "House_A4.kicad_wks" ) );
    BOOST_REQUIRE( file );
    BOOST_CHECK( file->type == EMBEDDED_FILES::EMBEDDED_FILE::FILE_TYPE::WORKSHEET );

    std::string   content( file->decompressedData.begin(), file->decompressedData.end() );
    DS_DATA_MODEL model;
    model.SetPageLayout( content.c_str() );

    BOOST_CHECK_EQUAL( model.GetLeftMargin(), 0.0 );
    BOOST_CHECK_EQUAL( model.GetTopMargin(), 0.0 );

    // In page mm from the top left corner; the sheet is 1000 x 800 Altium units, 254 x 203.2 mm
    std::multiset<std::tuple<int, int, int, int, int, int>> shapes;
    std::map<wxString, std::vector<DS_DATA_ITEM_TEXT*>> textsByName;

    auto um = []( double aMM )
    {
        return KiROUND( aMM * 1000.0 );
    };

    for( DS_DATA_ITEM* item : model.GetItems() )
    {
        BOOST_CHECK( item->m_Pos.m_Anchor == LT_CORNER );

        if( item->GetType() == DS_DATA_ITEM::DS_TEXT )
        {
            DS_DATA_ITEM_TEXT* text = static_cast<DS_DATA_ITEM_TEXT*>( item );
            textsByName[text->m_TextBase].push_back( text );
            continue;
        }

        shapes.emplace( item->GetType(), um( item->m_Pos.m_Pos.x ), um( item->m_Pos.m_Pos.y ),
                        um( item->m_End.m_Pos.x ), um( item->m_End.m_Pos.y ), um( item->m_LineWidth ) );
    }

    const std::multiset<std::tuple<int, int, int, int, int, int>> expectedShapes = {
        // Altium's border: the page edge and the zone margin, 20 units in
        { DS_DATA_ITEM::DS_RECT, 0, 203200, 254000, 0, 42 },
        { DS_DATA_ITEM::DS_RECT, 5080, 198120, 248920, 5080, 42 },
        // four zone columns and two rows, divided on both sides
        { DS_DATA_ITEM::DS_SEGMENT, 63500, 0, 63500, 5080, 25 },
        { DS_DATA_ITEM::DS_SEGMENT, 63500, 203200, 63500, 198120, 25 },
        { DS_DATA_ITEM::DS_SEGMENT, 127000, 0, 127000, 5080, 25 },
        { DS_DATA_ITEM::DS_SEGMENT, 127000, 203200, 127000, 198120, 25 },
        { DS_DATA_ITEM::DS_SEGMENT, 190500, 0, 190500, 5080, 25 },
        { DS_DATA_ITEM::DS_SEGMENT, 190500, 203200, 190500, 198120, 25 },
        { DS_DATA_ITEM::DS_SEGMENT, 0, 101600, 5080, 101600, 25 },
        { DS_DATA_ITEM::DS_SEGMENT, 254000, 101600, 248920, 101600, 25 },
        // the template's line, polyline and rectangle in Altium's line widths
        { DS_DATA_ITEM::DS_SEGMENT, 147320, 182880, 248920, 182880, 254 },
        { DS_DATA_ITEM::DS_SEGMENT, 147320, 198120, 147320, 177800, 25 },
        { DS_DATA_ITEM::DS_SEGMENT, 147320, 177800, 248920, 177800, 25 },
        { DS_DATA_ITEM::DS_RECT, 147320, 198120, 248920, 177800, 254 },
    };

    BOOST_CHECK( shapes == expectedShapes );

    // Zone references, each twice, and the template's texts with its special strings as variables
    for( const wxString& zone : { wxT( "1" ), wxT( "2" ), wxT( "3" ), wxT( "4" ), wxT( "A" ), wxT( "B" ) } )
        BOOST_CHECK_EQUAL( textsByName[zone].size(), 2 );

    BOOST_REQUIRE_EQUAL( textsByName[wxT( "${TITLE}" )].size(), 1 );
    BOOST_REQUIRE_EQUAL( textsByName[wxT( "${COMMENT6}" )].size(), 1 );
    BOOST_REQUIRE_EQUAL( textsByName[wxT( "Drawn by:" )].size(), 1 );
    BOOST_CHECK_EQUAL( textsByName.size(), 9 );

    // The first zone column's references are centred on it, in the system font
    for( DS_DATA_ITEM_TEXT* zone : textsByName[wxT( "1" )] )
    {
        BOOST_CHECK_EQUAL( um( zone->m_Pos.m_Pos.x ), 31750 );
        BOOST_CHECK( zone->m_Hjustify == GR_TEXT_H_ALIGN_CENTER );
        BOOST_CHECK( zone->m_Vjustify == GR_TEXT_V_ALIGN_CENTER );
        BOOST_REQUIRE( zone->m_Font );
        BOOST_CHECK_EQUAL( zone->m_Font->GetName(), KIFONT::FONT::GetFont( wxT( "Times New Roman" ) )->GetName() );
    }

    // A template text keeps its font, size and colour, anchored at its bottom left corner
    DS_DATA_ITEM_TEXT* title = textsByName[wxT( "${TITLE}" )].front();
    KIFONT::FONT*      arial = KIFONT::FONT::GetFont( wxT( "Arial" ) );

    BOOST_REQUIRE( title->m_Font );
    BOOST_CHECK_EQUAL( title->m_Font->GetName(), arial->GetName() );
    BOOST_CHECK( title->m_Hjustify == GR_TEXT_H_ALIGN_LEFT );
    BOOST_CHECK( title->m_Vjustify == GR_TEXT_V_ALIGN_BOTTOM );
    BOOST_CHECK_EQUAL( um( title->m_Pos.m_Pos.x ), 177800 );
    BOOST_CHECK_EQUAL( title->m_TextColor, COLOR4D( 0.0, 0.0, 128.0 / 255.0, 1.0 ) );

    if( arial->IsOutline() )
    {
        double size = static_cast<KIFONT::OUTLINE_FONT*>( arial )->GetTextSizeForAscentPlusDescent(
                schIUScale.MilsToIU( 80 ) );
        BOOST_CHECK_CLOSE( title->m_TextSize.y, schIUScale.IUTomm( KiROUND( size ) ), 0.01 );
    }

    // Placed within its 8 point cell of its anchor, 60 units up the sheet
    BOOST_CHECK_LT( std::abs( title->m_Pos.m_Pos.y - ( 203.2 - 60 * 0.254 ) ), 8 * 0.254 );
}


BOOST_AUTO_TEST_SUITE_END()

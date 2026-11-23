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
#include <sch_shape.h>
#include <sch_text.h>
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

    wxString issue16903DataFile( const wxString& aName ) const
    {
        return wxString::FromUTF8( KI_TEST::GetEeschemaTestDataDir()
                                   + "/plugins/altium/issue16903/" )
               + aName;
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

    // The flag on the upper wire points up (Altium orientation 1), the other right (0)
    std::map<wxString, EXPECTED> expected = {
        { wxT( "HT" ), { altiumPoint( 200, 400 ), SPIN_STYLE::RIGHT, altiumPoint( 200, 420 ) } },
        { wxT( "H" ), { altiumPoint( 300, 200 ), SPIN_STYLE::UP, altiumPoint( 320, 200 ) } },
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
    BOOST_CHECK( infos == centers );

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


BOOST_AUTO_TEST_SUITE_END()

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
 * @file test_default_font.cpp
 * Test the default font face of new board text, set per layer class in Board Setup > Text & Graphics Defaults:
 * a board that sets none keeps the KiCad stroke font, and new text, text boxes, footprint fields and dimensions
 * take the face their layer class names. The footprint editor's own defaults (Preferences > Footprint Editor >
 * Defaults) have the same setting for new footprint text.
 */

#include <qa_utils/file_utils.h>
#include <qa_utils/wx_utils/unit_test_utils.h>

#include <board.h>
#include <board_design_settings.h>
#include <embedded_files.h>
#include <font/font.h>
#include <font/fontconfig.h>
#include <font/kicad_font_name.h>
#include <footprint.h>
#include <footprint_editor_settings.h>
#include <json_common.h>
#include <pcb_dimension.h>
#include <pcb_field.h>
#include <pcb_text.h>
#include <pcb_textbox.h>
#include <pcbnew_utils/board_file_utils.h>
#include <pgm_base.h>

#include <algorithm>
#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>


namespace
{
/// A family installed on this machine, preferring the house fixed-width face; empty when none is listed.
wxString installedFamily()
{
    std::vector<std::string> families;

    Fontconfig()->ListFonts( families, std::string( Pgm().GetLanguageTag().utf8_str() ) );

    for( const wxString& preferred : { wxString( "Monaspace Xenon" ), wxString( "DejaVu Sans" ) } )
    {
        for( const std::string& family : families )
        {
            if( wxString::FromUTF8( family ) == preferred )
                return preferred;
        }
    }

    return families.empty() ? wxString() : wxString::FromUTF8( families.front() );
}


/// Precondition for the tests that need a font: a run on a machine with none shows them as skipped
boost::test_tools::assertion_result hasInstalledFamily( boost::unit_test::test_unit_id )
{
    return !installedFamily().IsEmpty();
}


std::string readFile( const std::filesystem::path& aPath )
{
    std::ifstream     file( aPath );
    std::stringstream contents;

    contents << file.rdbuf();
    return contents.str();
}


std::vector<std::string> sortedLines( const std::string& aText )
{
    std::vector<std::string> lines;
    std::istringstream       stream( aText );

    for( std::string line; std::getline( stream, line ); )
        lines.push_back( line );

    std::sort( lines.begin(), lines.end() );
    return lines;
}


size_t countOccurrences( const std::string& aText, const std::string& aNeedle )
{
    size_t count = 0;

    for( size_t at = aText.find( aNeedle ); at != std::string::npos; at = aText.find( aNeedle, at + 1 ) )
        ++count;

    return count;
}
} // namespace


BOOST_AUTO_TEST_SUITE( BoardDefaultFont )


BOOST_AUTO_TEST_CASE( NothingSetKeepsTheStrokeFont )
{
    BOARD                  board;
    BOARD_DESIGN_SETTINGS& bds = board.GetDesignSettings();

    // Not set is its own state, an empty name, apart from an explicit choice of the stroke font
    for( int layerClass = 0; layerClass < LAYER_CLASS_COUNT; ++layerClass )
        BOOST_CHECK( bds.m_TextFont[layerClass].IsEmpty() );

    BOOST_CHECK( bds.GetTextFont( F_SilkS, false, false ) == nullptr );
    BOOST_CHECK( bds.GetTextFont( F_Fab, false, true ) == nullptr );

    PCB_TEXT* text = new PCB_TEXT( &board );
    text->SetLayer( F_SilkS );
    board.Add( text );
    text->StyleFromSettings( bds, true );

    // No font of its own is the stroke font, which is what a file without a (face ...) holds
    BOOST_CHECK( text->GetFont() == nullptr );
    BOOST_CHECK( text->GetFontName().IsEmpty() );
}


BOOST_AUTO_TEST_CASE( NewItemsTakeTheirLayerClassFont, *boost::unit_test::precondition( hasInstalledFamily ) )
{
    const wxString family = installedFamily();

    BOOST_REQUIRE( !family.IsEmpty() );
    BOOST_TEST_MESSAGE( "Default font family used: " << family );

    BOARD                  board;
    BOARD_DESIGN_SETTINGS& bds = board.GetDesignSettings();

    bds.m_TextFont[LAYER_CLASS_SILK] = family;

    BOOST_CHECK_EQUAL( bds.GetTextFontName( F_SilkS ), family );
    BOOST_CHECK_EQUAL( bds.GetTextFontName( B_SilkS ), family );
    BOOST_CHECK( bds.GetTextFontName( F_Fab ).IsEmpty() );

    // Board text
    PCB_TEXT* text = new PCB_TEXT( &board );
    text->SetLayer( F_SilkS );
    board.Add( text );
    text->StyleFromSettings( bds, true );

    BOOST_REQUIRE( text->GetFont() );
    BOOST_CHECK( text->GetFont()->IsOutline() );
    BOOST_CHECK_EQUAL( text->GetFont()->GetName(), family );

    // Another layer class is untouched
    PCB_TEXT* fabText = new PCB_TEXT( &board );
    fabText->SetLayer( F_Fab );
    board.Add( fabText );
    fabText->StyleFromSettings( bds, true );

    BOOST_CHECK( fabText->GetFont() == nullptr );

    // Text box
    PCB_TEXTBOX* textBox = new PCB_TEXTBOX( &board );
    textBox->SetLayer( B_SilkS );
    board.Add( textBox );
    textBox->StyleFromSettings( bds, true );

    BOOST_REQUIRE( textBox->GetFont() );
    BOOST_CHECK_EQUAL( textBox->GetFont()->GetName(), family );

    // Dimension text
    PCB_DIM_ALIGNED* dimension = new PCB_DIM_ALIGNED( &board );
    dimension->SetLayer( F_SilkS );
    board.Add( dimension );
    dimension->StyleFromSettings( bds, true );

    BOOST_REQUIRE( dimension->GetFont() );
    BOOST_CHECK_EQUAL( dimension->GetFont()->GetName(), family );

    // Footprint fields, which is what a new footprint's reference and value are
    FOOTPRINT* footprint = new FOOTPRINT( &board );
    board.Add( footprint );

    PCB_FIELD* field = new PCB_FIELD( footprint, FIELD_T::USER, wxT( "Extra" ) );
    field->SetLayer( F_SilkS );
    footprint->Add( field );
    field->StyleFromSettings( bds, true );

    BOOST_REQUIRE( field->GetFont() );
    BOOST_CHECK_EQUAL( field->GetFont()->GetName(), family );

    // Setting the class back to the stroke font takes the face off again
    bds.m_TextFont[LAYER_CLASS_SILK] = KICAD_FONT_NAME;
    text->StyleFromSettings( bds, true );

    BOOST_CHECK( text->GetFont() == nullptr );
}


/// Restyling a footprint's fields to the board defaults leaves their face alone unless a default font is set.
BOOST_AUTO_TEST_CASE( ApplyDefaultSettingsAndTheFootprintFieldFace,
                      *boost::unit_test::precondition( hasInstalledFamily ) )
{
    const wxString family = installedFamily();

    BOOST_REQUIRE( !family.IsEmpty() );
    BOOST_TEST_MESSAGE( "Default font family used: " << family );

    // The face the footprint's field starts with is not the one the board will name
    const wxString fieldFamily =
            family == wxS( "DejaVu Sans" ) ? wxString( "DejaVu Serif" ) : wxString( "DejaVu Sans" );

    BOARD                  board;
    BOARD_DESIGN_SETTINGS& bds = board.GetDesignSettings();

    FOOTPRINT* footprint = new FOOTPRINT( &board );
    board.Add( footprint );

    PCB_FIELD* field = new PCB_FIELD( footprint, FIELD_T::USER, wxT( "Extra" ) );
    field->SetLayer( F_SilkS );
    footprint->Add( field );

    auto giveFieldItsOwnFace = [&]()
    {
        field->SetFont( KIFONT::FONT::GetFont( fieldFamily, false, false ) );
        BOOST_REQUIRE( field->GetFont() && field->GetFont()->IsOutline() );
    };

    // Not set: the face stays
    giveFieldItsOwnFace();
    footprint->ApplyDefaultSettings( board, true, false, false, false, false );

    BOOST_REQUIRE( field->GetFont() );
    BOOST_CHECK_EQUAL( field->GetFont()->GetName(), fieldFamily );

    // Explicitly the stroke font: the face is cleared
    bds.m_TextFont[LAYER_CLASS_SILK] = KICAD_FONT_NAME;
    giveFieldItsOwnFace();
    footprint->ApplyDefaultSettings( board, true, false, false, false, false );

    BOOST_CHECK( field->GetFont() == nullptr );

    // A face: it is applied
    bds.m_TextFont[LAYER_CLASS_SILK] = family;
    giveFieldItsOwnFace();
    footprint->ApplyDefaultSettings( board, true, false, false, false, false );

    BOOST_REQUIRE( field->GetFont() );
    BOOST_CHECK_EQUAL( field->GetFont()->GetName(), family );
}


BOOST_AUTO_TEST_CASE( FontIsCopiedAndCompared )
{
    BOARD_DESIGN_SETTINGS first( nullptr, "first" );
    BOARD_DESIGN_SETTINGS second( nullptr, "second" );

    BOOST_CHECK( first == second );

    first.m_TextFont[LAYER_CLASS_FAB] = wxS( "Some Family" );

    BOOST_CHECK( !( first == second ) );

    BOARD_DESIGN_SETTINGS copy( first );

    BOOST_CHECK_EQUAL( copy.m_TextFont[LAYER_CLASS_FAB], wxString( "Some Family" ) );
    BOOST_CHECK( copy == first );

    // A face the board names but this machine lacks is kept as a name, not dropped
    BOOST_CHECK_EQUAL( copy.GetTextFontName( F_Fab ), wxString( "Some Family" ) );
}


BOOST_AUTO_TEST_CASE( FontIsEmbeddedOnSave, *boost::unit_test::precondition( hasInstalledFamily ) )
{
    const wxString family = installedFamily();

    BOOST_REQUIRE( !family.IsEmpty() );
    BOOST_TEST_MESSAGE( "Default font family used: " << family );

    BOARD                  board;
    BOARD_DESIGN_SETTINGS& bds = board.GetDesignSettings();

    bds.m_TextFont[LAYER_CLASS_SILK] = family;
    board.SetAreFontsEmbedded( true );

    FOOTPRINT* footprint = new FOOTPRINT( &board );
    board.Add( footprint );

    PCB_FIELD* field = new PCB_FIELD( footprint, FIELD_T::USER, wxT( "Extra" ) );
    field->SetLayer( F_SilkS );
    footprint->Add( field );
    field->StyleFromSettings( bds, true );

    // Only a font whose licence allows embedding is embedded, so say if this one does not
    if( board.GetFonts().empty() )
    {
        BOOST_TEST_MESSAGE( "SKIPPED: " << family << " does not permit embedding" );
        return;
    }

    board.EmbedFonts();

    size_t fonts = 0;

    for( const auto& [name, file] : board.GetEmbeddedFiles()->EmbeddedFileMap() )
    {
        if( file->type == EMBEDDED_FILES::EMBEDDED_FILE::FILE_TYPE::FONT )
            ++fonts;
    }

    BOOST_CHECK_GE( fonts, 1u );
}


/// A board saved before the setting existed loads, and saves, with no face on any of its text.
BOOST_AUTO_TEST_CASE( ExistingBoardRoundTripIsUnchanged )
{
    KI_TEST::SCOPED_TEMP_DIR tempDir( "kicad_qa_default_font" );

    const std::filesystem::path original =
            std::filesystem::path( KI_TEST::GetPcbnewTestDataDir() ) / "custom_fields.kicad_pcb";
    std::unique_ptr<BOARD> board = KI_TEST::ReadBoardFromFile( original );

    for( int layerClass = 0; layerClass < LAYER_CLASS_COUNT; ++layerClass )
        BOOST_CHECK( board->GetDesignSettings().m_TextFont[layerClass].IsEmpty() );

    const std::filesystem::path firstSave = tempDir.Path() / "first.kicad_pcb";
    KI_TEST::DumpBoardToFile( *board, firstSave );

    std::unique_ptr<BOARD>      reloaded = KI_TEST::ReadBoardFromFile( firstSave );
    const std::filesystem::path secondSave = tempDir.Path() / "second.kicad_pcb";
    KI_TEST::DumpBoardToFile( *reloaded, secondSave );

    const std::string before = readFile( original );
    const std::string first = readFile( firstSave );
    const std::string second = readFile( secondSave );

    BOOST_CHECK( sortedLines( first ) == sortedLines( second ) );
    BOOST_CHECK_EQUAL( countOccurrences( first, "(face " ), countOccurrences( before, "(face " ) );
    BOOST_CHECK_EQUAL( countOccurrences( first, "(embedded_fonts yes)" ),
                       countOccurrences( before, "(embedded_fonts yes)" ) );
}


BOOST_AUTO_TEST_SUITE_END()


namespace
{
const char* const FP_EDITOR_FONT_KEYS[] = { "silk_text_font", "copper_text_font", "fab_text_font", "others_text_font" };


/// The footprint editor settings file written by SaveToFile() into @a aDirectory
std::filesystem::path fpEditorSettingsFile( const std::filesystem::path& aDirectory )
{
    return aDirectory / "fpedit.json";
}


nlohmann::json readJson( const std::filesystem::path& aPath )
{
    std::ifstream file( aPath );
    return nlohmann::json::parse( file );
}


void writeJson( const std::filesystem::path& aPath, const nlohmann::json& aJson )
{
    std::ofstream file( aPath );
    file << aJson.dump( 2 );
}
} // namespace


/**
 * The default font face of new footprint text, set per layer class in Preferences > Footprint Editor > Defaults
 * and kept in the footprint editor settings, with the same three states as the board's (not set, the stroke font,
 * a face).
 */
BOOST_AUTO_TEST_SUITE( FootprintEditorDefaultFont )


BOOST_AUTO_TEST_CASE( NothingSetKeepsTheStrokeFont )
{
    FOOTPRINT_EDITOR_SETTINGS    cfg;
    const BOARD_DESIGN_SETTINGS& bds = cfg.m_DesignSettings;

    for( int layerClass = 0; layerClass < LAYER_CLASS_COUNT; ++layerClass )
        BOOST_CHECK( bds.m_TextFont[layerClass].IsEmpty() );

    BOOST_CHECK( bds.GetTextFont( F_SilkS, false, false ) == nullptr );
    BOOST_CHECK( bds.GetTextFont( F_Cu, false, false ) == nullptr );
    BOOST_CHECK( bds.GetTextFont( F_Fab, false, true ) == nullptr );
    BOOST_CHECK( bds.GetTextFont( Cmts_User, false, false ) == nullptr );
}


/// The footprint editor puts its defaults on the board it edits on, and new text takes the face from there.
BOOST_AUTO_TEST_CASE( NewFootprintTextTakesTheFace, *boost::unit_test::precondition( hasInstalledFamily ) )
{
    const wxString family = installedFamily();

    BOOST_REQUIRE( !family.IsEmpty() );
    BOOST_TEST_MESSAGE( "Default font family used: " << family );

    FOOTPRINT_EDITOR_SETTINGS cfg;

    cfg.m_DesignSettings.m_TextFont[LAYER_CLASS_SILK] = family;
    cfg.m_DesignSettings.m_TextFont[LAYER_CLASS_FAB] = KICAD_FONT_NAME;

    BOARD board;
    board.GetDesignSettings() = cfg.m_DesignSettings;

    const BOARD_DESIGN_SETTINGS& bds = board.GetDesignSettings();

    FOOTPRINT* footprint = new FOOTPRINT( &board );
    board.Add( footprint );

    auto newText = [&]( PCB_LAYER_ID aLayer ) -> PCB_TEXT*
    {
        PCB_TEXT* text = new PCB_TEXT( footprint );
        text->SetLayer( aLayer );
        footprint->Add( text );
        text->SetFont( bds.GetTextFont( aLayer, text->IsBold(), text->IsItalic(), text->GetEmbeddedFonts() ) );
        return text;
    };

    // A face
    PCB_TEXT* silkText = newText( F_SilkS );

    BOOST_REQUIRE( silkText->GetFont() );
    BOOST_CHECK( silkText->GetFont()->IsOutline() );
    BOOST_CHECK_EQUAL( silkText->GetFont()->GetName(), family );

    // The stroke font, chosen explicitly
    BOOST_CHECK( newText( F_Fab )->GetFont() == nullptr );

    // Not set
    BOOST_CHECK( newText( F_Cu )->GetFont() == nullptr );

    // A footprint's fields restyled to the defaults take the face too
    PCB_FIELD* field = new PCB_FIELD( footprint, FIELD_T::USER, wxT( "Extra" ) );
    field->SetLayer( B_SilkS );
    footprint->Add( field );
    field->StyleFromSettings( bds, true );

    BOOST_REQUIRE( field->GetFont() );
    BOOST_CHECK_EQUAL( field->GetFont()->GetName(), family );
}


/// Each state is saved and loaded back, per layer class; a face this machine lacks is kept as a name
BOOST_AUTO_TEST_CASE( SettingsRoundTrip )
{
    KI_TEST::SCOPED_TEMP_DIR tempDir( "kicad_qa_fp_editor_default_font" );

    {
        FOOTPRINT_EDITOR_SETTINGS cfg;

        cfg.m_DesignSettings.m_TextFont[LAYER_CLASS_SILK] = wxS( "Some Family" );
        cfg.m_DesignSettings.m_TextFont[LAYER_CLASS_COPPER].clear();
        cfg.m_DesignSettings.m_TextFont[LAYER_CLASS_FAB] = KICAD_FONT_NAME;
        cfg.m_DesignSettings.m_TextFont[LAYER_CLASS_OTHERS] = wxS( "Another Family" );

        cfg.SaveToFile( tempDir.PathStr(), true );
    }

    const nlohmann::json  saved = readJson( fpEditorSettingsFile( tempDir.Path() ) );
    const nlohmann::json& designSettings = saved["design_settings"];

    BOOST_CHECK_EQUAL( designSettings.value( "silk_text_font", "x" ), "Some Family" );
    BOOST_CHECK_EQUAL( designSettings.value( "copper_text_font", "x" ), "" );
    BOOST_CHECK_EQUAL( designSettings.value( "fab_text_font", "x" ), "KiCad Font" );
    BOOST_CHECK_EQUAL( designSettings.value( "others_text_font", "x" ), "Another Family" );

    FOOTPRINT_EDITOR_SETTINGS loaded;
    loaded.LoadFromFile( tempDir.PathStr() );

    BOOST_CHECK_EQUAL( loaded.m_DesignSettings.m_TextFont[LAYER_CLASS_SILK], wxString( "Some Family" ) );
    BOOST_CHECK( loaded.m_DesignSettings.m_TextFont[LAYER_CLASS_COPPER].IsEmpty() );
    BOOST_CHECK_EQUAL( loaded.m_DesignSettings.m_TextFont[LAYER_CLASS_FAB], wxString( KICAD_FONT_NAME ) );
    BOOST_CHECK_EQUAL( loaded.m_DesignSettings.m_TextFont[LAYER_CLASS_OTHERS], wxString( "Another Family" ) );

    // Saving what was loaded writes the same design settings again
    loaded.SaveToFile( tempDir.PathStr(), true );

    const nlohmann::json resaved = readJson( fpEditorSettingsFile( tempDir.Path() ) )["design_settings"];

    BOOST_CHECK_MESSAGE( resaved == designSettings,
                         "changed on the second save: " << nlohmann::json::diff( designSettings, resaved ).dump() );
}


/// Settings saved before the default font existed load with no face in any layer class, and the rest as it was
BOOST_AUTO_TEST_CASE( SettingsWithoutTheFontKeysStayUnset )
{
    KI_TEST::SCOPED_TEMP_DIR tempDir( "kicad_qa_fp_editor_default_font_old" );

    {
        FOOTPRINT_EDITOR_SETTINGS cfg;

        cfg.m_DesignSettings.m_TextItalic[LAYER_CLASS_SILK] = true;
        cfg.SaveToFile( tempDir.PathStr(), true );
    }

    const std::filesystem::path file = fpEditorSettingsFile( tempDir.Path() );
    nlohmann::json              old = readJson( file );

    for( const char* key : FP_EDITOR_FONT_KEYS )
    {
        BOOST_REQUIRE( old["design_settings"].contains( key ) );
        old["design_settings"].erase( key );
    }

    writeJson( file, old );

    FOOTPRINT_EDITOR_SETTINGS loaded;

    // Something to overwrite, so the load is seen to set the font and not just leave it alone
    for( int layerClass = 0; layerClass < LAYER_CLASS_COUNT; ++layerClass )
        loaded.m_DesignSettings.m_TextFont[layerClass] = wxS( "Left Over" );

    loaded.LoadFromFile( tempDir.PathStr() );

    for( int layerClass : { LAYER_CLASS_SILK, LAYER_CLASS_COPPER, LAYER_CLASS_FAB, LAYER_CLASS_OTHERS } )
        BOOST_CHECK( loaded.m_DesignSettings.m_TextFont[layerClass].IsEmpty() );

    BOOST_CHECK( loaded.m_DesignSettings.m_TextItalic[LAYER_CLASS_SILK] );
}


BOOST_AUTO_TEST_SUITE_END()

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 Thomas Pointhuber <thomas.pointhuber@gmx.at>
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

#ifndef _SCH_IO_ALTIUM_H_
#define _SCH_IO_ALTIUM_H_

#include <memory>
#include <set>
#include <tuple>
#include <sch_sheet_path.h>
#include <vector>
#include <sch_io/sch_io.h>
#include <sch_io/sch_io_mgr.h>
#include <wx/filename.h>
#include <wx/gdicmn.h>
#include <case_insensitive_map.h>

#include "altium_parser_sch.h"

class SYMBOL;
class SCH_SYMBOL;
class SCH_SHEET;
class EDA_TEXT;
class SCH_DIRECTIVE_LABEL;
class TITLE_BLOCK;

class ALTIUM_COMPOUND_FILE;

/**
 * SCH_IO_ALTIUM
 * is a #SCH_IO derivation for loading Altium .SchDoc schematic files.
 *
 * As with all SCH_IO there is no UI dependencies i.e. windowing calls allowed.
 */

static std::vector<LIB_SYMBOL*> nullsym;
static std::vector<int> nullint;

struct HARNESS
{
    struct HARNESS_PORT
    {
        VECTOR2I              m_location;
        VECTOR2I              m_entryLocation;
        ASCH_SHEET_ENTRY_SIDE m_harnessConnectorSide;
        int                   m_primaryConnectionPosition;
        wxString              m_name;
    };

    wxString                  m_name;
    VECTOR2I                  m_location;
    VECTOR2I                  m_size;
    std::vector<HARNESS_PORT> m_ports;
    HARNESS_PORT              m_entry;
};


class SCH_IO_ALTIUM : public SCH_IO
{
public:
    SCH_IO_ALTIUM();
    ~SCH_IO_ALTIUM();

    const IO_BASE::IO_FILE_DESC GetSchematicFileDesc() const override
    {
        return IO_BASE::IO_FILE_DESC( _HKI( "Altium schematic files" ), { "SchDoc" } );
    }

    const IO_BASE::IO_FILE_DESC GetLibraryDesc() const override
    {
        return IO_BASE::IO_FILE_DESC( _HKI( "Altium Schematic Library or Integrated Library" ),
                                      { "SchLib", "IntLib" } );
    }

    bool CanReadSchematicFile( const wxString& aFileName ) const override;
    bool CanReadLibrary( const wxString& aFileName ) const override;

    int GetModifyHash() const override;

    SCH_SHEET* LoadSchematicFile( const wxString& aFileName, SCHEMATIC* aSchematic,
                                  SCH_SHEET*             aAppendToMe = nullptr,
                                  const std::map<std::string, UTF8>* aProperties = nullptr ) override;

    SCH_SHEET* LoadSchematicProject( SCHEMATIC*                         aSchematic,
                                     const std::map<std::string, UTF8>* aProperties );

    // unimplemented functions. Will trigger a not_implemented IO error.
    //void SaveLibrary( const wxString& aFileName, const PROPERTIES* aProperties = NULL ) override;

    //void Save( const wxString& aFileName, SCH_SCREEN* aSchematic, KIWAY* aKiway,
    //           const PROPERTIES* aProperties = NULL ) override;


    void EnumerateSymbolLib( wxArrayString&         aSymbolNameList,
                             const wxString&        aLibraryPath,
                             const std::map<std::string, UTF8>* aProperties = nullptr ) override;

    void EnumerateSymbolLib( std::vector<LIB_SYMBOL*>& aSymbolList,
                             const wxString&           aLibraryPath,
                             const std::map<std::string, UTF8>*    aProperties = nullptr ) override;

    LIB_SYMBOL* LoadSymbol( const wxString&        aLibraryPath,
                            const wxString&        aAliasName,
                            const std::map<std::string, UTF8>* aProperties = nullptr ) override;

    //void SaveSymbol( const wxString& aLibraryPath, const LIB_SYMBOL* aSymbol,
    //                 const PROPERTIES* aProperties = NULL ) override;

    //void DeleteAlias( const wxString& aLibraryPath, const wxString& aAliasName,
    //                  const PROPERTIES* aProperties = NULL ) override;

    //void DeleteSymbol( const wxString& aLibraryPath, const wxString& aAliasName,
    //                   const PROPERTIES* aProperties = NULL ) override;

    bool IsLibraryWritable( const wxString& aLibraryPath ) override { return false; }

    wxString   getLibName();
    wxFileName getLibFileName();

    void ParseAltiumSch( const wxString& aFileName );

    /// Give a sheet whose source file is missing an empty screen with a file of its own; aMissingFile
    /// is empty for a converted signal harness.
    void attachEmptySheetScreen( SCH_SHEET* aSheet, const wxString& aMissingFile );

    /// Claim a file in the project folder for a sheet screen that neither the root nor another sheet
    /// holds, suffixing aStem as needed.
    wxString claimSheetFile( const wxString& aStem );

    /// Remove sheet symbols referencing a parse-stack file (an unrepresentable cycle), converting
    /// their pins to hierarchical labels and clearing their sheet-instance records.
    void pruneCyclicSheets( const std::vector<SCH_SHEET*>& aCyclicSheets, SCH_SCREEN* aScreen );

    void ParseStorage( const ALTIUM_COMPOUND_FILE& aAltiumSchFile );
    void ParseAdditional( const ALTIUM_COMPOUND_FILE& aAltiumSchFile );
    void ParseFileHeader( const ALTIUM_COMPOUND_FILE& aAltiumSchFile );

    void ParseASCIISchematic( const wxString& aFileName );

    void ParseRecord( int index, std::map<wxString, wxString>& properties,
                      const wxString& aSectionName );

private:
    SCH_SCREEN* getCurrentScreen();
    SCH_SHEET* getCurrentSheet();

    bool ShouldPutItemOnSheet( int aOwnerindex );
    const ASCH_STORAGE_FILE* GetFileFromStorage( const wxString& aFilename ) const;
    void CreateAliases();
    void PostProcessBusLabels();
    void EnsureSheetSymbolNames();
    wxFileName ResolveSheetFileName( const wxString& aParentPath, const wxString& aSheetFileName ) const;
    void NormalizeRepeatedSheetInstances();

    /**
     * Name power nets after their net labels, as Altium does unless Power Port Names Take
     * Priority is set. A power pin outranks a local label here, so the label becomes global
     * wherever that cannot change connectivity.
     */
    void NameNetsAfterNetLabels();

    /**
     * Give a text the face, size and style of a font from the sheet's font table.
     *
     * Altium draws a font of N points with its ascent plus descent N * 10 mil tall, so an outline
     * font is sized from its own metrics to match. A font that resolves only to the stroke font
     * gets half that height.
     *
     * @param aText is the text to style.
     * @param aFontId is Altium's one-based font id; anything out of range means font 1.
     */
    void SetTextFont( EDA_TEXT* aText, int aFontId ) const;

    /// The kinds of text KiCad positions differently about their anchor.
    enum class TEXT_KIND
    {
        FREE_TEXT,
        SYMBOL_FIELD,
        SHEET_FIELD
    };

    /**
     * Find the anchor that puts a text's glyphs where Altium draws them.
     *
     * Altium anchors text at a corner, edge or centre of a cell from the font's descender line
     * to its ascender line. KiCad places the baseline at a distance from the anchor that depends
     * on the kind of text, its vertical justification and the face's height; those distances
     * were measured on plotted KiCad text.  Text Altium turns upside down is drawn upright in
     * the same cell.
     *
     * @param aKind is the kind of KiCad text that will hold it.
     * @param aAnchor is Altium's anchor, in sheet coordinates.
     * @param aFontId is Altium's font id.
     * @param aJustification is Altium's justification of the anchor.
     * @param aOrientation is Altium's text orientation.
     * @param aPosition receives the KiCad anchor.
     * @return false, leaving \a aPosition alone, if the font is not an outline font.
     */
    bool AltiumTextPosition( TEXT_KIND aKind, const VECTOR2I& aAnchor, int aFontId,
                             ASCH_LABEL_JUSTIFICATION aJustification, ASCH_RECORD_ORIENTATION aOrientation,
                             VECTOR2I& aPosition ) const;

    /**
     * As above, for a font given by its face, its cell height (N * 10 mil for N points) and style.
     */
    static bool AltiumTextPosition( TEXT_KIND aKind, const VECTOR2I& aAnchor, const wxString& aFace, int aCellHeight,
                                    bool aBold, bool aItalic, ASCH_LABEL_JUSTIFICATION aJustification,
                                    ASCH_RECORD_ORIENTATION aOrientation, VECTOR2I& aPosition );

    /**
     * As SetTextFont(), for a font given by its face, its cell height and style.
     */
    static void SetTextFont( EDA_TEXT* aText, const wxString& aFace, int aCellHeight, bool aBold, bool aItalic );

    void AddTextBox( const ASCH_TEXT_FRAME* aElem );
    void AddLibTextBox( const ASCH_TEXT_FRAME* aElem, std::vector<LIB_SYMBOL*>& aSymbol = nullsym,
                        std::vector<int>& aFontSize = nullint );

    void ParseComponent( int aIndex, const std::map<wxString, wxString>& aProperties );
    void ParseTemplate( int aIndex, const std::map<wxString, wxString>& aProperties );
    void ParsePin( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol = nullsym);
    void ParseLabel( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol = nullsym,
                     std::vector<int>& aFontSize = nullint );
    void ParseTextFrame( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol = nullsym,
                         std::vector<int>& aFontSize = nullint );
    void ParseNote( const std::map<wxString, wxString>& aProperties );
    void ParseBezier( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParsePolyline( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParsePolygon( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseRoundRectangle( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseArc( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseEllipticalArc( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseEllipse( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseCircle( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseLine( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseSignalHarness( const std::map<wxString, wxString>& aProperties );
    void ParseHarnessConnector( int aIndex, const std::map<wxString, wxString>& aProperties );
    void ParseHarnessEntry( const std::map<wxString, wxString>& aProperties );
    void ParseHarnessType( const std::map<wxString, wxString>& aProperties );
    void ParseHarnessPort( const ASCH_PORT& aElem );
    void ParseHyperlink( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParsePieChart( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseRectangle( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym);
    void ParseSheetSymbol( int aIndex, const std::map<wxString, wxString>& aProperties );
    void ParseSheetEntry( const std::map<wxString, wxString>& aProperties );
    void ParsePowerPort( const std::map<wxString, wxString>& aProperties );
    void ParsePort( const ASCH_PORT& aElem );
    void ParsePortHelper( const ASCH_PORT& aElem );
    void ParseNoERC( const std::map<wxString, wxString>& aProperties );
    void ParseNetLabel( const std::map<wxString, wxString>& aProperties );
    void ParseBus( const std::map<wxString, wxString>& aProperties );
    void ParseWire( const std::map<wxString, wxString>& aProperties );
    void ParseJunction( const std::map<wxString, wxString>& aProperties );
    void ParseImage( const std::map<wxString, wxString>& aProperties );
    void ParseSheet( const std::map<wxString, wxString>& aProperties );
    void ParseSheetName( const std::map<wxString, wxString>& aProperties );
    void ParseFileName( const std::map<wxString, wxString>& aProperties );
    void ParseDesignator( const std::map<wxString, wxString>& aProperties );
    void ParseLibDesignator( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbols  = nullsym, std::vector<int>& aFontSize = nullint );
    void ParseBusEntry( const std::map<wxString, wxString>& aProperties );
    void ParseParameter( const std::map<wxString, wxString>& aProperties );

    /**
     * Import a Parameter Set directive as a net class flag. Its ClassName parameter, a later
     * record, becomes the flag's Netclass field and a net class in the project.
     */
    void ParseParameterSet( int aIndex, const std::map<wxString, wxString>& aProperties );

    /**
     * Give a parameter set's flag the net class named by its ClassName parameter, adding the
     * class to the project at the end of the import.
     *
     * @return false if \a aElem does not belong to a parameter set.
     */
    bool AddParameterSetParameter( const ASCH_PARAMETER& aElem );

    /// Remove the flags of the sheet's parameter sets that name no net class.
    void FinishParameterSets();
    void ParseLibParameter( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym, std::vector<int>& aFontSize = nullint );
    void ParseImplementationList( int aIndex, const std::map<wxString, wxString>& aProperties );
    void ParseImplementation( const std::map<wxString, wxString>& aProperties, std::vector<LIB_SYMBOL*>& aSymbol  = nullsym );

    void ParseLibHeader( const ALTIUM_COMPOUND_FILE& aAltiumSchFile, std::vector<int>& aFontSizes );
    CASE_INSENSITIVE_MAP<LIB_SYMBOL*> ParseLibFile( const ALTIUM_COMPOUND_FILE& aAltiumSchFile );
    std::vector<LIB_SYMBOL*> ParseLibComponent( const std::map<wxString, wxString>& aProperties );

    void doEnumerateSymbolLib( const wxString& aLibraryPath, const std::map<std::string, UTF8>* aProperties,
                               std::function<void(const wxString&, LIB_SYMBOL*)> aInserter );

    void fixupSymbolPinNameNumbers( SYMBOL* aSymbol );

    // Symbol caching
    void ensureLoadedLibrary( const wxString& aLibraryPath, const std::map<std::string, UTF8>* aProperties );
    long long getLibraryTimestamp( const wxString& aLibraryPath ) const;

    static bool isBinaryFile( const wxString& aFileName );
    static bool isASCIIFile( const wxString& aFileName );
    static bool checkFileHeader( const wxString& aFileName );

private:
    wxString       m_rootFilepath;      // The file path of the root sheet being imported
    SCH_SHEET*     m_rootSheet;         // The root sheet of the schematic being loaded..
    SCH_SHEET_PATH m_sheetPath;
    std::set<wxString> m_unsetSheetParams;   // Sheet parameters Altium leaves as "*"
    SCHEMATIC*     m_schematic;         // Passed to Load(), the schematic object being loaded
    wxString       m_libName;           // Library name to save symbols
    bool           m_isIntLib;          // Flag to indicate Integrated Library

    std::unique_ptr<TITLE_BLOCK>       m_currentTitleBlock;   // Will be assigned at the end of parsing
                                                              //   a sheet

    VECTOR2I                           m_sheetOffset;
    std::unique_ptr<ASCH_SHEET>        m_altiumSheet;
    std::map<int, SCH_SYMBOL*>         m_symbols;
    std::map<int, SCH_SHEET*>          m_sheets;
    /// A parameter set's flag, and the circle and "i" drawn beside it to look like Altium's
    struct PARAMETER_SET_ITEMS
    {
        SCH_DIRECTIVE_LABEL*   flag = nullptr;
        std::vector<SCH_ITEM*> decorations;
    };

    /// How many net labels use each font (face, bold, italic), for the label offset
    std::map<std::tuple<wxString, bool, bool>, int> m_netLabelFonts;

    std::map<int, PARAMETER_SET_ITEMS>  m_parameterSets;          // the current sheet's, by record index
    std::set<wxString>                  m_parameterSetNetClasses; // net classes they name
    std::map<int, LIB_SYMBOL*>         m_libSymbols;        // every symbol has its unique lib_symbol

    std::map<wxString, LIB_SYMBOL*>    m_powerSymbols;
    std::vector<ASCH_STORAGE_FILE>     m_altiumStorage;
    std::vector<ASCH_ADDITIONAL_FILE>  m_altiumAdditional;

    std::map<int, ASCH_SYMBOL>         m_altiumComponents;
    std::map<int, ASCH_TEMPLATE>       m_altiumTemplates;
    std::map<int, int>                 m_altiumImplementationList;
    std::vector<ASCH_PORT>             m_altiumPortsCurrentSheet; // we require all connections first

    // parse harness ports after "FileHeader" was parsed, in 2nd run.
    std::vector<ASCH_PORT>             m_altiumHarnessPortsCurrentSheet;
    std::map<int, HARNESS>             m_altiumHarnesses;

    // Add offset to all harness ownerIndex'es after parsing FileHeader.
    int m_harnessOwnerIndexOffset;
    int m_harnessEntryParent; // used to identify harness connector for harness entry element

    std::map<wxString, long long> m_timestamps;
    std::map<wxString, CASE_INSENSITIVE_MAP<LIB_SYMBOL*>> m_libCache;

    // List of available fonts with font name and font size in pt
    std::vector<std::pair<wxString, int>> m_fonts;

    // Persists across sheets so variant application can match by Altium UniqueId
    std::map<const SCH_SYMBOL*, wxString> m_altiumSymbolToUid;

    // Special strings sheet text refers to, which Altium resolves to nothing when no value exists
    std::set<wxString> m_sheetSpecialStrings;

    // Cache the error messages to avoid duplicate messages
    std::unordered_map<wxString, SEVERITY > m_errorMessages;

    // Canonical parse-stack paths, ref-counted so a multi-page block holds all sibling members at
    // once. Breaks cyclic OrCad sheet references. See PARSE_FILE_GUARD.
    std::map<wxString, int> m_parsingFiles;

    // Fully parsed sub-sheet screens by their ordered source files, shared by repeated channels
    std::map<std::vector<wxString>, SCH_SCREEN*> m_loadedScreens;

    // Empty screens of sheets whose source file is missing, by their KiCad file path
    std::map<wxString, SCH_SCREEN*> m_missingScreens;

    // The lower-cased files the sheet screens claimed
    std::set<wxString> m_sheetFiles;

    // The file the root or first project sheet is saved as, which no other sheet may take
    wxString m_rootSheetFile;

    // Parse keys of the source files read so far
    std::set<wxString> m_parsedFiles;

    // Lower-cased parse keys of the source files some sheet symbol shows
    std::set<wxString> m_subsheetFiles;

    // The schematic documents a project lists, which its sheet symbols name wherever they are
    std::vector<wxFileName> m_projectDocuments;

    friend struct PARSE_FILE_GUARD;
};

#endif // _SCH_IO_ALTIUM_H_

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019-2020 Thomas Pointhuber <thomas.pointhuber@gmx.at>
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

#ifndef ALTIUM_PCB_H
#define ALTIUM_PCB_H

#include <core/utf8.h>
#include <geometry/shape_line_chain.h>
#include <embedded_files.h>
#include <functional>
#include <layer_ids.h>
#include <map>
#include <set>
#include <memory>
#include <set>
#include <vector>
#include <pcb_io/common/plugin_common_layer_mapping.h>


#include <altium_parser_pcb.h>


enum class ALTIUM_PCB_DIR
{
    FILE_HEADER,

    ADVANCEDPLACEROPTIONS6,
    ARCS6,
    BOARD6,
    BOARDREGIONS,
    CLASSES6,
    COMPONENTBODIES6,
    COMPONENTS6,
    CONNECTIONS6,
    COORDINATES6,
    DESIGNRULECHECKEROPTIONS6,
    DIFFERENTIALPAIRS6,
    DIMENSIONS6,
    EMBEDDEDBOARDS6,
    EMBEDDEDFONTS6,
    EMBEDDEDS6,
    EXTENDPRIMITIVEINFORMATION,
    FILEVERSIONINFO,
    FILLS6,
    FROMTOS6,
    MODELS,
    MODELSNOEMBED,
    NETS6,
    PADS6,
    PADVIALIBRARY,
    PADVIALIBRARYCACHE,
    PADVIALIBRARYLINKS,
    PINSWAPOPTIONS6,
    PINPAIRSSECTION,
    POLYGONS6,
    REGIONS6,
    RULES6,
    SHAPEBASEDCOMPONENTBODIES6,
    SHAPEBASEDREGIONS6,
    SIGNALCLASSES,
    SMARTUNIONS,
    TEXTS,
    TEXTS6,
    TEXTURES,
    TRACKS6,
    UNIONNAMES,
    UNIQUEIDPRIMITIVEINFORMATION,
    VIAS6,
    WIDESTRINGS6
};


class BOARD;
class BOARD_ITEM;
class EDA_TEXT;
class FP_SHAPE;
class PAD;
class PCB_SHAPE;
class PCB_TEXTBOX;
class FOOTPRINT;
class ZONE;
class PCB_DIM_RADIAL;
class PROGRESS_REPORTER;
class NET_SETTINGS;

namespace CFB
{
struct COMPOUND_FILE_ENTRY;
} // namespace CFB

class ALTIUM_PCB_COMPOUND_FILE;
class ALTIUM_DESIGN_RULES;
struct ALTIUM_PCB_IMPORT_FIXTURE;

// Structure for storing embedded model data
struct ALTIUM_EMBEDDED_MODEL_DATA
{
    wxString m_modelname;
    VECTOR3D m_rotation;
    double   m_z_offset;
    std::vector<char> m_data;

    // Constructor
    ALTIUM_EMBEDDED_MODEL_DATA(const wxString& name, const VECTOR3D& rotation, double z_offset, std::vector<char>&& data)
        : m_modelname(name), m_rotation(rotation), m_z_offset(z_offset), m_data(std::move(data)) {}
};

/**
 * Invent a placeholder name for an Altium net that has none.
 *
 * NETINFO_LIST keys nets by name and the unconnected net owns the empty string, so a nameless
 * net has to be given one or it is merged into another net.  @p aCounter is advanced past every
 * candidate that is already taken on @p aBoard.
 */
wxString AltiumUnnamedNetName( const BOARD& aBoard, int& aCounter );


/**
 * Copy the values of Altium rules scoped to a single netclass onto that netclass.
 *
 * Only rules of the form InNetClass('\<name\>') against All are considered, and only for the
 * clearance, width and routing-via kinds.  Each vector in @p aRulesByKind must be sorted by
 * ARULE6::priority ascending; the first enabled match for a netclass wins, because Altium
 * priority 1 is the most specific.  Disabled rules are skipped.
 *
 * @p aUnresolved, when given, collects the rules that name a netclass the board does not define.
 * Those constraints are lost, and the pointers alias @p aRulesByKind.
 */
void ApplyAltiumNetclassRules( const std::map<ALTIUM_RULE_KIND, std::vector<ARULE6>>& aRulesByKind,
                               NET_SETTINGS& aNetSettings,
                               std::vector<const ARULE6*>* aUnresolved = nullptr );


// type declaration required for a helper method
class ALTIUM_PCB;
typedef std::function<void( const ALTIUM_PCB_COMPOUND_FILE&, const CFB::COMPOUND_FILE_ENTRY* )>
        PARSE_FUNCTION_POINTER_fp;

class ALTIUM_PCB
{
    friend struct ALTIUM_PCB_IMPORT_FIXTURE;

public:
    explicit ALTIUM_PCB( BOARD* aBoard, PROGRESS_REPORTER* aProgressReporter,
                         LAYER_MAPPING_HANDLER& aLayerMappingHandler,
                         REPORTER* aReporter = nullptr,
                         const wxString& aLibrary = wxEmptyString,
                         const wxString& aFootprintName = wxEmptyString);
    ~ALTIUM_PCB();

    void Parse( const ALTIUM_PCB_COMPOUND_FILE&              aAltiumPcbFile,
                const std::map<ALTIUM_PCB_DIR, std::string>& aFileMapping,
                const std::map<std::string, UTF8>*           aProperties = nullptr );

    std::unique_ptr<FOOTPRINT> ParseFootprint( ALTIUM_PCB_COMPOUND_FILE& altiumLibFile,
                                               const wxString&           aFootprintName );

    /// Special strings in the parsed text that KiCad has no variable for.
    const std::set<wxString>& GetSpecialStrings() const { return m_specialStrings; }

    /// The zones left unfilled for the caller to fill, the internal planes.
    const std::vector<ZONE*>& GetZonesToFill() const { return m_zonesToFill; }

    /**
     * @return the board's Altium design rules as a KiCad custom rules file, or an empty string
     *         when none could be translated.  Valid after Parse().
     * @param aSourceName the Altium file name, quoted in the file's header comment.
     */
    wxString GetDesignRules( const wxString& aSourceName ) const;

    /**
     * @return the component classes the translated rules test for, with their members'
     *         designators.  Valid after Parse().
     */
    std::vector<std::pair<wxString, std::vector<wxString>>> GetDesignRuleComponentClasses() const;

private:
    void checkpoint();

    PCB_LAYER_ID  GetKicadLayer( ALTIUM_LAYER aAltiumLayer ) const;
    std::vector<PCB_LAYER_ID> GetKicadLayersToIterate( ALTIUM_LAYER aAltiumLayer ) const;
    int           GetNetCode( uint16_t aId ) const;
    const ARULE6* GetRule( ALTIUM_RULE_KIND aKind, const wxString& aName ) const;
    const ARULE6* GetRuleDefault( ALTIUM_RULE_KIND aKind ) const;
    const ARULE6* GetRuleForPolygon( ALTIUM_RULE_KIND aKind ) const;

    void ParseFileHeader( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                          const CFB::COMPOUND_FILE_ENTRY* aEntry );

    // Text Format
    void ParseBoard6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                          const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseClasses6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                            const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseComponents6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                               const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseDimensions6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                               const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseModelsData( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                          const CFB::COMPOUND_FILE_ENTRY* aEntry,
                          const std::vector<std::string>& aRootDir );
    void ParseNets6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                         const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParsePolygons6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                             const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseRules6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                          const CFB::COMPOUND_FILE_ENTRY* aEntry );

    // Binary Format
    void ParseArcs6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                         const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ConvertArcs6ToPcbShape( const AARC6& aElem, PCB_SHAPE* aShape );
    void ConvertArcs6ToBoardItem( const AARC6& aElem, const int aPrimitiveIndex );
    void ConvertArcs6ToFootprintItem( FOOTPRINT* aFootprint, const AARC6& aElem,
                                      const int aPrimitiveIndex, const bool aIsBoardImport );
    void ConvertArcs6ToBoardItemOnLayer( const AARC6& aElem, PCB_LAYER_ID aLayer );
    void ConvertArcs6ToFootprintItemOnLayer( FOOTPRINT* aFootprint, const AARC6& aElem,
                                             PCB_LAYER_ID aLayer );
    void ParseComponentsBodies6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                                     const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ConvertComponentBody6ToFootprintItem( const ALTIUM_PCB_COMPOUND_FILE& aAltiumPcbFile,
                                              FOOTPRINT* aFootprint,
                                              const ACOMPONENTBODY6& aElem );
    void ParsePads6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                         const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ConvertPads6ToBoardItem( const APAD6& aElem );
    void ConvertPads6ToFootprintItem( FOOTPRINT* aFootprint, const APAD6& aElem );
    void ConvertPads6ToBoardItemOnNonCopper( const APAD6& aElem );
    void ConvertPads6ToFootprintItemOnCopper( FOOTPRINT* aFootprint, const APAD6& aElem );

    /**
     * Give a non-plated Altium pad's copper back its pad number and net.
     *
     * Altium lets a non-plated hole carry copper, a pad number and a net (a pot or bush clamped
     * to a ground ring, say), but a KiCad NPTH pad has neither number nor net, and its padstack
     * cannot leave an outer layer without copper.  Such a pad is kept as the bare hole, which
     * carries the net so that its own copper is no hole clearance violation, and one SMD pad of
     * the Altium pad's number and net is added for each outer copper layer Altium has copper on.
     * A pad without copper beyond its hole only has its zero-size layers made valid.
     *
     * @param aFootprint is the footprint the pad belongs to; the SMD pads are added to it.
     * @param aHole is the converted NPTH pad, not yet added to \a aFootprint.
     * @param aElem is the Altium pad \a aHole was converted from.
     */
    void ConvertNpthCopperToSmdPads( FOOTPRINT* aFootprint, PAD* aHole, const APAD6& aElem );

    void ConvertPads6ToFootprintItemOnNonCopper( FOOTPRINT* aFootprint, const APAD6& aElem );
    void ParseVias6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                         const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ConvertVias6ToBoardItem( const AVIA6& aElem );
    PAD* ConvertVias6ToFootprintItem( FOOTPRINT* aFootprint, const AVIA6& aElem );
    void ParseTracks6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                           const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ConvertTracks6ToBoardItem( const ATRACK6& aElem, const int aPrimitiveIndex );
    void ConvertTracks6ToFootprintItem( FOOTPRINT* aFootprint, const ATRACK6& aElem,
                                        const int aPrimitiveIndex, const bool aIsBoardImport );
    void ConvertTracks6ToBoardItemOnLayer( const ATRACK6& aElem, PCB_LAYER_ID aLayer );
    void ConvertTracks6ToFootprintItemOnLayer( FOOTPRINT* aFootprint, const ATRACK6& aElem,
                                               PCB_LAYER_ID aLayer );
    void ParseSmartUnions6Data( const ALTIUM_PCB_COMPOUND_FILE&  aAltiumPcbFile,
                                const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseUnionNamesData( const ALTIUM_PCB_COMPOUND_FILE& aAltiumPcbFile, const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void HelperCreateTuningPatterns();

    /**
     * Remove board tracks that add no copper: exact duplicates of another track, and tracks of
     * zero length lying wholly inside other copper of their net and layer.  Altium keeps
     * duplicate primitives, and tracks of a few nanometres that the conversion to KiCad's 10 nm
     * grid turns into zero-length segments.  A zero-length track standing alone is real copper
     * (a dot of the track's width) and is kept.
     */
    void HelperRemoveRedundantTracks();

    /**
     * Where two pours of different nets overlap and Altium's stored fills show which of them
     * owns the overlap, give that one the higher priority, so that a KiCad refill pours it the
     * same way.  Altium's pour order alone does not always say: a pour repoured on its own keeps
     * its index.
     */
    void HelperSetZonePrioritiesFromFills();

    /**
     * Prepare Altium internal planes, which a file stores without fills, to be poured: Altium
     * plots a plane whole, so a board saved without its fill would have no plane until refilled.
     * A plane stops its layer's pullback distance short of the board edge.  The planes are listed
     * in m_zonesToFill for the caller to fill, the importer having no zone filler of its own.
     */
    void HelperPrepareUnfilledPlanes();
    void HelperSetFootprintMountingStyles();

    /**
     * @return the mounting style FOOTPRINT::GetLikelyAttribute() derives from the pads, except
     *         that a footprint whose only SMD pads are the copper of non-plated holes has none.
     */
    int HelperGetLikelyMountingStyle( const FOOTPRINT* aFootprint ) const;

    /**
     * Join the pads of a footprint that share a number, as Altium does: it treats them as one pin
     * of the part, where KiCad otherwise wants copper between them.  Nothing is joined when two
     * such pads are on different nets.
     */
    void HelperSetDuplicatePadNumbersAreJumpers( FOOTPRINT* aFootprint );

    void HelperSetFootprintStackups();

    /**
     * Translate the board's Altium design rules into KiCad custom rules once every item is on the
     * board, report the rules that could not be, and lower the board minimums to what the rules
     * ask for.
     */
    void HelperConvertDesignRules();

    /**
     * Hold copper a footprint draws until its pads exist.
     *
     * @param aFallback places the copper where it would otherwise go when no pad of its net touches it.
     */
    void HelperDeferComponentCopper( FOOTPRINT* aFootprint, PCB_LAYER_ID aLayer, uint16_t aNet,
                                     std::unique_ptr<PCB_SHAPE> aShape, std::function<void()> aFallback );

    /**
     * Join each deferred copper shape on \a aCopper to the same-net numbered pad it touches.
     *
     * @param aExtensionPads copper-only pads made by earlier passes, which grow in place.
     * @return true when any shape was joined.
     */
    bool HelperFoldPendingCopper( const LSET& aCopper, int aMaxError, std::set<PAD*>& aExtensionPads );

    /// Run the fallback of every deferred copper shape no pad took.
    void HelperPlaceUnfoldedCopper();

    /// Join deferred component copper and vias to the same-net pads they touch.
    void HelperFoldComponentCopperIntoPads();

    /// Rebuild the composite netclasses and point every net at its effective netclass.
    void HelperAssignNetclassesToNets();

    void     MapSchematicNetNames( const std::map<std::string, UTF8>& aProperties );
    wxString SchematicCasedNetName( const wxString& aNetName ) const;

    void ParseTexts6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                          const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ConvertTexts6ToBoardItem( const ATEXT6& aElem );
    void ConvertTexts6ToFootprintItem( FOOTPRINT* aFootprint, const ATEXT6& aElem );
    void ConvertTexts6ToBoardItemOnLayer( const ATEXT6& aElem, PCB_LAYER_ID aLayer );
    void ConvertTexts6ToFootprintItemOnLayer( FOOTPRINT* aFootprint, const ATEXT6& aElem,
                                              PCB_LAYER_ID aLayer );
    void ConvertBarcodes6ToBoardItemOnLayer( const ATEXT6& aElem, PCB_LAYER_ID aLayer );
    void ConvertBarcodes6ToFootprintItemOnLayer( FOOTPRINT* aFootprint, const ATEXT6& aElem,
                                                 PCB_LAYER_ID aLayer );
    void ConvertTexts6ToEdaTextSettings( const ATEXT6& aElem, EDA_TEXT& aEdaText );
    void ParseFills6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                          const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ConvertFills6ToBoardItem( const AFILL6& aElem );
    void ConvertFills6ToFootprintItem( FOOTPRINT* aFootprint, const AFILL6& aElem,
                                       const bool aIsBoardImport );
    void ConvertFills6ToPcbShape( const AFILL6& aElem, PCB_SHAPE* aShape );
    void ConvertFills6ToBoardItemOnLayer( const AFILL6& aElem, PCB_LAYER_ID aLayer );
    void ConvertFills6ToFootprintItemOnLayer( FOOTPRINT* aFootprint, const AFILL6& aElem,
                                              PCB_LAYER_ID aLayer );
    void ParseBoardRegionsData( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                                const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseShapeBasedRegions6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                                      const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ConvertShapeBasedRegions6ToBoardItem( const AREGION6& aElem, const int aPrimitiveIndex );
    void ConvertShapeBasedRegions6ToFootprintItem( FOOTPRINT* aFootprint, const AREGION6& aElem,
                                                   const int aPrimitiveIndex );

    /// Plain copper region of a footprint; inner layers wait to join the pad they touch.
    void ConvertShapeBasedRegions6ToFootprintCopper( FOOTPRINT* aFootprint, const AREGION6& aElem,
                                                     const int aPrimitiveIndex, uint16_t aNet );
    void ConvertShapeBasedRegions6ToBoardItemOnLayer( const AREGION6& aElem, PCB_LAYER_ID aLayer,
                                                      const int aPrimitiveIndex );
    void ConvertShapeBasedRegions6ToFootprintItemOnLayer( FOOTPRINT*      aFootprint,
                                                          const AREGION6& aElem,
                                                          PCB_LAYER_ID    aLayer,
                                                          const int       aPrimitiveIndex );
    void ParseExtendedPrimitiveInformationData( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                                                const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseRegions6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                            const CFB::COMPOUND_FILE_ENTRY* aEntry );
    void ParseWideStrings6Data( const ALTIUM_PCB_COMPOUND_FILE&     aAltiumPcbFile,
                                const CFB::COMPOUND_FILE_ENTRY* aEntry );

    // Helper Functions
    void HelperParseDimensions6Linear( const ADIMENSION6& aElem );
    void HelperParseDimensions6Radial( const ADIMENSION6& aElem );
    void HelperParseDimensions6Leader( const ADIMENSION6& aElem );
    void HelperParseDimensions6Datum( const ADIMENSION6& aElem );
    void HelperParseDimensions6Center( const ADIMENSION6& aElem );

    void HelperParsePad6NonCopper( const APAD6& aElem, PCB_LAYER_ID aLayer, PCB_SHAPE* aShape );

    void HelperFillMechanicalLayerAssignments( const std::vector<ABOARD6_LAYER_STACKUP>& aStackup );

    void HelperCreateBoardOutline( const std::vector<ALTIUM_VERTICE>& aVertices );
    void HelperCreateBoardOutline( const SHAPE_LINE_CHAIN& aOutline );
    void HelperFinishBoardOutline();

    void HelperSetZoneLayers( ZONE& aZone, const ALTIUM_LAYER aAltiumLayer );
    void HelperSetZoneKeepoutRestrictions( ZONE& aZone, const uint8_t aKeepoutRestrictions );

    /**
     * Resolve an Altium keepout restriction mask, substituting the restrictions implied by the
     * layer when the mask is empty.
     *
     * @return the mask to apply, or zero if the keepout restricts nothing and should be dropped.
     */
    uint8_t HelperGetKeepoutRestrictions( const uint8_t aKeepoutRestrictions, const ALTIUM_LAYER aAltiumLayer );

    void HelperFootprintZoneToLibFrame( ZONE& aZone, const FOOTPRINT& aFootprint );
    void HelperPcpShapeAsBoardKeepoutRegion( const PCB_SHAPE&   aShape,
                                             const ALTIUM_LAYER aAltiumLayer,
                                             const uint8_t      aKeepoutRestrictions );
    void HelperPcpShapeAsFootprintKeepoutRegion( FOOTPRINT* aFootprint, const PCB_SHAPE& aShape,
                                                 const ALTIUM_LAYER aAltiumLayer,
                                                 const uint8_t      aKeepoutRestrictions );

    void HelperSetTextboxAlignmentAndPos( const ATEXT6& aElem, PCB_TEXTBOX* aPcbTextbox );
    void HelperSetTextAlignmentAndPos( const ATEXT6& aElem, EDA_TEXT* aEdaText );

    /**
     * Place a free Altium stroke text where Altium draws its ink: left and bottom justified at
     * the text's origin, which is where Altium's glyph run starts and its baseline lies, with the
     * glyphs widened or narrowed so the run is as long as Altium's.  Altium's run length is its
     * stored box where it has one, and otherwise comes from its stroke fonts' glyph advances.
     *
     * @return false, leaving \a aEdaText alone, when the text is not one this can place.
     */
    bool HelperPlaceStrokeTextOnAltiumInk( const ATEXT6& aElem, EDA_TEXT* aEdaText );

    std::vector<std::pair<PCB_LAYER_ID, int>>
    HelperGetSolderAndPasteMaskExpansions( const ALTIUM_RECORD aType, const int aPrimitiveIndex,
                                           const ALTIUM_LAYER aAltiumLayer );

    FOOTPRINT* HelperGetFootprint( uint16_t aComponent ) const;

    void HelperBuildPadstackLayerIndex();

    /**
     * Return the slot @p aLayer occupies in an Altium padstack's per-layer arrays.
     *
     * Those arrays are indexed by Altium layer id (top, mid 1 through 30, bottom), which only
     * lines up with the KiCad stack position when the board stacks its mid layers in order.
     *
     * @return the slot, or -1 when the layer has none, as internal planes do.
     */
    int HelperGetPadstackLayerIndex( PCB_LAYER_ID aLayer ) const;

    /**
     * Return the 3D model @p aModelName embedded in @p aFootprint, inflating and embedding
     * @p aCompressedData first if it is not there yet.
     *
     * @param aFootprint
     * @param aModelName
     * @param aCompressedData
     * @param aIsNew is set when the model was embedded by this call and so still owes a
     *               CompressAndEncode(), which the caller may run inline or on the pool.
     */
    std::shared_ptr<EMBEDDED_FILES::EMBEDDED_FILE>
    HelperEmbedModel( FOOTPRINT* aFootprint, const wxString& aModelName,
                      const std::vector<char>& aCompressedData, bool& aIsNew );

    void remapUnsureLayers( std::vector<ABOARD6_LAYER_STACKUP>& aStackup );

    BOARD*                               m_board;
    std::vector<FOOTPRINT*>              m_components;
    std::set<const PAD*>                 m_npthCopperPads; // SMD pads split from an NPTH pad
    std::vector<ZONE*>                   m_polygons;
    std::vector<PCB_DIM_RADIAL*>         m_radialDimensions;

    // The outline waits for every board cutout, since a cutout can run over the board edge
    SHAPE_LINE_CHAIN              m_boardOutline;
    std::vector<SHAPE_LINE_CHAIN> m_boardCutouts;
    int                           m_outlineJogCount = 0;
    int                           m_outlineJogLargest = 0;
    std::map<uint32_t, wxString>         m_unicodeStrings;
    std::vector<int>                     m_altiumToKicadNetcodes;
    std::map<wxString, wxString>         m_schematicNetNames; // upper cased name to schematic casing
    std::map<ALTIUM_LAYER, PCB_LAYER_ID> m_layermap; // used to correctly map layers
    std::map<ALTIUM_LAYER, wxString>     m_layerNames;
    std::map<PCB_LAYER_ID, int>          m_padstackLayerIndex;

    std::map<wxString, ALTIUM_EMBEDDED_MODEL_DATA>  m_EmbeddedModels;
    std::map<ALTIUM_RULE_KIND, std::vector<ARULE6>> m_rules;
    std::vector<ARULE6>                             m_ruleRecords;  ///< in the order of the file
    std::vector<ACLASS6>                            m_classRecords; ///< in the order of the file
    std::unique_ptr<ALTIUM_DESIGN_RULES>            m_designRules;
    std::map<ALTIUM_RECORD, std::multimap<int, const AEXTENDED_PRIMITIVE_INFORMATION>>
            m_extendedPrimitiveInformationMaps;

    std::vector<ASMARTUNION6>                  m_tuningUnions;
    std::map<uint32_t, wxString>               m_unionNames;
    std::map<int, std::vector<BOARD_ITEM*>>    m_unionToBoardItems;

    struct PENDING_COMPONENT_COPPER
    {
        FOOTPRINT*                 footprint;
        PCB_LAYER_ID               layer;
        int                        netcode;
        std::unique_ptr<PCB_SHAPE> shape;        ///< Board coordinates, released once folded
        std::function<void()>      fallback;
    };

    std::vector<PENDING_COMPONENT_COPPER>          m_pendingComponentCopper;
    std::vector<std::pair<FOOTPRINT*, AVIA6>>      m_pendingComponentVias;

    std::map<ALTIUM_LAYER, ZONE*>        m_outer_plane;
    std::map<ALTIUM_LAYER, int>          m_planePullbacks;
    std::vector<ZONE*>                   m_zonesToFill;

    LAYER_MAPPING_HANDLER   m_layerMappingHandler;

    PROGRESS_REPORTER* m_progressReporter;   ///< optional; may be nullptr
    REPORTER*          m_reporter;           ///< optional; may be nullptr
    std::set<wxString> m_specialStrings;     ///< special strings in text that KiCad has no variable for
    unsigned           m_doneCount;
    unsigned           m_lastProgressCount;
    unsigned           m_totalCount;         ///< for progress reporting

    wxString           m_library;            ///< for footprint library loading error reporting
    wxString           m_footprintName;      ///< for footprint library loading error reporting

    /// Altium stores pour order across all layers
    int m_highest_pour_index;
};


#endif //ALTIUM_PCB_H

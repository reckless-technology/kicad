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

#include "altium_outjob.h"

#include <jobs/jobset.h>
#include <settings/json_settings_internals.h>
#include <ki_exception.h>

#include <boost/uuid/name_generator_sha1.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/regex.h>
#include <wx/translation.h>


namespace
{

/// "Record=Name|A=1|B=2" split into its keys and values
std::map<wxString, wxString> parseRecord( const wxString& aText )
{
    std::map<wxString, wxString> record;

    for( const wxString& part : wxSplit( aText, '|', 0 ) )
    {
        if( part.Contains( wxT( "=" ) ) )
            record[part.BeforeFirst( '=' )] = part.AfterFirst( '=' );
    }

    return record;
}


wxString valueOr( const std::map<wxString, wxString>& aMap, const wxString& aKey, const wxString& aDefault )
{
    auto it = aMap.find( aKey );
    return it == aMap.end() ? aDefault : it->second;
}


/// Altium's V7 layer ids of the layers with a fixed KiCad counterpart
const std::map<long, wxString> fixedLayers = {
    { 0x01000001, wxT( "F.Cu" ) },    { 0x0100FFFF, wxT( "B.Cu" ) },    { 0x01030006, wxT( "F.SilkS" ) },
    { 0x01030007, wxT( "B.SilkS" ) }, { 0x01030008, wxT( "F.Paste" ) }, { 0x01030009, wxT( "B.Paste" ) },
    { 0x0103000A, wxT( "F.Mask" ) },  { 0x0103000B, wxT( "B.Mask" ) },  { 0x0103000D, wxT( "Edge.Cuts" ) },
};


/// Altium's print layer names, letters only and in lower case
const std::map<wxString, wxString> namedLayers = {
    { wxT( "toplayer" ), wxT( "F.Cu" ) },          { wxT( "bottomlayer" ), wxT( "B.Cu" ) },
    { wxT( "topoverlay" ), wxT( "F.SilkS" ) },     { wxT( "bottomoverlay" ), wxT( "B.SilkS" ) },
    { wxT( "toppaste" ), wxT( "F.Paste" ) },       { wxT( "bottompaste" ), wxT( "B.Paste" ) },
    { wxT( "topsolder" ), wxT( "F.Mask" ) },       { wxT( "bottomsolder" ), wxT( "B.Mask" ) },
    { wxT( "keepoutlayer" ), wxT( "Edge.Cuts" ) },
};

} // namespace


ALTIUM_OUTJOB::ALTIUM_OUTJOB( const wxString& aText, const wxString& aOutJobName, const wxString& aProjectName,
                              const std::set<wxString>& aCopperLayers ) :
        m_outJobName( aOutJobName ),
        m_projectName( aProjectName.IsEmpty() ? aOutJobName : aProjectName ),
        m_copperLayers( aCopperLayers )
{
    readSections( aText );
    readOutputs();
    convert();
}


ALTIUM_OUTJOB ALTIUM_OUTJOB::FromFile( const wxString& aPath, const wxString& aProjectName,
                                       const std::set<wxString>& aCopperLayers )
{
    wxFFile  file( aPath, wxT( "rb" ) );
    wxString text;

    if( !file.IsOpened() || !file.ReadAll( &text, wxConvISO8859_1 ) )
        THROW_IO_ERROR( wxString::Format( _( "Cannot read Altium output job '%s'." ), aPath ) );

    return ALTIUM_OUTJOB( text, wxFileName( aPath ).GetName(), aProjectName, aCopperLayers );
}


void ALTIUM_OUTJOB::readSections( const wxString& aText )
{
    // An INI file: [section] headers and key=value lines, which may end in \r\n, \n or \r
    wxString text = aText;
    text.Replace( wxT( "\r\n" ), wxT( "\n" ) );
    text.Replace( wxT( "\r" ), wxT( "\n" ) );

    std::map<wxString, wxString>* section = nullptr;

    for( const wxString& line : wxSplit( text, '\n', 0 ) )
    {
        if( line.length() > 2 && line.StartsWith( wxT( "[" ) ) && line.EndsWith( wxT( "]" ) ) )
        {
            section = &m_sections[line.Mid( 1, line.length() - 2 )];
        }
        else if( section && line.Contains( wxT( "=" ) ) )
        {
            ( *section )[line.BeforeFirst( '=' )] = line.AfterFirst( '=' );
        }
    }
}


void ALTIUM_OUTJOB::readOutputs()
{
    const std::map<wxString, wxString>& group = m_sections[wxT( "OutputGroup1" )];

    // The output mediums (print, generated files, ...) by their number
    std::map<long, wxString> mediums;
    wxRegEx                  mediumKey( wxT( "^OutputMedium([0-9]+)$" ), wxRE_ADVANCED );

    for( const auto& [key, value] : group )
    {
        long number;

        if( mediumKey.Matches( key ) && mediumKey.GetMatch( key, 1 ).ToLong( &number ) )
            mediums[number] = value;
    }

    for( int i = 1; group.count( wxString::Format( wxT( "OutputType%d" ), i ) ); i++ )
    {
        OUTPUT output;
        output.m_Index = i;
        output.m_Type = group.at( wxString::Format( wxT( "OutputType%d" ), i ) );
        output.m_Name = valueOr( group, wxString::Format( wxT( "OutputName%d" ), i ), wxEmptyString );
        output.m_Document = valueOr( group, wxString::Format( wxT( "OutputDocumentPath%d" ), i ), wxEmptyString );
        output.m_Enabled =
                valueOr( group, wxString::Format( wxT( "OutputEnabled%d" ), i ), wxEmptyString ) == wxT( "1" );

        for( const auto& [number, medium] : mediums )
        {
            wxString key = wxString::Format( wxT( "OutputEnabled%d_OutputMedium%ld" ), i, number );

            if( valueOr( group, key, wxEmptyString ) == wxT( "1" ) )
                output.m_Mediums.push_back( medium );
        }

        for( int j = 1; group.count( wxString::Format( wxT( "Configuration%d_Name%d" ), i, j ) ); j++ )
        {
            output.m_Config[group.at( wxString::Format( wxT( "Configuration%d_Name%d" ), i, j ) )] =
                    valueOr( group, wxString::Format( wxT( "Configuration%d_Item%d" ), i, j ), wxEmptyString );
        }

        m_outputs.push_back( output );
    }
}


std::map<wxString, wxString> ALTIUM_OUTJOB::configRecord( const OUTPUT& aOutput, int aParameter ) const
{
    return parseRecord( valueOr( aOutput.m_Config,
                                 wxString::Format( wxT( "OutputConfigurationParameter%d" ), aParameter ),
                                 wxEmptyString ) );
}


std::vector<wxString> ALTIUM_OUTJOB::plotLayers( const wxString& aConfig, std::vector<wxString>& aDropped ) const
{
    std::vector<wxString> layers;

    // The plotted layers are the ids marked ~1 in "Plot.Set=...", which runs to the next '|'
    wxString plotSet;
    size_t   pos = 0;

    while( ( pos = aConfig.find( wxT( "Plot.Set=" ), pos ) ) != wxString::npos )
    {
        wxUniChar before = pos ? aConfig[pos - 1] : wxUniChar( ' ' );

        if( !( ( before >= 'a' && before <= 'z' ) || ( before >= 'A' && before <= 'Z' ) ) )
        {
            plotSet = aConfig.Mid( pos + 9 ).BeforeFirst( '|' );
            break;
        }

        pos++;
    }

    for( size_t i = 0; i < plotSet.length(); )
    {
        if( !( plotSet[i] >= '0' && plotSet[i] <= '9' ) )
        {
            i++;
            continue;
        }

        size_t end = i;

        while( end < plotSet.length() && plotSet[end] >= '0' && plotSet[end] <= '9' )
            end++;

        if( plotSet.Mid( end, 2 ) != wxT( "~1" ) )
        {
            i = end;
            continue;
        }

        long id = 0;
        plotSet.Mid( i, end - i ).ToLong( &id );
        i = end + 2;

        wxString layer;
        wxString why;

        if( auto it = fixedLayers.find( id ); it != fixedLayers.end() )
        {
            layer = it->second;
        }
        else if( id >= 0x01000002 && id <= 0x0100001E )
        {
            wxString name = wxString::Format( wxT( "In%ld.Cu" ), id - 0x01000001 );

            if( m_copperLayers.count( name ) )
                layer = name;
            else
                why = wxString::Format( wxT( "mid layer %ld not in board" ), id - 0x01000001 );
        }
        else if( id >= 0x01010001 && id <= 0x01010010 )
        {
            // the board importer numbers the planes among the mid layers
            why = wxString::Format( wxT( "internal plane %ld: KiCad has no plane id mapping from the OutJob" ),
                                    id - 0x01010000 );
        }
        else if( id >= 0x01020001 && id <= 0x01020020 )
        {
            layer = wxString::Format( wxT( "User.%ld" ), id - 0x01020000 );
        }
        else
        {
            why = wxString::Format( wxT( "unknown layer id %ld (0x%08lX)" ), id, id );
        }

        if( !layer.IsEmpty() && std::find( layers.begin(), layers.end(), layer ) == layers.end() )
            layers.push_back( layer );
        else if( !why.IsEmpty() )
            aDropped.push_back( why );
    }

    return layers;
}


wxString ALTIUM_OUTJOB::jobId( const std::vector<wxString>& aParts ) const
{
    // Fixed namespace: the same OutJob always gives the same ids
    static const boost::uuids::uuid nameSpace =
            boost::uuids::string_generator()( "6f1c3c52-8b35-4d6c-9f55-2a7b6c0d0c11" );

    wxString name;

    for( size_t i = 0; i < aParts.size(); i++ )
        name += ( i ? wxT( "|" ) : wxT( "" ) ) + aParts[i];

    boost::uuids::name_generator_sha1 generator( nameSpace );
    return wxString( boost::uuids::to_string( generator( std::string( name.ToUTF8() ) ) ) );
}


void ALTIUM_OUTJOB::addJob( const OUTPUT& aOutput, const wxString& aType, const nlohmann::json& aSettings,
                            bool aIsFabrication )
{
    EMITTED_JOB job;
    job.m_Id = jobId( { m_outJobName, wxString::Format( wxT( "%d" ), aOutput.m_Index ), aType } );
    job.m_Type = aType;
    job.m_Description = wxString::Format(
            wxT( "%s (Altium %s, %s)" ), aOutput.m_Name.IsEmpty() ? aOutput.m_Type : aOutput.m_Name, aOutput.m_Type,
            aOutput.m_Document.IsEmpty() ? wxString( wxT( "project" ) ) : aOutput.m_Document );
    job.m_Settings = aSettings;
    job.m_IsFabrication = aIsFabrication;
    m_jobs.push_back( job );
}


void ALTIUM_OUTJOB::note( const OUTPUT& aOutput, const wxString& aStatus, const wxString& aDetail )
{
    m_outcomes.push_back( { aOutput.m_Type, aOutput.m_Name, aOutput.m_Document, aOutput.m_Enabled, aStatus, aDetail } );
}


static wxString joined( const std::vector<wxString>& aParts, const wxString& aSeparator )
{
    wxString text;

    for( size_t i = 0; i < aParts.size(); i++ )
        text += ( i ? aSeparator : wxString() ) + aParts[i];

    return text;
}


void ALTIUM_OUTJOB::convertGerber( const OUTPUT& aOutput )
{
    std::map<wxString, wxString> config = configRecord( aOutput );
    std::vector<wxString>        dropped;
    std::vector<wxString>        layers =
            plotLayers( valueOr( aOutput.m_Config, wxT( "OutputConfigurationParameter1" ), wxEmptyString ), dropped );

    if( layers.empty() )
        return note( aOutput, wxT( "omitted" ), wxT( "no layers selected in the plot set" ) );

    long decimals = 5;
    valueOr( config, wxT( "NumberOfDecimals" ), wxT( "5" ) ).ToLong( &decimals );

    const int precision = decimals >= 6 ? 6 : 5;

    nlohmann::json settings = { { "layers", nlohmann::json::array() },
                                { "use_protel_file_extension", true },
                                { "use_x2_format", false },
                                { "include_netlist_attributes", false },
                                { "disable_aperture_macros", false },
                                { "precision", precision },
                                { "create_gerber_job_file", false },
                                { "output_dir", "gerbers" },
                                { "subtract_solder_mask_from_silk", false },
                                { "mirror", false },
                                { "plot_drawing_sheet", false },
                                { "plot_footprint_values", false },
                                { "plot_ref_des", false },
                                { "check_zones", false } };

    for( const wxString& layer : layers )
        settings["layers"].push_back( layer.ToStdString( wxConvUTF8 ) );

    std::vector<wxString> detail = { wxT( "layers " ) + joined( layers, wxT( "," ) ),
                                     wxString::Format( wxT( "Protel file extensions, plain RS-274X (no X2), "
                                                            "precision %d" ),
                                                       precision ) };

    std::set<wxString> droppedOnce( dropped.begin(), dropped.end() );

    for( const wxString& why : droppedOnce )
        detail.push_back( wxT( "dropped: " ) + why );

    const std::vector<std::pair<const wxChar*, const wxChar*>> unmapped = {
        { wxT( "FlashAllFills" ), wxT( "flash all fills" ) },
        { wxT( "PlotPositivePlaneLayers" ), wxT( "positive plane plots" ) },
        { wxT( "Panelize" ), wxT( "panelise" ) },
        { wxT( "SoftwareArcs" ), wxT( "software arcs" ) },
        { wxT( "MirrorDrillDrawingPlots" ), wxT( "mirror drill plots" ) },
    };

    for( const auto& [key, why] : unmapped )
    {
        if( valueOr( config, key, wxT( "False" ) ) == wxT( "True" ) )
            detail.push_back( wxString( why ) + wxT( " not mapped" ) );
    }

    detail.push_back( wxString::Format( wxT( "Altium film size/border, aperture size limits, G54, unit (Imperial) "
                                             "and zero mode: no KiCad equivalent (gerbers are always 4.%d mm), "
                                             "drill drawing symbols not plotted" ),
                                        precision ) );

    if( valueOr( config, wxT( "GenerateDRCRulesFile" ), wxEmptyString ) == wxT( "True" ) )
        detail.push_back( wxT( "Altium DRC rules file (.RUL) beside the gerbers: not produced" ) );

    addJob( aOutput, wxT( "pcb_export_gerbers" ), settings, true );
    note( aOutput, wxT( "converted" ), joined( detail, wxT( "; " ) ) );
}


void ALTIUM_OUTJOB::convertDrill( const OUTPUT& aOutput )
{
    std::map<wxString, wxString> config = configRecord( aOutput );

    static const std::map<wxString, wxString> zeroModes = {
        { wxT( "KeepLeadingAndTrailingZeroes" ), wxT( "keep_zeros" ) },
        { wxT( "SuppressLeadingZeroes" ), wxT( "surpress_leading" ) },
        { wxT( "SuppressTrailingZeroes" ), wxT( "surpress_trailing" ) },
    };

    wxString zeros = valueOr( zeroModes, valueOr( config, wxT( "ZeroesMode" ), wxEmptyString ), wxT( "decimal" ) );
    bool     relative = valueOr( config, wxT( "OriginPosition" ), wxEmptyString ) == wxT( "Relative" );
    bool     imperial = valueOr( config, wxT( "Units" ), wxEmptyString ) == wxT( "Imperial" );
    bool combined = valueOr( config, wxT( "GenerateSeparatePlatedNonPlatedFiles" ), wxT( "False" ) ) != wxT( "True" );
    bool ovalRoute = valueOr( config, wxT( "GenerateDrilledSlotsG85" ), wxT( "False" ) ) != wxT( "True" );

    nlohmann::json settings = { { "format", "excellon" },
                                { "drill_origin", relative ? "plot" : "abs" },
                                { "units", imperial ? "in" : "mm" },
                                { "zero_format", zeros.ToStdString( wxConvUTF8 ) },
                                { "excellon.combine_pth_npth", combined },
                                { "excellon.oval_drill_route", ovalRoute },
                                { "excellon.mirror_y", false },
                                { "excellon.minimal_header", false },
                                { "generate_map", false },
                                { "generate_tenting", false },
                                { "generate_report", false },
                                { "output_dir", "drill" } };

    std::vector<wxString> detail = { wxString::Format( wxT( "Excellon, %s, %s, one PTH+NPTH file%s" ),
                                                       imperial ? wxT( "in" ) : wxT( "mm" ), zeros,
                                                       combined ? wxT( "" ) : wxT( " (separate)" ) ) };

    if( relative )
    {
        detail.push_back( wxT( "Altium \"relative\" origin mapped to KiCad drill/place file origin (set it in the "
                               "board to match)" ) );
    }

    if( valueOr( config, wxT( "GenerateBoardEdgeRout" ), wxEmptyString ) == wxT( "True" ) )
        detail.push_back( wxT( "board edge route file not produced" ) );

    if( valueOr( config, wxT( "GenerateEIADrillFile" ), wxEmptyString ) == wxT( "True" ) )
        detail.push_back( wxT( "EIA drill file not produced" ) );

    detail.push_back( wxT( "Altium 2:" ) + valueOr( config, wxT( "NumberOfDecimals" ), wxT( "5" ) )
                      + wxT( " digit format fixed by KiCad (in: 3.3, mm: 3.3 / 2.4)" ) );

    addJob( aOutput, wxT( "pcb_export_drill" ), settings, true );

    if( aOutput.m_Mediums.empty() )
    {
        note( aOutput, wxT( "converted-no-medium" ),
              joined( detail, wxT( "; " ) ) + wxT( "; Altium has no output medium ticked for it" ) );
    }
    else
    {
        note( aOutput, wxT( "converted" ), joined( detail, wxT( "; " ) ) );
    }
}


void ALTIUM_OUTJOB::convertSchematicPrint( const OUTPUT& aOutput )
{
    const std::map<wxString, wxString>& publish = m_sections[wxT( "PublishSettings" )];

    wxString outputName = valueOr( publish, wxT( "OutputFileName2" ), wxEmptyString );

    if( outputName.IsEmpty() )
        outputName = m_projectName + wxT( ".pdf" );

    // the file name alone, whichever separator the path uses
    wxString fileName = outputName.AfterLast( '\\' ).AfterLast( '/' );

    nlohmann::json settings = {
        { "format", "pdf" },
        { "plot_all", true },
        { "plot_drawing_sheet", true },
        { "black_and_white", false },
        { "pdf_hierarchical_links", valueOr( publish, wxT( "MarkNetLabels2" ), wxT( "1" ) ) == wxT( "1" ) },
        { "pdf_property_popups", valueOr( publish, wxT( "ShowComponentParameters2" ), wxT( "1" ) ) == wxT( "1" ) },
        { "pdf_metadata", true },
        { "output_filename", fileName.ToStdString( wxConvUTF8 ) }
    };

    addJob( aOutput, wxT( "sch_export_plot_pdf" ), settings, false );
    note( aOutput, wxT( "converted" ),
          wxT( "all sheets to one PDF " ) + fileName
                  + wxT( "; Altium print scale/offset, ERC marker and parameter-set display options: not mapped; "
                         "Altium \"Fit to document\" -> sheet size" ) );
}


void ALTIUM_OUTJOB::convertPcbPrint( const OUTPUT& aOutput )
{
    std::vector<wxString> layers;
    wxRegEx               mechanical( wxT( "^Mechanical([0-9]+)$" ), wxRE_ADVANCED );

    for( int n = 3; n < 40; n++ )
    {
        std::map<wxString, wxString> record = configRecord( aOutput, n );

        if( valueOr( record, wxT( "Record" ), wxEmptyString ) != wxT( "PcbPrintLayer" ) )
            continue;

        wxString altiumLayer = valueOr( record, wxT( "Layer" ), wxEmptyString );
        wxString letters;

        for( wxUniChar ch : altiumLayer.Lower() )
        {
            if( ch >= 'a' && ch <= 'z' )
                letters += ch;
        }

        wxString layer = valueOr( namedLayers, letters, wxEmptyString );

        if( layer.IsEmpty() && mechanical.Matches( altiumLayer ) )
            layer = wxT( "User." ) + mechanical.GetMatch( altiumLayer, 1 );

        if( !layer.IsEmpty() && std::find( layers.begin(), layers.end(), layer ) == layers.end() )
            layers.push_back( layer );
    }

    nlohmann::json settings = { { "layers", nlohmann::json::array() },
                                { "pdf_gen_mode", "all-layers-one-file" },
                                { "scale", 1.0 },
                                { "black_and_white", false },
                                { "mirror", false },
                                { "plot_drawing_sheet", false },
                                { "output_filename",
                                  ( m_projectName + wxT( "-pcb.pdf" ) ).ToStdString( wxConvUTF8 ) } };

    for( const wxString& layer : layers )
        settings["layers"].push_back( layer.ToStdString( wxConvUTF8 ) );

    addJob( aOutput, wxT( "pcb_export_pdf" ), settings, false );
    note( aOutput, wxT( "converted" ),
          wxT( "layers " ) + joined( layers, wxT( "," ) )
                  + wxT( "; Altium print area, per-object display modes (Full/Draft/Hidden) and multilayer "
                         "composite print setup not mapped" ) );
}


void ALTIUM_OUTJOB::convert()
{
    for( const OUTPUT& output : m_outputs )
    {
        if( !output.m_Enabled )
        {
            note( output, wxT( "skipped-disabled" ), wxT( "output is disabled in the OutJob" ) );
            continue;
        }

        // Altium's type names, in lower case with runs of other characters made one underscore
        wxString type;

        for( wxUniChar ch : output.m_Type.Lower() )
        {
            bool word = ( ch >= 'a' && ch <= 'z' ) || ( ch >= '0' && ch <= '9' );

            if( word )
                type += ch;
            else if( !type.EndsWith( wxT( "_" ) ) )
                type += '_';
        }

        while( type.StartsWith( wxT( "_" ) ) )
            type.Remove( 0, 1 );

        while( type.EndsWith( wxT( "_" ) ) )
            type.RemoveLast();

        const std::string utf8Project = m_projectName.ToStdString( wxConvUTF8 );

        if( type == wxT( "gerber" ) )
        {
            convertGerber( output );
        }
        else if( type == wxT( "nc_drill" ) )
        {
            convertDrill( output );
        }
        else if( type == wxT( "schematic_print" ) )
        {
            convertSchematicPrint( output );
        }
        else if( type == wxT( "pcb_print" ) )
        {
            convertPcbPrint( output );
        }
        else if( type == wxT( "compositedrill" ) )
        {
            note( output, wxT( "omitted" ),
                  wxT( "composite drill drawing: closest is the drill map (generate_map in the drill job), not "
                       "converted" ) );
        }
        else if( type == wxT( "pick_place_files" ) )
        {
            addJob( output, wxT( "pcb_export_pos" ),
                    { { "format", "csv" },
                      { "units", "mm" },
                      { "side", "both" },
                      { "smd_only", false },
                      { "exclude_dnp", false },
                      { "single_file", true },
                      { "output_filename", utf8Project + "-pos.csv" } },
                    true );
            note( output, wxT( "converted" ), wxT( "default CSV position file (no samples in corpus)" ) );
        }
        else if( type == wxT( "bill_of_materials" ) )
        {
            addJob( output, wxT( "sch_export_bom" ), { { "output_filename", utf8Project + "-bom.csv" } }, false );
            note( output, wxT( "converted" ),
                  wxT( "default KiCad BOM (Altium column/grouping settings not mapped; no samples)" ) );
        }
        else if( type == wxT( "odb_files" ) )
        {
            addJob( output, wxT( "pcb_export_odb" ),
                    { { "units", "mm" },
                      { "precision", 2 },
                      { "compression", "zip" },
                      { "output_filename", utf8Project + "-odb.zip" } },
                    true );
            note( output, wxT( "converted" ), wxT( "default ODB++ (no samples in corpus)" ) );
        }
        else if( type == wxT( "step" ) )
        {
            addJob( output, wxT( "pcb_export_3d" ),
                    { { "format", "step" }, { "output_filename", utf8Project + ".step" } }, false );
            note( output, wxT( "converted" ), wxT( "default STEP export (no samples in corpus)" ) );
        }
        else
        {
            note( output, wxT( "omitted" ),
                  wxString::Format( wxT( "no KiCad job for Altium output type '%s'" ), output.m_Type ) );
        }
    }
}


nlohmann::json ALTIUM_OUTJOB::GetJobset() const
{
    nlohmann::json jobs = nlohmann::json::array();
    nlohmann::json outputs = nlohmann::json::array();

    for( const EMITTED_JOB& job : m_jobs )
    {
        jobs.push_back( { { "id", job.m_Id.ToStdString( wxConvUTF8 ) },
                          { "type", job.m_Type.ToStdString( wxConvUTF8 ) },
                          { "description", job.m_Description.ToStdString( wxConvUTF8 ) },
                          { "settings", job.m_Settings } } );
    }

    for( bool fabrication : { true, false } )
    {
        nlohmann::json only = nlohmann::json::array();

        for( const EMITTED_JOB& job : m_jobs )
        {
            if( job.m_IsFabrication == fabrication )
                only.push_back( job.m_Id.ToStdString( wxConvUTF8 ) );
        }

        if( only.empty() )
            continue;

        const wxString group = fabrication ? wxT( "fab" ) : wxT( "docs" );

        outputs.push_back( { { "id", jobId( { m_outJobName, wxT( "dest" ), group } ).ToStdString( wxConvUTF8 ) },
                             { "type", "folder" },
                             { "only", only },
                             { "description", fabrication ? "Fabrication outputs" : "Documents" },
                             { "settings", { { "output_path", fabrication ? "fabrication" : "documents" } } } } );
    }

    return { { "meta", { { "version", 1 } } }, { "jobs", jobs }, { "outputs", outputs } };
}


bool ALTIUM_OUTJOB::SaveJobset( const wxString& aPath ) const
{
    nlohmann::json json = GetJobset();
    JOBSET         jobset( aPath );

    for( const nlohmann::json& jobJson : json.at( "jobs" ) )
    {
        JOBSET_JOB job;
        from_json( jobJson, job );
        jobset.GetJobs().push_back( job );
    }

    for( const nlohmann::json& destinationJson : json.at( "outputs" ) )
    {
        JOBSET_DESTINATION destination;
        from_json( destinationJson, destination );
        jobset.GetDestinations().push_back( destination );
    }

    // A new settings file records the path it was created at, which means nothing once the
    // project moves, and differs between two imports of one project
    if( jobset.Internals()->contains( "meta" ) )
        jobset.Internals()->at( "meta" ).erase( "filename" );

    return jobset.SaveToFile( wxEmptyString, true );
}

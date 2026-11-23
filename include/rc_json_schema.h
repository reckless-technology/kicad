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

#ifndef RC_JSON_SCHEMA_H
#define RC_JSON_SCHEMA_H

#include <json_common.h>
#include <wx/string.h>
#include <optional>
#include <vector>
#include <json_conversions.h>

/**
 * Contains the json serialization structs for DRC and ERC reports
 * If you are trying to change the output schema
 * Please update the schemas located in /resources/schemas/ as both documentation
 * and use by end user implementations
 */
namespace RC_JSON
{
struct COORDINATE
{
    double x;
    double y;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( COORDINATE, x, y )


struct AFFECTED_ITEM
{
    wxString uuid;
    wxString description;
    COORDINATE pos;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( AFFECTED_ITEM, uuid, description, pos )


struct VIOLATION
{
    wxString                   type;
    wxString                   description;
    wxString                   severity;
    std::vector<AFFECTED_ITEM> items;
    std::optional<COORDINATE>  pos;
    wxString                   layer;
    bool                       excluded;
    wxString                   comment;     // exclusion comment; if any
};

inline void to_json( nlohmann::json& aJson, const VIOLATION& aViolation )
{
    aJson["type"] = aViolation.type;
    aJson["description"] = aViolation.description;
    aJson["severity"] = aViolation.severity;
    aJson["items"] = aViolation.items;

    if( aViolation.pos )
        aJson["pos"] = *aViolation.pos;

    if( !aViolation.layer.IsEmpty() )
        aJson["layer"] = aViolation.layer;

    if( aViolation.excluded )
    {
        aJson["excluded"] = aViolation.excluded;
        aJson["comment"] = aViolation.comment;
    }
}

inline void from_json( const nlohmann::json& aJson, VIOLATION& aViolation )
{
    aJson.at( "type" ).get_to( aViolation.type );
    aJson.at( "description" ).get_to( aViolation.description );
    aJson.at( "severity" ).get_to( aViolation.severity );
    aJson.at( "items" ).get_to( aViolation.items );

    if( aJson.contains( "pos" ) )
        aViolation.pos = aJson.at( "pos" ).get<COORDINATE>();
    else
        aViolation.pos.reset();

    if( aJson.contains( "layer" ) )
        aJson.at( "layer" ).get_to( aViolation.layer );
    else
        aViolation.layer.clear();

    aJson.at( "excluded" ).get_to( aViolation.excluded );
    aJson.at( "comment" ).get_to( aViolation.comment );
}


struct IGNORED_CHECK
{
    wxString key;
    wxString description;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( IGNORED_CHECK, key, description )


struct REPORT_BASE
{
    wxString schema;
    wxString source;
    wxString date;
    wxString kicad_version;
    wxString type;
    wxString coordinate_units;
};


struct DRC_REPORT : REPORT_BASE
{
    DRC_REPORT() { type = wxS( "drc" ); }

    std::vector<VIOLATION>                 violations;
    std::vector<VIOLATION>                 unconnected_items;
    std::vector<VIOLATION>                 schematic_parity;
    std::vector<wxString>                  included_severities;
    std::vector<IGNORED_CHECK>             ignored_checks;
};

// The member is named schema because $ is not valid in a C++ identifier; the JSON key stays "$schema".
inline void to_json( nlohmann::json& aJson, const DRC_REPORT& aReport )
{
    aJson["$schema"] = aReport.schema;
    aJson["source"] = aReport.source;
    aJson["date"] = aReport.date;
    aJson["kicad_version"] = aReport.kicad_version;
    aJson["violations"] = aReport.violations;
    aJson["unconnected_items"] = aReport.unconnected_items;
    aJson["schematic_parity"] = aReport.schematic_parity;
    aJson["coordinate_units"] = aReport.coordinate_units;
    aJson["included_severities"] = aReport.included_severities;
    aJson["ignored_checks"] = aReport.ignored_checks;
}

inline void from_json( const nlohmann::json& aJson, DRC_REPORT& aReport )
{
    aJson.at( "$schema" ).get_to( aReport.schema );
    aJson.at( "source" ).get_to( aReport.source );
    aJson.at( "date" ).get_to( aReport.date );
    aJson.at( "kicad_version" ).get_to( aReport.kicad_version );
    aJson.at( "violations" ).get_to( aReport.violations );
    aJson.at( "unconnected_items" ).get_to( aReport.unconnected_items );
    aJson.at( "schematic_parity" ).get_to( aReport.schematic_parity );
    aJson.at( "coordinate_units" ).get_to( aReport.coordinate_units );
    aJson.at( "included_severities" ).get_to( aReport.included_severities );
    aJson.at( "ignored_checks" ).get_to( aReport.ignored_checks );
}


struct ERC_SHEET
{
    wxString               uuid_path;
    wxString               path;
    std::vector<VIOLATION> violations;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( ERC_SHEET, uuid_path, path, violations )


struct ERC_REPORT : REPORT_BASE
{
    ERC_REPORT() { type = wxS( "erc" ); }

    std::vector<ERC_SHEET>     sheets;
    std::vector<wxString>      included_severities;
    std::vector<IGNORED_CHECK> ignored_checks;
};

// See DRC_REPORT above for why the JSON key is "$schema" and the member is schema.
inline void to_json( nlohmann::json& aJson, const ERC_REPORT& aReport )
{
    aJson["$schema"] = aReport.schema;
    aJson["source"] = aReport.source;
    aJson["date"] = aReport.date;
    aJson["kicad_version"] = aReport.kicad_version;
    aJson["sheets"] = aReport.sheets;
    aJson["coordinate_units"] = aReport.coordinate_units;
    aJson["included_severities"] = aReport.included_severities;
    aJson["ignored_checks"] = aReport.ignored_checks;
}

inline void from_json( const nlohmann::json& aJson, ERC_REPORT& aReport )
{
    aJson.at( "$schema" ).get_to( aReport.schema );
    aJson.at( "source" ).get_to( aReport.source );
    aJson.at( "date" ).get_to( aReport.date );
    aJson.at( "kicad_version" ).get_to( aReport.kicad_version );
    aJson.at( "sheets" ).get_to( aReport.sheets );
    aJson.at( "coordinate_units" ).get_to( aReport.coordinate_units );
    aJson.at( "included_severities" ).get_to( aReport.included_severities );
    aJson.at( "ignored_checks" ).get_to( aReport.ignored_checks );
}

} // namespace RC_JSON

#endif

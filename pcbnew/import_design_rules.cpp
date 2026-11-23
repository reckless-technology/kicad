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

#include "import_design_rules.h"

#include <reporter.h>
#include <wildcards_and_files_ext.h>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/log.h>


bool SaveImportedDesignRules( const wxString& aRules, const wxFileName& aBoardPath, REPORTER& aReporter )
{
    if( aRules.IsEmpty() )
        return false;

    // a nameless board would put the rules in a hidden file
    if( aBoardPath.GetName().IsEmpty() )
    {
        aReporter.Report( _( "Imported design rules were not saved, the board has no name.\n" ), RPT_SEVERITY_WARNING );
        return false;
    }

    wxFileName rulesPath( aBoardPath );
    rulesPath.SetExt( FILEEXT::DesignRulesFileExtension );

    // Creating exclusively both refuses an existing file and closes the race a FileExists test would leave open
    wxFile rulesFile;

    {
        // Refusal is expected and reported below, so keep wx from raising its own error
        wxLogNull noLog;
        rulesFile.Open( rulesPath.GetFullPath(), wxFile::write_excl );
    }

    if( !rulesFile.IsOpened() )
    {
        aReporter.Report( wxString::Format( rulesPath.FileExists()
                                                    ? _( "Imported design rules were not saved, '%s' already exists.\n" )
                                                    : _( "Could not write imported design rules to '%s'\n" ),
                                            rulesPath.GetFullPath() ),
                          RPT_SEVERITY_WARNING );
        return false;
    }

    bool written = rulesFile.Write( aRules );

    if( !rulesFile.Close() )
        written = false;

    // A partial file would not parse, and because it is created exclusively it would block the next import
    if( !written )
    {
        wxRemoveFile( rulesPath.GetFullPath() );

        aReporter.Report( wxString::Format( _( "Could not write imported design rules to '%s'\n" ),
                                            rulesPath.GetFullPath() ),
                          RPT_SEVERITY_WARNING );
        return false;
    }

    return true;
}


wxString RenameNetsInDesignRules( const wxString& aRules, const std::map<wxString, wxString>& aNames )
{
    if( aNames.empty() )
        return aRules;

    const wxString property = wxS( ".NetName" );

    wxString result;
    size_t   copied = 0;
    size_t   at = 0;

    while( ( at = aRules.find( property, at ) ) != wxString::npos )
    {
        size_t cursor = at + property.length();
        at = cursor;

        auto skipSpaces = [&]()
        {
            while( cursor < aRules.length() && ( aRules[cursor] == ' ' || aRules[cursor] == '\t' ) )
                cursor++;
        };

        skipSpaces();

        // only a comparison with a literal names a net: `A.NetName == B.NetName` names none
        if( cursor + 1 >= aRules.length() || ( aRules[cursor] != '=' && aRules[cursor] != '!' )
            || aRules[cursor + 1] != '=' )
        {
            continue;
        }

        cursor += 2;
        skipSpaces();

        if( cursor >= aRules.length() || aRules[cursor] != '\'' )
            continue;

        const size_t literalStart = ++cursor;
        wxString     name;

        for( ; cursor < aRules.length() && aRules[cursor] != '\''; cursor++ )
        {
            if( aRules[cursor] == '\\' && cursor + 1 < aRules.length() )
                cursor++;

            name += aRules[cursor];
        }

        if( cursor >= aRules.length() )
            break;

        auto renamed = aNames.find( name );

        if( renamed != aNames.end() )
        {
            wxString escaped = renamed->second;
            escaped.Replace( wxS( "'" ), wxS( "\\'" ) );

            result += aRules.Mid( copied, literalStart - copied ) + escaped;
            copied = cursor;
        }

        at = cursor;
    }

    return result + aRules.Mid( copied );
}


bool RenameNetsInDesignRulesFile( const wxFileName& aRulesPath, const std::map<wxString, wxString>& aNames,
                                  REPORTER& aReporter )
{
    if( aNames.empty() || !aRulesPath.FileExists() )
        return false;

    wxString rules;

    {
        wxFile rulesFile( aRulesPath.GetFullPath() );

        if( !rulesFile.IsOpened() || !rulesFile.ReadAll( &rules, wxConvUTF8 ) )
        {
            aReporter.Report( wxString::Format( _( "Could not read the design rules '%s' to rename the nets they "
                                                   "name.\n" ),
                                                aRulesPath.GetFullPath() ),
                              RPT_SEVERITY_WARNING );
            return false;
        }
    }

    wxString renamed = RenameNetsInDesignRules( rules, aNames );

    if( renamed == rules )
        return false;

    wxFile rulesFile( aRulesPath.GetFullPath(), wxFile::write );

    if( !rulesFile.IsOpened() || !rulesFile.Write( renamed, wxConvUTF8 ) || !rulesFile.Close() )
    {
        aReporter.Report( wxString::Format( _( "Could not write the design rules '%s' with the renamed nets.\n" ),
                                            aRulesPath.GetFullPath() ),
                          RPT_SEVERITY_WARNING );
        return false;
    }

    return true;
}

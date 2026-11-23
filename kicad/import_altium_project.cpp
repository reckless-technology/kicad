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

#include "import_altium_project.h"

#include <import_proj_properties.h>
#include <io/altium/altium_outjob.h>
#include <io/io_utils.h>
#include <ki_exception.h>
#include <pcb_io/pcb_io_mgr.h>
#include <wildcards_and_files_ext.h>

#include <wx/fileconf.h>


/// The PCB_IO_MGR plugin type of an Altium board document, or -1 for another document
static int altiumBoardType( const wxFileName& aFile )
{
    const wxString ext = aFile.GetExt();

    if( !ext.CmpNoCase( wxS( "PcbDoc" ) ) )
        return PCB_IO_MGR::ALTIUM_DESIGNER;

    // Older Altium (and Protel) projects keep the board as a binary .pcb, a compound file
    // holding the same records as a .PcbDoc.  The header keeps other .pcb formats out.
    if( !ext.CmpNoCase( wxS( "pcb" ) )
        && IO_UTILS::fileHasBinaryHeader( aFile.GetFullPath(), IO_UTILS::COMPOUND_FILE_HEADER ) )
    {
        return PCB_IO_MGR::ALTIUM_DESIGNER;
    }

    if( !ext.CmpNoCase( wxS( "CSPcbDoc" ) ) )
        return PCB_IO_MGR::ALTIUM_CIRCUIT_STUDIO;

    if( !ext.CmpNoCase( wxS( "CMPcbDoc" ) ) )
        return PCB_IO_MGR::ALTIUM_CIRCUIT_MAKER;

    if( !ext.CmpNoCase( wxS( "SWPcbDoc" ) ) )
        return PCB_IO_MGR::SOLIDWORKS_PCB;

    return -1;
}


ALTIUM_PROJECT_FILES ReadAltiumProjectFiles( const wxFileName& aProjectFile )
{
    ALTIUM_PROJECT_FILES files;
    files.m_boardType = PCB_IO_MGR::ALTIUM_DESIGNER;

    wxFileConfig config( wxEmptyString, wxEmptyString, wxEmptyString, aProjectFile.GetFullPath(),
                         wxCONFIG_USE_NO_ESCAPE_CHARACTERS );

    wxString groupname;
    long     groupid;

    for( bool more = config.GetFirstGroup( groupname, groupid ); more;
         more = config.GetNextGroup( groupname, groupid ) )
    {
        if( !groupname.StartsWith( wxS( "Document" ) ) )
            continue;

        wxString number = groupname.Mid( 8 );
        long     docNumber;

        if( !number.ToLong( &docNumber ) )
            continue;

        wxString path = config.Read( groupname + wxS( "/DocumentPath" ), wxEmptyString );

        if( path.empty() )
            continue;

        wxFileName fname( path, wxPATH_WIN );

        if( !fname.IsAbsolute() )
            fname.MakeAbsolute( aProjectFile.GetPath() );

        int boardType = altiumBoardType( fname );

        if( boardType >= 0 && files.m_board.empty() )
        {
            files.m_board = fname.GetFullPath();
            files.m_boardType = boardType;
        }

        if( !fname.GetExt().CmpNoCase( "SCHDOC" ) )
            files.m_schematics.insert( fname.GetFullPath() );

        if( !fname.GetExt().CmpNoCase( "PCBLIB" ) )
            files.m_footprintLibraries.insert( fname.GetFullPath() );

        if( !fname.GetExt().CmpNoCase( "SCHLIB" ) )
            files.m_symbolLibraries.insert( fname.GetFullPath() );

        if( !fname.GetExt().CmpNoCase( "OUTJOB" ) && fname.FileExists() )
            files.m_outputJobs.push_back( fname.GetFullPath() );
    }

    return files;
}


/// The library nicknames the import gives the libraries at \p aPaths, joined for a property
static wxString joinLibraryNicknames( const std::set<wxString>& aPaths )
{
    wxArrayString nicks;

    for( const wxString& path : aPaths )
    {
        wxString nick = wxFileName( path ).GetName();

        if( !nick.IsEmpty() )
            nicks.Add( nick );
    }

    return IMPORT_PROJ_PROPS::JoinList( nicks );
}


std::map<std::string, UTF8> AltiumProjectImportProperties( const ALTIUM_PROJECT_FILES& aFiles,
                                                           const wxFileName&           aProjectFile,
                                                           const wxString&             aTargetStem )
{
    std::map<std::string, UTF8> properties;

    // both imports must agree on the cache nicknames up front
    wxString stem = aTargetStem.IsEmpty() ? aProjectFile.GetName() : aTargetStem;

    properties[IMPORT_PROJ_PROPS::FP_CACHE_NICKNAME] = IMPORT_PROJ_PROPS::MakeCacheNickname( stem );
    properties[IMPORT_PROJ_PROPS::SYM_CACHE_NICKNAME] = IMPORT_PROJ_PROPS::MakeSymbolCacheNickname( stem );

    // pass source fp libs to both imports so found footprints relink to source, not the cache
    properties[IMPORT_PROJ_PROPS::SOURCE_FP_LIBS] = joinLibraryNicknames( aFiles.m_footprintLibraries );

    // and the symbol libraries, so only symbols from these keep their source library's nickname
    properties[IMPORT_PROJ_PROPS::SOURCE_SYM_LIBS] = joinLibraryNicknames( aFiles.m_symbolLibraries );

    properties["project_file"] = aProjectFile.GetFullPath();

    int ii = 0;

    for( const wxString& path : aFiles.m_schematics )
        properties["sch" + std::to_string( ii++ )] = path.ToStdString();

    return properties;
}


void ConvertAltiumOutputJobs( const std::vector<wxString>& aOutputJobs, const wxFileName& aTargetProject,
                              const std::set<wxString>& aCopperLayers,
                              const std::function<void( const wxString&, const wxString& )>& aOnError )
{
    const wxString projectName = aTargetProject.GetName();

    for( const wxString& outJobPath : aOutputJobs )
    {
        wxString stem = projectName;

        if( aOutputJobs.size() > 1 )
            stem += wxT( "-" ) + wxFileName( outJobPath ).GetName();

        wxFileName jobsetFile( aTargetProject.GetPath(), stem, FILEEXT::KiCadJobSetFileExtension );

        try
        {
            ALTIUM_OUTJOB outJob = ALTIUM_OUTJOB::FromFile( outJobPath, projectName, aCopperLayers );

            // An output job KiCad can make nothing of would leave an empty jobset
            if( outJob.HasJobs() && !jobsetFile.FileExists() )
                outJob.SaveJobset( jobsetFile.GetFullPath() );
        }
        catch( const IO_ERROR& error )
        {
            aOnError( outJobPath, error.What() );
        }
    }
}

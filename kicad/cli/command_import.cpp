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
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "command_import.h"
#include "import_altium_project.h"
#include <cli/exit_codes.h>
#include <jobs/job_pcb_import.h>
#include <jobs/job_sch_import.h>
#include <jobs/job_import_utils.h>
#include <ki_exception.h>
#include <pgm_base.h>
#include <settings/settings_manager.h>
#include <project.h>
#include <project/project_file.h>
#include <reporter.h>
#include <kiid.h>
#include <string_utils.h>
#include <macros.h>
#include <wildcards_and_files_ext.h>
#include <wx/crt.h>
#include <wx/filename.h>

#include <string>
#include <optional>
#include <set>
#include <vector>


#define ARG_LAYER_MAP "--layer-map"


CLI::IMPORT_COMMAND::IMPORT_COMMAND() : COMMAND( "import" )
{
    m_argParser.add_description( UTF8STDSTR(
            _( "Import non-KiCad board and/or schematic files into a new KiCad project" ) ) );

    m_argParser.add_argument( ARG_INPUT )
            .help( UTF8STDSTR( _( "Input file(s) to import; each is autodetected as a board or "
                                  "schematic.  An Altium project (.PrjPcb), given alone, is imported "
                                  "whole, as File > Import Non-KiCad Project does" ) ) )
            .nargs( argparse::nargs_pattern::at_least_one )
            .metavar( "INPUT_FILES" );

    m_argParser.add_argument( "-o", ARG_OUTPUT )
            .default_value( std::string() )
            .help( UTF8STDSTR( _( "Output project path stem; produces <stem>.kicad_pro plus "
                                  "imported board and/or schematic files beside it. Defaults to the first "
                                  "input's name in the current directory." ) ) )
            .metavar( "PROJECT" );

    m_argParser.add_argument( ARG_LAYER_MAP )
            .default_value( std::string( "" ) )
            .help( UTF8STDSTR( _( "JSON file mapping source layer names to KiCad layer names for "
                                  "imported boards; unmapped layers use the automatic best-guess" ) ) )
            .metavar( "FILE" );
}


int CLI::IMPORT_COMMAND::doPerform( KIWAY& aKiway )
{
    std::vector<std::string> rawInputs = m_argParser.get<std::vector<std::string>>( ARG_INPUT );
    wxString outputArg = From_UTF8( m_argParser.get<std::string>( ARG_OUTPUT ).c_str() );

    if( rawInputs.empty() )
    {
        wxFprintf( stderr, _( "At least one input file is required\n" ) );
        return EXIT_CODES::ERR_ARGS;
    }

    std::vector<wxString> inputFiles;

    for( const std::string& raw : rawInputs )
        inputFiles.push_back( From_UTF8( raw.c_str() ) );

    std::map<wxString, wxString> layerMap;
    wxString layerMapFile = From_UTF8( m_argParser.get<std::string>( ARG_LAYER_MAP ).c_str() );

    if( !layerMapFile.IsEmpty() )
    {
        wxString error;

        if( !LoadLayerMapFile( layerMapFile, layerMap, error ) )
        {
            wxFprintf( stderr, wxS( "%s\n" ), error );
            return EXIT_CODES::ERR_ARGS;
        }
    }

    wxFileName projectFn;

    if( !outputArg.IsEmpty() )
    {
        projectFn.Assign( outputArg );

        // The argument is a stem, so a dot in it is part of the name unless it names a project file
        if( projectFn.HasExt() && projectFn.GetExt() != FILEEXT::ProjectFileExtension )
            projectFn.SetName( projectFn.GetFullName() );
    }
    else
    {
        wxFileName firstInput( inputFiles.front() );
        projectFn.AssignDir( wxFileName::GetCwd() );
        projectFn.SetName( firstInput.GetName() );
    }

    projectFn.SetExt( FILEEXT::ProjectFileExtension );
    projectFn.MakeAbsolute();

    if( !projectFn.DirExists()
        && !wxFileName::Mkdir( projectFn.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL ) )
    {
        wxFprintf( stderr, _( "Could not create output directory: %s\n" ), projectFn.GetPath() );
        return EXIT_CODES::ERR_INVALID_OUTPUT_CONFLICT;
    }

    wxString stem = projectFn.GetName();
    wxString dir = projectFn.GetPath();

    wxFileName boardFn( dir, stem, FILEEXT::KiCadPcbFileExtension );
    wxFileName schFn( dir, stem, FILEEXT::KiCadSchematicFileExtension );

    // LoadProject returns false for a not-yet-existing file but still registers a fully-defaulted
    // active project, so success is signalled by GetProject() returning it.
    SETTINGS_MANAGER& mgr = Pgm().GetSettingsManager();

    mgr.LoadProject( projectFn.GetFullPath(), true );

    PROJECT* project = mgr.GetProject( projectFn.GetFullPath() );

    if( !project )
    {
        wxFprintf( stderr, _( "Could not create project: %s\n" ), projectFn.GetFullPath() );
        return EXIT_CODES::ERR_UNKNOWN;
    }
    REPORTER& reporter = CLI_REPORTER::GetInstance();

    bool haveBoard = false;
    bool haveSch = false;
    std::vector<FILE_INFO_PAIR> boards;
    int  retCode = EXIT_CODES::SUCCESS;

    std::map<wxString, wxString> netNameMap;
    wxString boardInput;

    // The boards of a multi-board archive: imported once the schematic has supplied its names, as a
    // single board is
    struct DEFERRED_BOARD
    {
        wxString input;
        wxString pcbId;
        wxString outputPath;
        bool     isPartOfMultiBoardProject;
    };

    std::vector<DEFERRED_BOARD> deferredBoards;
    std::set<wxString>          copperLayers;

    // The schematic's netlist, from which the board is linked to its symbols as the project
    // manager's import links it
    wxString netlistFile = wxFileName::CreateTempFileName( wxS( "kicad-import-netlist" ) );
    bool     haveNetlist = false;

    // An Altium project names its sheets, board, libraries and output jobs, and carries parameters,
    // variants and the net identifier scope; it is imported as the project manager's File > Import
    // Non-KiCad Project > Altium Project imports it
    std::optional<ALTIUM_PROJECT_FILES> altiumProject;
    std::map<std::string, UTF8>         projectProperties;
    wxString                            sourceProjectDir;

    if( inputFiles.size() == 1 && wxFileName( inputFiles.front() ).GetExt().CmpNoCase( wxS( "PrjPcb" ) ) == 0 )
    {
        wxFileName altiumProjectFn( inputFiles.front() );
        altiumProjectFn.MakeAbsolute();

        altiumProject = ReadAltiumProjectFiles( altiumProjectFn );
        projectProperties = AltiumProjectImportProperties( *altiumProject, altiumProjectFn, stem );
        sourceProjectDir = altiumProjectFn.GetPath();

        if( !altiumProject->m_schematics.empty() )
        {
            JOB_SCH_IMPORT schJob;
            schJob.m_inputFile = altiumProjectFn.GetFullPath();
            schJob.m_format = JOB_SCH_IMPORT::FORMAT::ALTIUM;
            schJob.m_properties = projectProperties;
            schJob.m_sourceProjectDir = sourceProjectDir;
            schJob.m_localLibraries.assign( altiumProject->m_symbolLibraries.begin(),
                                            altiumProject->m_symbolLibraries.end() );
            schJob.SetConfiguredOutputPath( schFn.GetFullPath() );
            schJob.m_netlistFile = netlistFile;

            retCode = aKiway.ProcessJob( KIWAY::FACE_SCH, &schJob, &reporter );

            if( retCode == EXIT_CODES::SUCCESS )
            {
                haveSch = true;
                netNameMap = std::move( schJob.m_netNameMap );
                haveNetlist = !netlistFile.IsEmpty() && wxFileName( netlistFile ).GetSize() > 0;
            }
        }

        if( retCode == EXIT_CODES::SUCCESS && !altiumProject->m_board.empty() )
        {
            boards.emplace_back( KIID(), boardFn.GetFullName() );
            haveBoard = true;
            boardInput = altiumProject->m_board;
        }
    }

    // Preserve board-first format detection, but defer loading until the schematic supplies its names.
    for( const wxString& input : inputFiles )
    {
        if( altiumProject )
            break;

        bool accepted = false;

        if( !haveBoard )
        {
            JOB_PCB_IMPORT probe;
            probe.m_inputFile = input;
            probe.m_probeOnly = true;

            int pcbResult = aKiway.ProcessJob( KIWAY::FACE_PCB, &probe, &reporter );

            if( pcbResult == EXIT_CODES::SUCCESS )
            {
                const auto& sourcePcbs = probe.m_projectBoards;

                if( !sourcePcbs.empty() )
                {
                    const bool         hasMultipleBoards = sourcePcbs.size() > 1;
                    std::set<wxString> usedBoardNames;

                    for( const auto& [pcbId, sourceName] : sourcePcbs )
                    {
                        wxFileName outputFn = boardFn;

                        if( hasMultipleBoards )
                        {
                            wxString boardName = EscapeString( sourceName, CTX_FILENAME );

                            if( boardName.empty() )
                                boardName = pcbId;

                            wxString uniqueBoardName = boardName;
                            unsigned duplicate = 2;

                            while( !usedBoardNames.insert( uniqueBoardName ).second )
                            {
                                uniqueBoardName = wxString::Format( wxS( "%s-%u" ), boardName, duplicate );
                                ++duplicate;
                            }

                            outputFn.SetName( uniqueBoardName );
                        }

                        deferredBoards.push_back( { input, pcbId, outputFn.GetFullPath(), hasMultipleBoards } );

                        boards.emplace_back( KIID(), outputFn.GetFullName() );
                        haveBoard = true;
                    }
                }
                else
                {
                    boards.emplace_back( KIID(), boardFn.GetFullName() );
                    haveBoard = true;
                    boardInput = input;
                }

                accepted = true;
            }
            else if( pcbResult != EXIT_CODES::ERR_UNKNOWN_FILE_FORMAT )
            {
                retCode = pcbResult;
                break;
            }
        }

        // Some formats keep the board and the schematic in one file, so a board input is offered to both faces;
        // a board file without a schematic is still a good board
        if( !haveSch )
        {
            JOB_SCH_IMPORT schJob;
            schJob.m_inputFile = input;
            schJob.m_format = JOB_SCH_IMPORT::FORMAT::AUTO;
            schJob.SetConfiguredOutputPath( schFn.GetFullPath() );
            schJob.m_netlistFile = netlistFile;

            int schResult = aKiway.ProcessJob( KIWAY::FACE_SCH, &schJob, &reporter );

            if( schResult == EXIT_CODES::SUCCESS )
            {
                haveSch = true;
                netNameMap = std::move( schJob.m_netNameMap );
                haveNetlist = !netlistFile.IsEmpty() && wxFileName( netlistFile ).GetSize() > 0;
                accepted = true;
            }
            else if( schResult != EXIT_CODES::ERR_UNKNOWN_FILE_FORMAT && !accepted )
            {
                retCode = schResult;
                break;
            }
        }

        if( accepted )
            continue;

        if( haveBoard && haveSch )
        {
            wxFprintf( stderr, _( "Could not import '%s': a board and a schematic have already "
                                  "been imported for this project\n" ), input );
        }
        else
        {
            wxFprintf( stderr, _( "No board or schematic importer recognizes the file: %s\n" ), input );
        }

        retCode = EXIT_CODES::ERR_INVALID_INPUT_FILE;
        break;
    }

    if( retCode == EXIT_CODES::SUCCESS && !boardInput.empty() )
    {
        JOB_PCB_IMPORT pcbJob;
        pcbJob.m_inputFile = boardInput;
        pcbJob.m_format = JOB_PCB_IMPORT::FORMAT::AUTO;
        pcbJob.m_layerMap = layerMap;
        pcbJob.m_netNameMap = std::move( netNameMap );

        if( haveNetlist )
            pcbJob.m_netlistFile = netlistFile;

        if( altiumProject )
        {
            pcbJob.m_properties = projectProperties;
            pcbJob.m_sourceProjectDir = sourceProjectDir;
            pcbJob.m_localLibraries.assign( altiumProject->m_footprintLibraries.begin(),
                                            altiumProject->m_footprintLibraries.end() );
        }

        pcbJob.SetConfiguredOutputPath( boardFn.GetFullPath() );
        retCode = aKiway.ProcessJob( KIWAY::FACE_PCB, &pcbJob, &reporter );

        // the schematic takes the footprints the board import settled on, as the project
        // manager's import links them once both halves are in
        if( retCode == EXIT_CODES::SUCCESS && haveSch && !pcbJob.m_footprintIds.empty() )
        {
            JOB_SCH_IMPORT linkJob;
            linkJob.m_inputFile = schFn.GetFullPath();
            linkJob.m_footprintIds = std::move( pcbJob.m_footprintIds );
            retCode = aKiway.ProcessJob( KIWAY::FACE_SCH, &linkJob, &reporter );
        }

        copperLayers = std::move( pcbJob.m_copperLayers );
    }

    // The boards of a multi-board archive take the schematic's names and links as a single board does
    std::map<wxString, wxString> footprintIds;

    for( const DEFERRED_BOARD& deferred : deferredBoards )
    {
        if( retCode != EXIT_CODES::SUCCESS )
            break;

        JOB_PCB_IMPORT pcbJob;
        pcbJob.m_inputFile = deferred.input;
        pcbJob.m_importPcbId = deferred.pcbId;
        pcbJob.m_isPartOfMultiBoardProject = deferred.isPartOfMultiBoardProject;
        pcbJob.m_netNameMap = netNameMap;

        if( haveNetlist )
            pcbJob.m_netlistFile = netlistFile;

        pcbJob.SetConfiguredOutputPath( deferred.outputPath );
        retCode = aKiway.ProcessJob( KIWAY::FACE_PCB, &pcbJob, &reporter );

        // a reference on two boards keeps the first board's footprint
        footprintIds.insert( pcbJob.m_footprintIds.begin(), pcbJob.m_footprintIds.end() );
    }

    if( retCode == EXIT_CODES::SUCCESS && haveSch && !footprintIds.empty() )
    {
        JOB_SCH_IMPORT linkJob;
        linkJob.m_inputFile = schFn.GetFullPath();
        linkJob.m_footprintIds = std::move( footprintIds );
        retCode = aKiway.ProcessJob( KIWAY::FACE_SCH, &linkJob, &reporter );
    }

    // after the board, whose copper layers decide which mid layers the output jobs' plots keep
    if( retCode == EXIT_CODES::SUCCESS && altiumProject && !altiumProject->m_outputJobs.empty() )
    {
        if( copperLayers.empty() )
            copperLayers = { wxT( "F.Cu" ), wxT( "B.Cu" ) };

        ConvertAltiumOutputJobs( altiumProject->m_outputJobs, projectFn, copperLayers,
                                 [&]( const wxString& aOutJob, const wxString& aError )
                                 {
                                     reporter.Report( wxString::Format( _( "The Altium output job '%s' was not "
                                                                           "converted: %s\n" ),
                                                                        aOutJob, aError ),
                                                      RPT_SEVERITY_WARNING );
                                 } );
    }

    if( retCode == EXIT_CODES::SUCCESS && !haveBoard && !haveSch )
    {
        wxFprintf( stderr, _( "No inputs could be imported\n" ) );
        retCode = EXIT_CODES::ERR_INVALID_INPUT_FILE;
    }

    if( retCode == EXIT_CODES::SUCCESS && project )
    {
        // The schematic handler already registered any sheets; link the board here.
        if( haveBoard )
        {
            std::vector<FILE_INFO_PAIR>& projectBoards = project->GetProjectFile().GetBoards();
            projectBoards = boards;
        }

        if( mgr.SaveProject( projectFn.GetFullPath() ) )
        {
            reporter.Report( wxString::Format( _( "Created project '%s'\n" ),
                                               projectFn.GetFullPath() ),
                             RPT_SEVERITY_INFO );
        }
        else
        {
            wxFprintf( stderr, _( "Failed to write project file: %s\n" ),
                       projectFn.GetFullPath() );
            retCode = EXIT_CODES::ERR_INVALID_OUTPUT_CONFLICT;
        }
    }

    mgr.UnloadProject( project, false );

    if( !netlistFile.IsEmpty() && wxFileExists( netlistFile ) )
        wxRemoveFile( netlistFile );

    return retCode;
}

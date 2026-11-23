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

#ifndef IMPORT_ALTIUM_PROJECT_H
#define IMPORT_ALTIUM_PROJECT_H

#include <core/utf8.h>
#include <wx/filename.h>
#include <wx/string.h>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

/**
 * The documents an Altium project (.PrjPcb) lists, as its import needs them.
 */
struct ALTIUM_PROJECT_FILES
{
    std::set<wxString>    m_schematics;          ///< the schematic sheets (.SchDoc)
    wxString              m_board;               ///< the first board the project lists, if any
    int                   m_boardType;           ///< the PCB_IO_MGR plugin type of m_board
    std::set<wxString>    m_symbolLibraries;     ///< the project's own .SchLib files
    std::set<wxString>    m_footprintLibraries;  ///< the project's own .PcbLib files
    std::vector<wxString> m_outputJobs;          ///< the .OutJob files that exist
};

/**
 * Read the document list of an Altium project.  Relative paths are made absolute against the
 * project's directory.  Shared by the project manager's import and the `kicad-cli import` command.
 *
 * @param aProjectFile is the .PrjPcb file.
 */
ALTIUM_PROJECT_FILES ReadAltiumProjectFiles( const wxFileName& aProjectFile );

/**
 * The importer properties both halves of an Altium project import need: the project file (for its
 * parameters, variants and net identifier scope), the schematic sheets, the nicknames of the
 * project's import caches and the project's own libraries.
 *
 * @param aFiles is what ReadAltiumProjectFiles() read from the project.
 * @param aProjectFile is the .PrjPcb file.
 * @param aTargetStem is the name of the KiCad project being made, which names the caches.
 */
std::map<std::string, UTF8> AltiumProjectImportProperties( const ALTIUM_PROJECT_FILES& aFiles,
                                                           const wxFileName&           aProjectFile,
                                                           const wxString&             aTargetStem );

/**
 * Convert an Altium project's output jobs into KiCad jobsets beside the new project, one jobset
 * named after the project, or one per output job when there are several.  An output job of which
 * KiCad can make nothing, or whose jobset already exists, is skipped.
 *
 * @param aOutputJobs are the .OutJob files.
 * @param aTargetProject is the new KiCad project file.
 * @param aCopperLayers are the names of the imported board's copper layers.
 * @param aOnError is told of each output job that could not be read, with the error.
 */
void ConvertAltiumOutputJobs( const std::vector<wxString>& aOutputJobs, const wxFileName& aTargetProject,
                              const std::set<wxString>& aCopperLayers,
                              const std::function<void( const wxString&, const wxString& )>& aOnError );

#endif // IMPORT_ALTIUM_PROJECT_H

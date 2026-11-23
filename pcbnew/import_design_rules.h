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

#ifndef IMPORT_DESIGN_RULES_H
#define IMPORT_DESIGN_RULES_H

#include <map>
#include <wx/filename.h>
#include <wx/string.h>

class REPORTER;

/**
 * Save the custom rules an importer derived beside the board it produced.
 *
 * An existing rules file is the user's and is never replaced.
 *
 * Shared by the board editor import and the `kicad-cli import` job.
 *
 * @param[in] aRules is the rules text, nothing is written when it is empty.
 * @param[in] aBoardPath is the board the rules belong to, the rules take its name.
 * @param[in] aReporter receives a warning when the rules are not written.
 * @return true if the rules file was written.
 */
bool SaveImportedDesignRules( const wxString& aRules, const wxFileName& aBoardPath, REPORTER& aReporter );

/**
 * Make imported custom rules name the nets by their new names.
 *
 * An importer writes rules that name the source design's nets, such as `A.NetName == 'NetR34_1'`.  Where the
 * board's nets were renamed afterwards, as the reconcile with the schematic does, a rule on one of them would
 * match nothing.  Each net name such a condition compares against is replaced, the renames not chained.
 *
 * @param[in] aRules is the rules text.
 * @param[in] aNames maps each net's old name to its new one.
 * @return the rules text naming the nets by their new names.
 */
wxString RenameNetsInDesignRules( const wxString& aRules, const std::map<wxString, wxString>& aNames );

/**
 * Rename the nets the rules file of an imported board names, see RenameNetsInDesignRules().
 *
 * @param[in] aRulesPath is the rules file, nothing is done when there is none.
 * @param[in] aNames maps each net's old name to its new one.
 * @param[in] aReporter receives a warning when the rules cannot be read or written.
 * @return true if the file was changed.
 */
bool RenameNetsInDesignRulesFile( const wxFileName& aRulesPath, const std::map<wxString, wxString>& aNames,
                                  REPORTER& aReporter );

#endif // IMPORT_DESIGN_RULES_H

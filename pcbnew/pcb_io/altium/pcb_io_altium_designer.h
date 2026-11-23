/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 Thomas Pointhuber <thomas.pointhuber@gmx.at>
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

#ifndef PCB_IO_ALTIUM_DESIGNER_H_
#define PCB_IO_ALTIUM_DESIGNER_H_

#include <pcb_io/pcb_io.h>
#include <pcb_io/pcb_io_mgr.h>
#include <pcb_io/common/plugin_common_layer_mapping.h>

#include <map>
#include <memory>
#include <vector>

enum class ALTIUM_PCB_DIR;
class ALTIUM_PCB_COMPOUND_FILE;
class BOARD;
class PROJECT;
struct ALTIUM_PROJECT_VARIANT;

/**
 * Apply parsed Altium project variants to a board by setting FOOTPRINT_VARIANT
 * data on each footprint whose reference matches a variant entry.
 */
void ApplyAltiumProjectVariantsToBoard( BOARD* aBoard,
                                        const std::vector<ALTIUM_PROJECT_VARIANT>& aVariants );


/**
 * Load an Altium board file of any of the Altium flavours into @p aBoard.
 *
 * On a new load the component classes the translated rules test for are added to the project's
 * component class assignments, where KiCad keeps them.
 *
 * @return the board's design rules translated into a KiCad custom rules file, or an empty string
 *         when it has none KiCad can express.
 */
wxString LoadAltiumBoard( const wxString& aFileName, BOARD* aBoard,
                          const std::map<ALTIUM_PCB_DIR, std::string>& aMapping,
                          const std::map<std::string, UTF8>* aProperties, PROJECT* aProject,
                          PROGRESS_REPORTER* aProgressReporter, LAYER_MAPPING_HANDLER& aLayerMappingHandler,
                          REPORTER* aReporter, std::vector<ZONE*>& aZonesToFill, bool aIsNewLoad = true );


class PCB_IO_ALTIUM_DESIGNER : public PCB_IO, public LAYER_MAPPABLE_PLUGIN
{
public:
    // -----<PUBLIC PCB_IO API>--------------------------------------------------
    const IO_BASE::IO_FILE_DESC GetBoardFileDesc() const override
    {
        // Older Altium and Protel versions also saved binary boards as .pcb.  Other formats share
        // that extension, so CanReadBoard() also checks for the compound file header.
        return IO_BASE::IO_FILE_DESC( _HKI( "Altium Designer PCB files" ), { "PcbDoc", "pcb" } );
    }

    const IO_BASE::IO_FILE_DESC GetLibraryDesc() const override
    {
        return IO_BASE::IO_FILE_DESC( _HKI( "Altium PCB Library or Integrated Library" ),
                                 { "PcbLib", "IntLib" } );
    }

    bool CanReadBoard( const wxString& aFileName ) const override;
    bool CanReadLibrary( const wxString& aFileName ) const override;

    long long GetLibraryTimestamp( const wxString& aLibraryPath ) const override;

    void FootprintEnumerate( wxArrayString& aFootprintNames, const wxString& aLibraryPath,
                             bool aBestEfforts, const std::map<std::string, UTF8>* aProperties = nullptr ) override;

    std::unique_ptr<FOOTPRINT> FootprintLoad( const wxString& aLibraryPath, const wxString& aFootprintName,
                                              bool                               aKeepUUID = false,
                                              const std::map<std::string, UTF8>* aProperties = nullptr ) override;

    std::vector<FOOTPRINT*> GetImportedCachedLibraryFootprints() override;

    //bool FootprintExists( const wxString& aLibraryPath, const wxString& aFootprintName, const PROPERTIES* aProperties = nullptr );

    bool IsLibraryWritable( const wxString& aLibraryPath ) override { return false; }

    // -----</PUBLIC PCB_IO API>-------------------------------------------------

    PCB_IO_ALTIUM_DESIGNER();
    ~PCB_IO_ALTIUM_DESIGNER();

    static bool checkFileHeader( const wxString& aFileName );

    /**
     * Return the automapped layers.
     *
     * @param aInputLayerDescriptionVector
     * @return Auto-mapped layers
     */
    static std::map<wxString, PCB_LAYER_ID> DefaultLayerMappingCallback(
            const std::vector<INPUT_LAYER_DESC>& aInputLayerDescriptionVector );

    wxString GetImportedDesignRules() const override { return m_importedDesignRules; }

protected:
    void loadBoard( const wxString& aFileName, BOARD& aBoard, bool aIsNewLoad,
                    const std::map<std::string, UTF8>* aProperties, PROJECT* aProject = nullptr ) override;

    wxString m_importedDesignRules; ///< the most recent board's Altium rules, as KiCad rules

private:
    struct ALTIUM_FILE_CACHE
    {
        std::vector<std::unique_ptr<ALTIUM_PCB_COMPOUND_FILE>> m_Files;
        long long                                              m_Timestamp;
    };

    std::map<wxString, ALTIUM_FILE_CACHE> m_fplibFiles;

    void loadAltiumLibrary( const wxString& aLibraryPath );
};

#endif // PCB_IO_ALTIUM_DESIGNER_H_

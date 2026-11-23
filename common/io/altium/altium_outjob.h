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

#ifndef ALTIUM_OUTJOB_H
#define ALTIUM_OUTJOB_H

#include <map>
#include <set>
#include <vector>

#include <json_common.h>
#include <wx/string.h>


/**
 * What became of one output of an Altium OutJob.
 */
struct ALTIUM_OUTJOB_OUTCOME
{
    wxString m_Type; ///< Altium output type, such as "Gerber" or "NC Drill"
    wxString m_Name;
    wxString m_Document; ///< the Altium document the output is made from
    bool     m_Enabled = false;

    /**
     * "converted", "converted-no-medium" (converted, though Altium sends it nowhere),
     * "skipped-disabled" or "omitted" (KiCad has no job for it).
     */
    wxString m_Status;
    wxString m_Detail;
};


/**
 * Translate an Altium output job file (.OutJob) into a KiCad jobset.
 *
 * Gerber, NC drill, schematic print and PCB print outputs are converted, and pick and place,
 * bill of materials, ODB++ and STEP outputs become KiCad's default job of that kind.  Only the
 * outputs Altium has enabled are converted.  A jobset always runs against its project's own
 * board and schematic, so the Altium document an output names is kept only in the job's
 * description.  Fabrication jobs go to a "fabrication" folder and documents to "documents".
 *
 * Plots are stored by Altium layer id: the top, bottom and mid copper layers, overlays, solder
 * and paste masks, mechanical layers (Mechanical n becomes User.n) and the keep-out layer, which
 * becomes Edge.Cuts, the board outline the board importer builds, are mapped.  A mid layer the
 * board does not have, and the internal planes, are dropped.
 *
 * Job ids are derived from the OutJob's name and the output's position, so the same file always
 * gives the same jobset.
 */
class ALTIUM_OUTJOB
{
public:
    /**
     * @param aText          the contents of the .OutJob file.
     * @param aOutJobName    the file's name without extension, from which the job ids are made.
     * @param aProjectName   the KiCad project name, used in the names of output files.
     * @param aCopperLayers  the canonical names of the board's copper layers.
     */
    ALTIUM_OUTJOB( const wxString& aText, const wxString& aOutJobName, const wxString& aProjectName,
                   const std::set<wxString>& aCopperLayers );

    /**
     * Read and convert an .OutJob file, which Altium writes in Latin-1.
     *
     * @throw IO_ERROR when the file cannot be read.
     */
    static ALTIUM_OUTJOB FromFile( const wxString& aPath, const wxString& aProjectName,
                                   const std::set<wxString>& aCopperLayers );

    /**
     * @return the jobset as KiCad stores it, with only the settings the OutJob decides; the jobs
     *         fill in the rest with their defaults when they are loaded.
     */
    nlohmann::json GetJobset() const;

    bool HasJobs() const { return !m_jobs.empty(); }

    const std::vector<ALTIUM_OUTJOB_OUTCOME>& GetOutcomes() const { return m_outcomes; }

    /**
     * Write the jobset to @p aPath through KiCad's own jobset file.
     *
     * @return false when the file could not be written.
     */
    bool SaveJobset( const wxString& aPath ) const;

private:
    struct OUTPUT
    {
        int                          m_Index = 0;
        wxString                     m_Type;
        wxString                     m_Name;
        wxString                     m_Document;
        bool                         m_Enabled = false;
        std::vector<wxString>        m_Mediums;
        std::map<wxString, wxString> m_Config;
    };

    struct EMITTED_JOB
    {
        wxString       m_Id;
        wxString       m_Type;
        wxString       m_Description;
        nlohmann::json m_Settings;
        bool           m_IsFabrication = false;
    };

    void readSections( const wxString& aText );
    void readOutputs();
    void convert();

    std::map<wxString, wxString> configRecord( const OUTPUT& aOutput, int aParameter = 1 ) const;
    std::vector<wxString>        plotLayers( const wxString& aConfig, std::vector<wxString>& aDropped ) const;

    wxString jobId( const std::vector<wxString>& aParts ) const;

    void addJob( const OUTPUT& aOutput, const wxString& aType, const nlohmann::json& aSettings, bool aIsFabrication );
    void note( const OUTPUT& aOutput, const wxString& aStatus, const wxString& aDetail = wxEmptyString );

    void convertGerber( const OUTPUT& aOutput );
    void convertDrill( const OUTPUT& aOutput );
    void convertSchematicPrint( const OUTPUT& aOutput );
    void convertPcbPrint( const OUTPUT& aOutput );

    wxString           m_outJobName;
    wxString           m_projectName;
    std::set<wxString> m_copperLayers;

    std::map<wxString, std::map<wxString, wxString>> m_sections;
    std::vector<OUTPUT>                              m_outputs;

    std::vector<EMITTED_JOB>           m_jobs;
    std::vector<ALTIUM_OUTJOB_OUTCOME> m_outcomes;
};

#endif // ALTIUM_OUTJOB_H

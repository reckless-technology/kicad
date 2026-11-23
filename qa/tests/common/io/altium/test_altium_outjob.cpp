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

/**
 * @file test_altium_outjob.cpp
 * Conversion of Altium output jobs into KiCad jobsets.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <io/altium/altium_outjob.h>
#include <jobs/jobset.h>
#include <jobs/job_export_pcb_gerbers.h>

#include <wx/filename.h>


namespace
{

// A made-up output job in Altium's layout: a gerber plot of the top copper, top silk, a mid
// layer, an internal plane and the keep-out layer; an NC drill output sent to no medium; a
// schematic print; a disabled PCB print and an output type KiCad has no job for
const char* const OUTJOB =
        "[OutputJobFile]\r\n"
        "Version=1.0\r\n"
        "\r\n"
        "[OutputGroup1]\r\n"
        "Name=Synthetic\r\n"
        "OutputMedium1=Print Job\r\n"
        "OutputMedium1_Type=Printer\r\n"
        "OutputMedium2=Folder Structure\r\n"
        "OutputMedium2_Type=GeneratedFiles\r\n"
        "OutputType1=Gerber\r\n"
        "OutputName1=Gerber Files\r\n"
        "OutputDocumentPath1=Board.PcbDoc\r\n"
        "OutputEnabled1=1\r\n"
        "OutputEnabled1_OutputMedium1=0\r\n"
        "OutputEnabled1_OutputMedium2=1\r\n"
        "Configuration1_Name1=OutputConfigurationParameter1\r\n"
        "Configuration1_Item1=MirrorPlot.Set=16973830~1|NumberOfDecimals=6|Plot.Set=SerializeLayerHash.Version~2,"
        "ClassName~TLayerToBoolean,16777217~1,16973830~1,16777218~1,16842753~1,16973837~1,16973831~0|"
        "Panelize=True\r\n"
        "OutputType2=NC Drill\r\n"
        "OutputName2=NC Drill Files\r\n"
        "OutputDocumentPath2=Board.PcbDoc\r\n"
        "OutputEnabled2=1\r\n"
        "OutputEnabled2_OutputMedium1=0\r\n"
        "OutputEnabled2_OutputMedium2=2\r\n"
        "Configuration2_Name1=OutputConfigurationParameter1\r\n"
        "Configuration2_Item1=Record=DrillView|Units=Imperial|ZeroesMode=SuppressTrailingZeroes|"
        "OriginPosition=Relative|GenerateSeparatePlatedNonPlatedFiles=True\r\n"
        "OutputType3=Schematic Print\r\n"
        "OutputName3=Schematic Prints\r\n"
        "OutputDocumentPath3=\r\n"
        "OutputEnabled3=1\r\n"
        "OutputEnabled3_OutputMedium1=1\r\n"
        "OutputType4=PCB Print\r\n"
        "OutputName4=PCB Prints\r\n"
        "OutputEnabled4=0\r\n"
        "OutputType5=Testpoint Report\r\n"
        "OutputName5=Testpoints\r\n"
        "OutputEnabled5=1\r\n"
        "\r\n"
        "[PublishSettings]\r\n"
        "OutputFileName2=C:\\Output\\Synthetic schematics.pdf\r\n"
        "MarkNetLabels2=0\r\n";


const ALTIUM_OUTJOB_OUTCOME* outcomeOf( const ALTIUM_OUTJOB& aOutJob, const wxString& aType )
{
    for( const ALTIUM_OUTJOB_OUTCOME& outcome : aOutJob.GetOutcomes() )
    {
        if( outcome.m_Type == aType )
            return &outcome;
    }

    return nullptr;
}

} // namespace


BOOST_AUTO_TEST_SUITE( AltiumOutJob )


BOOST_AUTO_TEST_CASE( OutputsBecomeJobs )
{
    ALTIUM_OUTJOB  outJob( wxString::FromUTF8( OUTJOB ), wxT( "Synthetic" ), wxT( "MyProject" ),
                           { wxT( "F.Cu" ), wxT( "In1.Cu" ), wxT( "B.Cu" ) } );
    nlohmann::json jobset = outJob.GetJobset();

    BOOST_REQUIRE_EQUAL( jobset["jobs"].size(), 3 );

    const nlohmann::json& gerbers = jobset["jobs"][0];

    BOOST_CHECK_EQUAL( gerbers["type"], "pcb_export_gerbers" );
    BOOST_CHECK_EQUAL( gerbers["description"], "Gerber Files (Altium Gerber, Board.PcbDoc)" );

    // the ids follow from the output job's name and the output's position
    BOOST_CHECK_EQUAL( gerbers["id"], "a9bc2167-ce40-556b-b1eb-3332ceedb7f7" );

    // only the ids marked ~1 in Plot.Set, not those of MirrorPlot.Set; the plane is dropped
    BOOST_CHECK_EQUAL( gerbers["settings"]["layers"], nlohmann::json( { "F.Cu", "F.SilkS", "In1.Cu", "Edge.Cuts" } ) );
    BOOST_CHECK_EQUAL( gerbers["settings"]["precision"], 6 );
    BOOST_CHECK_EQUAL( gerbers["settings"]["use_protel_file_extension"], true );

    const nlohmann::json& drill = jobset["jobs"][1];

    BOOST_CHECK_EQUAL( drill["type"], "pcb_export_drill" );
    BOOST_CHECK_EQUAL( drill["settings"]["units"], "in" );
    BOOST_CHECK_EQUAL( drill["settings"]["zero_format"], "surpress_trailing" );
    BOOST_CHECK_EQUAL( drill["settings"]["drill_origin"], "plot" );
    BOOST_CHECK_EQUAL( drill["settings"]["excellon.combine_pth_npth"], false );

    const nlohmann::json& schematic = jobset["jobs"][2];

    BOOST_CHECK_EQUAL( schematic["type"], "sch_export_plot_pdf" );
    BOOST_CHECK_EQUAL( schematic["description"], "Schematic Prints (Altium Schematic Print, project)" );
    BOOST_CHECK_EQUAL( schematic["settings"]["output_filename"], "Synthetic schematics.pdf" );
    BOOST_CHECK_EQUAL( schematic["settings"]["pdf_hierarchical_links"], false );

    // fabrication and documents go to their own folders
    BOOST_REQUIRE_EQUAL( jobset["outputs"].size(), 2 );
    BOOST_CHECK_EQUAL( jobset["outputs"][0]["id"], "28f6590e-19e9-5112-919c-1cfd9da4a0a9" );
    BOOST_CHECK_EQUAL( jobset["outputs"][0]["only"], nlohmann::json( { gerbers["id"], drill["id"] } ) );
    BOOST_CHECK_EQUAL( jobset["outputs"][0]["settings"]["output_path"], "fabrication" );
    BOOST_CHECK_EQUAL( jobset["outputs"][1]["only"], nlohmann::json::array( { schematic["id"] } ) );

    BOOST_CHECK_EQUAL( outcomeOf( outJob, wxT( "NC Drill" ) )->m_Status, wxT( "converted-no-medium" ) );
    BOOST_CHECK_EQUAL( outcomeOf( outJob, wxT( "PCB Print" ) )->m_Status, wxT( "skipped-disabled" ) );
    BOOST_CHECK_EQUAL( outcomeOf( outJob, wxT( "Testpoint Report" ) )->m_Status, wxT( "omitted" ) );
    BOOST_CHECK( outcomeOf( outJob, wxT( "Gerber" ) )->m_Detail.Contains( wxT( "internal plane 1" ) ) );
    BOOST_CHECK( outcomeOf( outJob, wxT( "Gerber" ) )->m_Detail.Contains( wxT( "panelise not mapped" ) ) );
}


BOOST_AUTO_TEST_CASE( MidLayersTheBoardLacksAreDropped )
{
    ALTIUM_OUTJOB outJob( wxString::FromUTF8( OUTJOB ), wxT( "Synthetic" ), wxT( "MyProject" ),
                          { wxT( "F.Cu" ), wxT( "B.Cu" ) } );

    BOOST_CHECK_EQUAL( outJob.GetJobset()["jobs"][0]["settings"]["layers"],
                       nlohmann::json( { "F.Cu", "F.SilkS", "Edge.Cuts" } ) );
    BOOST_CHECK( outcomeOf( outJob, wxT( "Gerber" ) )->m_Detail.Contains( wxT( "mid layer 1 not in board" ) ) );
}


/**
 * The jobset is written through KiCad's own jobset file and reads back with the settings the
 * output job decided.
 */
BOOST_AUTO_TEST_CASE( SavedJobsetLoads )
{
    ALTIUM_OUTJOB outJob( wxString::FromUTF8( OUTJOB ), wxT( "Synthetic" ), wxT( "MyProject" ),
                          { wxT( "F.Cu" ), wxT( "In1.Cu" ), wxT( "B.Cu" ) } );

    wxFileName path( wxFileName::CreateTempFileName( wxT( "altium_outjob" ) ) );
    wxRemoveFile( path.GetFullPath() );
    path.SetExt( wxT( "kicad_jobset" ) );

    BOOST_REQUIRE( outJob.SaveJobset( path.GetFullPath() ) );

    JOBSET jobset( path.GetFullPath() );
    jobset.LoadFromFile();

    BOOST_REQUIRE_EQUAL( jobset.GetJobs().size(), 3 );
    BOOST_REQUIRE_EQUAL( jobset.GetDestinations().size(), 2 );

    const JOBSET_JOB& gerbers = jobset.GetJobs()[0];

    BOOST_CHECK_EQUAL( gerbers.m_type, wxT( "pcb_export_gerbers" ) );
    BOOST_CHECK_EQUAL( gerbers.m_id, wxT( "a9bc2167-ce40-556b-b1eb-3332ceedb7f7" ) );

    auto* gerberJob = dynamic_cast<JOB_EXPORT_PCB_GERBERS*>( gerbers.m_job.get() );

    BOOST_REQUIRE( gerberJob );
    BOOST_CHECK( gerberJob->m_useProtelFileExtension );
    BOOST_CHECK( !gerberJob->m_useX2Format ); // the job's default is X2
    BOOST_CHECK_EQUAL( gerberJob->m_precision, 6 );

    wxRemoveFile( path.GetFullPath() );
}


BOOST_AUTO_TEST_SUITE_END()

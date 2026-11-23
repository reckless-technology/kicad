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
 * @file test_altium_design_rules.cpp
 * Translation of Altium design rules into KiCad custom rules.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_file_utils.h>

#include <pcbnew/pcb_io/altium/altium_design_rules.h>
#include <pcbnew/pcb_io/altium/altium_parser_pcb.h>
#include <pcbnew/pcb_io/altium/pcb_io_altium_designer.h>

#include <board.h>
#include <board_design_settings.h>
#include <drc/drc_rule.h>
#include <drc/drc_rule_parser.h>
#include <reporter.h>


namespace
{

using PROPS = std::map<wxString, wxString>;


ARULE6 rule( const wxString& aKind, const wxString& aName, int aPriority, const wxString& aScope1,
             const wxString& aScope2, PROPS aExtra = {} )
{
    PROPS props = { { wxT( "RULEKIND" ), aKind },
                    { wxT( "NAME" ), aName },
                    { wxT( "PRIORITY" ), wxString::Format( wxT( "%d" ), aPriority ) },
                    { wxT( "ENABLED" ), wxT( "TRUE" ) },
                    { wxT( "SCOPE1EXPRESSION" ), aScope1 },
                    { wxT( "SCOPE2EXPRESSION" ), aScope2 } };

    for( const auto& [key, value] : aExtra )
        props[key] = value;

    return ARULE6( props );
}


ACLASS6 altiumClass( int aKind, const wxString& aName, const std::vector<wxString>& aMembers, bool aSuperClass = false )
{
    PROPS props = { { wxT( "KIND" ), wxString::Format( wxT( "%d" ), aKind ) },
                    { wxT( "NAME" ), aName },
                    { wxT( "SUPERCLASS" ), aSuperClass ? wxT( "TRUE" ) : wxT( "FALSE" ) } };

    for( size_t i = 0; i < aMembers.size(); i++ )
        props[wxString::Format( wxT( "M%zu" ), i )] = aMembers[i];

    return ACLASS6( props );
}


ALTIUM_RULES_BOARD_CONTEXT board()
{
    ALTIUM_RULES_BOARD_CONTEXT context;
    context.m_NetNames = { wxT( "GND" ), wxT( "+15V" ), wxT( "SIG" ) };
    context.m_References = { wxT( "R1" ), wxT( "SP1" ), wxT( "U1" ) };
    context.m_FootprintNames = { wxT( "SOIC8" ) };
    context.m_CopperLayers = { { wxT( "F.Cu" ), LT_SIGNAL },
                               { wxT( "In1.Cu" ), LT_POWER },
                               { wxT( "B.Cu" ), LT_SIGNAL } };
    return context;
}


wxString statusOf( const ALTIUM_DESIGN_RULES& aRules, const wxString& aName )
{
    for( const ALTIUM_RULE_OUTCOME& outcome : aRules.GetOutcomes() )
    {
        if( outcome.m_Name == aName )
            return outcome.m_Status;
    }

    return wxEmptyString;
}


/// The rule file must be one KiCad itself accepts
void checkParses( const wxString& aText )
{
    DRC_RULES_PARSER                       parser( aText, wxS( "altium rules" ) );
    WX_STRING_REPORTER                     reporter;
    std::vector<std::shared_ptr<DRC_RULE>> rules;

    BOOST_CHECK_NO_THROW( parser.Parse( rules, &reporter ) );
    BOOST_CHECK_MESSAGE( !reporter.HasMessageOfSeverity( RPT_SEVERITY_ERROR ),
                         "rules must parse cleanly: " << reporter.GetMessages().ToStdString() );
}

} // namespace


BOOST_AUTO_TEST_SUITE( AltiumDesignRules )


/**
 * Altium priority 1 wins, and KiCad checks the last matching rule first, so the most specific
 * rule must be written last.
 */
BOOST_AUTO_TEST_CASE( HigherPriorityIsWrittenLater )
{
    std::vector<ARULE6> rules = {
        rule( wxT( "Clearance" ), wxT( "Clearance" ), 2, wxT( "All" ), wxT( "All" ),
              { { wxT( "GAP" ), wxT( "10mil" ) } } ),
        rule( wxT( "Clearance" ), wxT( "Ground" ), 1, wxT( "InNet('GND')" ), wxT( "All" ),
              { { wxT( "GAP" ), wxT( "0.5mm" ) } } ),
    };

    ALTIUM_RULES_BOARD_CONTEXT context = board();
    ALTIUM_DESIGN_RULES        converter( rules, {}, context );
    wxString                   text = converter.FormatRules( wxT( "test.PcbDoc" ) );

    int general = text.Find( wxT( "(rule \"Altium Clearance\"" ) );
    int ground = text.Find( wxT( "(rule \"Altium Clearance Ground\"" ) );

    BOOST_REQUIRE( general != wxNOT_FOUND && ground != wxNOT_FOUND );
    BOOST_CHECK_LT( general, ground );
    BOOST_CHECK( text.Contains( wxT( "(constraint clearance (min 0.254mm))" ) ) );
    BOOST_CHECK( text.Contains( wxT( "(condition \"A.NetName == 'GND'\")" ) ) );

    checkParses( text );
}


BOOST_AUTO_TEST_CASE( ScopesBecomeConditions )
{
    ALTIUM_RULES_BOARD_CONTEXT context = board();
    ALTIUM_DESIGN_RULES        converter(
            {}, { altiumClass( 0, wxT( "Power" ), { wxT( "+15V" ) } ), altiumClass( 0, wxT( "All Nets" ), {}, true ) },
            context );

    auto text = [&]( const wxString& aScope ) -> wxString
    {
        ALTIUM_DESIGN_RULES::CONDITION condition = converter.TranslateScope( aScope, wxT( "A" ) );
        return std::holds_alternative<wxString>( condition )
                       ? std::get<wxString>( condition )
                       : wxString( std::get<bool>( condition ) ? "<always>" : "<never>" );
    };

    BOOST_CHECK_EQUAL( text( wxT( "All" ) ), wxT( "<always>" ) );
    BOOST_CHECK_EQUAL( text( wxT( "InNet('gnd')" ) ), wxT( "A.NetName == 'GND'" ) );
    BOOST_CHECK_EQUAL( text( wxT( "InNet('NoSuchNet')" ) ), wxT( "<never>" ) );
    BOOST_CHECK_EQUAL( text( wxT( "InNetClass('power')" ) ), wxT( "A.hasNetclass('Power')" ) );
    BOOST_CHECK_EQUAL( text( wxT( "InNetClass('All Nets')" ) ), wxT( "<always>" ) );
    BOOST_CHECK_EQUAL( text( wxT( "IsPad And InComponent('r1')" ) ),
                       wxT( "(A.Type == 'Pad' && A.Reference == 'R1')" ) );
    BOOST_CHECK_EQUAL( text( wxT( "IsVia or IsTrack" ) ), wxT( "(A.Type == 'Via' || A.Type == 'Track')" ) );
    BOOST_CHECK_EQUAL( text( wxT( "Not IsSMDPad" ) ), wxT( "!(A.Type == 'Pad' && A.Pad_Type == 'SMD')" ) );
    BOOST_CHECK_EQUAL( text( wxT( "OnLayer('Mid-Layer 1')" ) ), wxT( "A.Layer == 'In1.Cu'" ) );
    BOOST_CHECK_EQUAL( text( wxT( "HasFootprint('soic8')" ) ), wxT( "A.Library_Link == '*:SOIC8'" ) );
    BOOST_CHECK_EQUAL( text( wxT( "InNet('GND') Or InNet('NoSuchNet')" ) ), wxT( "A.NetName == 'GND'" ) );

    // Comparisons and Altium-only functions have no KiCad equivalent
    BOOST_CHECK_THROW( text( wxT( "CompPinCount < 5" ) ), std::runtime_error );
    BOOST_CHECK_THROW( text( wxT( "IsBGA" ) ), std::runtime_error );
    BOOST_CHECK_THROW( text( wxT( "IsPad Xor IsVia" ) ), std::runtime_error );
}


/**
 * A (First, Second) pair matches one object against each; the same scope twice means both.
 */
BOOST_AUTO_TEST_CASE( ScopePairs )
{
    std::vector<ARULE6> rules = {
        rule( wxT( "Clearance" ), wxT( "Pair" ), 1, wxT( "InNet('GND')" ), wxT( "InNet('SIG')" ),
              { { wxT( "GAP" ), wxT( "20mil" ) } } ),
        rule( wxT( "Clearance" ), wxT( "Star" ), 2, wxT( "InComponent('SP1')" ), wxT( "InComponent('SP1')" ),
              { { wxT( "GAP" ), wxT( "0mil" ) } } ),
        rule( wxT( "Clearance" ), wxT( "Lone" ), 3, wxT( "All" ), wxT( "IsVia" ),
              { { wxT( "GAP" ), wxT( "12mil" ) } } ),
        rule( wxT( "Clearance" ), wxT( "Gone" ), 4, wxT( "InNet('NoSuchNet')" ), wxT( "All" ),
              { { wxT( "GAP" ), wxT( "12mil" ) } } ),
    };

    ALTIUM_RULES_BOARD_CONTEXT context = board();
    ALTIUM_DESIGN_RULES        converter( rules, {}, context );
    wxString                   text = converter.FormatRules( wxT( "test.PcbDoc" ) );

    BOOST_CHECK( text.Contains( wxT( "(condition \"A.NetName == 'GND' && B.NetName == 'SIG'\")" ) ) );
    BOOST_CHECK( text.Contains( wxT( "(condition \"A.Reference == 'SP1' && B.Reference == 'SP1'\")" ) ) );
    BOOST_CHECK( text.Contains( wxT( "(condition \"A.Type == 'Via'\")" ) ) );

    BOOST_CHECK_EQUAL( statusOf( converter, wxT( "Gone" ) ), wxT( "omitted" ) );
    BOOST_CHECK( !text.Contains( wxT( "Altium Clearance Gone" ) ) );

    checkParses( text );
}


/**
 * An allowed short circuit must override every clearance rule, so it goes last; one that an
 * All/All "not allowed" rule outranks never applies in Altium and is not written.
 */
BOOST_AUTO_TEST_CASE( ShortCircuitAllowances )
{
    std::vector<ARULE6> rules = {
        rule( wxT( "ShortCircuit" ), wxT( "StarPoint" ), 2, wxT( "InComponent('SP1')" ), wxT( "All" ),
              { { wxT( "ALLOWED" ), wxT( "TRUE" ) } } ),
        rule( wxT( "Clearance" ), wxT( "Clearance" ), 1, wxT( "All" ), wxT( "All" ),
              { { wxT( "GAP" ), wxT( "10mil" ) } } ),
        rule( wxT( "ShortCircuit" ), wxT( "ShortCircuit" ), 3, wxT( "All" ), wxT( "All" ),
              { { wxT( "ALLOWED" ), wxT( "FALSE" ) } } ),
    };

    ALTIUM_RULES_BOARD_CONTEXT context = board();

    {
        ALTIUM_DESIGN_RULES converter( rules, {}, context );
        wxString            text = converter.FormatRules( wxT( "test.PcbDoc" ) );

        BOOST_CHECK_EQUAL( statusOf( converter, wxT( "StarPoint" ) ), wxT( "ported" ) );
        BOOST_CHECK_EQUAL( statusOf( converter, wxT( "ShortCircuit" ) ), wxT( "ported-implicit" ) );
        BOOST_CHECK_GT( text.Find( wxT( "Altium ShortCircuit StarPoint" ) ), text.Find( wxT( "Altium Clearance" ) ) );
        BOOST_CHECK( text.Contains( wxT( "(constraint clearance (min -1mm))" ) ) );
        checkParses( text );
    }

    // The All/All "not allowed" rule now outranks the allowance
    rules[2] = rule( wxT( "ShortCircuit" ), wxT( "ShortCircuit" ), 1, wxT( "All" ), wxT( "All" ),
                     { { wxT( "ALLOWED" ), wxT( "FALSE" ) } } );

    ALTIUM_DESIGN_RULES converter( rules, {}, context );

    BOOST_CHECK_EQUAL( statusOf( converter, wxT( "StarPoint" ) ), wxT( "shadowed" ) );
    BOOST_CHECK( !converter.FormatRules( wxT( "test.PcbDoc" ) ).Contains( wxT( "StarPoint" ) ) );
}


BOOST_AUTO_TEST_CASE( KindsAndDisabledRules )
{
    std::vector<ARULE6> rules = {
        rule( wxT( "Width" ), wxT( "Width" ), 1, wxT( "All" ), wxT( "All" ),
              { { wxT( "MINLIMIT" ), wxT( "8mil" ) },
                { wxT( "MAXLIMIT" ), wxT( "100mil" ) },
                { wxT( "PREFEREDWIDTH" ), wxT( "12mil" ) } } ),
        rule( wxT( "RoutingVias" ), wxT( "RoutingVias" ), 1, wxT( "All" ), wxT( "All" ),
              { { wxT( "MINWIDTH" ), wxT( "20mil" ) },
                { wxT( "WIDTH" ), wxT( "24mil" ) },
                { wxT( "MAXWIDTH" ), wxT( "50mil" ) },
                { wxT( "MINHOLEWIDTH" ), wxT( "8mil" ) },
                { wxT( "HOLEWIDTH" ), wxT( "12mil" ) },
                { wxT( "MAXHOLEWIDTH" ), wxT( "28mil" ) } } ),
        rule( wxT( "HoleSize" ), wxT( "HoleSize" ), 1, wxT( "All" ), wxT( "All" ),
              { { wxT( "MINLIMIT" ), wxT( "10mil" ) }, { wxT( "MAXLIMIT" ), wxT( "100mil" ) } } ),
        rule( wxT( "PlaneClearance" ), wxT( "PlaneClearance" ), 1, wxT( "All" ), wxT( "All" ),
              { { wxT( "CLEARANCE" ), wxT( "20mil" ) } } ),
        rule( wxT( "PolygonConnect" ), wxT( "PolygonConnect" ), 1, wxT( "All" ), wxT( "All" ),
              { { wxT( "CONNECTSTYLE" ), wxT( "Relief" ) },
                { wxT( "RELIEFCONDUCTORWIDTH" ), wxT( "10mil" ) },
                { wxT( "AIRGAPWIDTH" ), wxT( "10mil" ) },
                { wxT( "RELIEFENTRIES" ), wxT( "4" ) } } ),
        rule( wxT( "ComponentClearance" ), wxT( "ComponentClearance" ), 1, wxT( "All" ), wxT( "All" ),
              { { wxT( "GAP" ), wxT( "10mil" ) } } ),
        rule( wxT( "FanoutControl" ), wxT( "Fanout_BGA" ), 1, wxT( "IsBGA" ), wxT( "All" ) ),
    };

    rules[5].properties[wxT( "ENABLED" )] = wxT( "FALSE" );

    ALTIUM_RULES_BOARD_CONTEXT context = board();
    ALTIUM_DESIGN_RULES        converter( rules, {}, context );
    wxString                   text = converter.FormatRules( wxT( "test.PcbDoc" ) );

    BOOST_CHECK( text.Contains( wxT( "(constraint track_width (min 0.2032mm) (opt 0.3048mm) (max 2.54mm))" ) ) );
    BOOST_CHECK( text.Contains( wxT( "(constraint via_diameter (min 0.508mm) (opt 0.6096mm) (max 1.27mm))" ) ) );

    // a via hole must also satisfy the All/All hole size rule
    BOOST_CHECK( text.Contains( wxT( "(constraint hole_size (min 0.254mm) (opt 0.3048mm) (max 0.7112mm))" ) ) );

    BOOST_CHECK( text.Contains( wxT( "(layer \"In1.Cu\")" ) ) );
    BOOST_CHECK( text.Contains( wxT( "(constraint zone_connection thermal_reliefs)" ) ) );
    BOOST_CHECK( text.Contains( wxT( "# (rule \"Altium ComponentClearance\"" ) ) );

    BOOST_CHECK_EQUAL( statusOf( converter, wxT( "ComponentClearance" ) ), wxT( "ported-disabled" ) );
    BOOST_CHECK_EQUAL( statusOf( converter, wxT( "Fanout_BGA" ) ), wxT( "omitted" ) );

    checkParses( text );
}


/**
 * KiCad's board minimums are a floor under every rule, so they must come down to the smallest
 * value an enabled rule asks for.
 */
BOOST_AUTO_TEST_CASE( BoardMinimumsComeDown )
{
    std::vector<ARULE6> rules = {
        rule( wxT( "Clearance" ), wxT( "Clearance" ), 2, wxT( "All" ), wxT( "All" ),
              { { wxT( "GAP" ), wxT( "10mil" ) } } ),
        rule( wxT( "Clearance" ), wxT( "Star" ), 1, wxT( "InComponent('SP1')" ), wxT( "InComponent('SP1')" ),
              { { wxT( "GAP" ), wxT( "0mil" ) } } ),
    };

    ALTIUM_RULES_BOARD_CONTEXT context = board();
    ALTIUM_DESIGN_RULES        converter( rules, {}, context );
    BOARD_DESIGN_SETTINGS      settings( nullptr, "test" );

    settings.m_MinClearance = pcbIUScale.mmToIU( 0.254 );
    converter.ApplyBoardMinimums( settings );

    BOOST_CHECK_EQUAL( settings.m_MinClearance, 0 );
}


BOOST_AUTO_TEST_CASE( ComponentClassesAreAssigned )
{
    std::vector<ARULE6> rules = {
        rule( wxT( "Clearance" ), wxT( "HighVoltage" ), 1, wxT( "InComponentClass('HV')" ), wxT( "All" ),
              { { wxT( "GAP" ), wxT( "2mm" ) } } ),
    };

    ALTIUM_RULES_BOARD_CONTEXT context = board();
    ALTIUM_DESIGN_RULES        converter( rules,
                                          { altiumClass( 1, wxT( "HV" ), { wxT( "U1" ), wxT( "R9" ) } ),
                                            altiumClass( 1, wxT( "Unused" ), { wxT( "R1" ) } ) },
                                          context );
    wxString                   text = converter.FormatRules( wxT( "test.PcbDoc" ) );

    BOOST_CHECK( text.Contains( wxT( "(condition \"A.hasComponentClass('HV')\")" ) ) );

    // KiCad assigns classes in the board setup, not in the rules file; only the classes a rule
    // tests for, and only the designators the board has
    BOOST_CHECK( !text.Contains( wxT( "assign_component_class" ) ) );

    auto classes = converter.GetComponentClasses();

    BOOST_REQUIRE_EQUAL( classes.size(), 1 );
    BOOST_CHECK_EQUAL( classes[0].first, wxT( "HV" ) );
    BOOST_CHECK( classes[0].second == std::vector<wxString>( { wxT( "U1" ) } ) );

    checkParses( text );
}


/**
 * The importer hands the translated rules over for the new board, in a file KiCad can read.
 */
BOOST_AUTO_TEST_CASE( ImporterProvidesRules )
{
    for( const char* path :
         { "plugins/altium/issue24456/Fastino_Ground_Isolator.PcbDoc", "plugins/altium/HiFive/HiFive1.B01.PcbDoc",
           "plugins/altium/eDP_adapter_dvt1_source/eDP_adapter_dvt1.PcbDoc" } )
    {
        BOOST_TEST_CONTEXT( path )
        {
            PCB_IO_ALTIUM_DESIGNER plugin;
            std::unique_ptr<BOARD> loaded = plugin.LoadBoard( KI_TEST::GetPcbnewTestDataDir() + path, nullptr );

            BOOST_REQUIRE( loaded );

            wxString text = plugin.GetImportedDesignRules();

            BOOST_CHECK( text.StartsWith( wxT( "(version 1)\n" ) ) );
            BOOST_CHECK( text.Contains( wxT( "(constraint clearance (min " ) ) );
            checkParses( text );
        }
    }

    // Fastino keeps a Width rule for its 50R netclass
    PCB_IO_ALTIUM_DESIGNER plugin;
    plugin.LoadBoard( KI_TEST::GetPcbnewTestDataDir() + "plugins/altium/issue24456/Fastino_Ground_Isolator.PcbDoc",
                      nullptr );

    BOOST_CHECK( plugin.GetImportedDesignRules().Contains( wxT( "A.hasNetclass('50R')" ) ) );
}


BOOST_AUTO_TEST_SUITE_END()

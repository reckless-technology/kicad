/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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
 * @file test_drc_npth_own_copper.cpp
 *
 * A non-plated hole drilled through copper of its own footprint, such as a ring around a mounting
 * hole, is not a hole clearance violation whatever the nets: the hole carries no number, so
 * Update PCB from Schematic leaves it with no net.  On the ring's layers the hole is tested as of
 * the ring's net, so the ring's own tracks and pour may reach it.  A hole only partly under its
 * footprint's copper still is one.
 *
 * H1: rings on both sides on net A, hole with no net, a track of net A on the top ring and a
 * pour of net A over the bottom one.  H2: one ring, no nets at all.  H3: a pad that covers only
 * part of the hole.  H4: a ring of net A on the bottom only, and the same track of net A within
 * hole clearance on each side: on the bottom it is on the ring, on the top it is a violation.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>
#include <pcbnew_utils/board_test_utils.h>

#include <board.h>
#include <board_design_settings.h>
#include <drc/drc_item.h>
#include <drc/drc_engine.h>
#include <footprint.h>
#include <settings/settings_manager.h>
#include <widgets/report_severity.h>

#include <map>


struct NPTH_OWN_COPPER_FIXTURE
{
    NPTH_OWN_COPPER_FIXTURE() = default;

    ~NPTH_OWN_COPPER_FIXTURE()
    {
        if( m_board && m_board->GetDesignSettings().m_DRCEngine )
            m_board->GetDesignSettings().m_DRCEngine->ClearViolationHandler();

        if( m_board )
        {
            m_board->SetProject( nullptr );
            m_board = nullptr;
        }
    }

    SETTINGS_MANAGER       m_settingsManager;
    std::unique_ptr<BOARD> m_board;
};


BOOST_FIXTURE_TEST_CASE( NpthUnderOwnCopperIsNoHoleClearanceViolation, NPTH_OWN_COPPER_FIXTURE )
{
    KI_TEST::LoadBoard( m_settingsManager, "npth_own_copper/npth_own_copper", m_board );

    BOOST_REQUIRE_MESSAGE( m_board, "Failed to load board npth_own_copper" );

    BOARD_DESIGN_SETTINGS& bds = m_board->GetDesignSettings();

    BOOST_REQUIRE_MESSAGE( bds.m_DRCEngine, "DRC engine not initialized" );

    for( int ii = DRCE_FIRST; ii <= DRCE_LAST; ++ii )
        bds.m_DRCSeverities[ii] = SEVERITY::RPT_SEVERITY_IGNORE;

    bds.m_DRCSeverities[DRCE_HOLE_CLEARANCE] = SEVERITY::RPT_SEVERITY_ERROR;
    bds.m_DRCSeverities[DRCE_SOLDERMASK_BRIDGE] = SEVERITY::RPT_SEVERITY_ERROR;

    std::map<wxString, int> violations;    // reference -> count
    std::map<wxString, int> topViolations; // reference -> count on F.Cu

    bds.m_DRCEngine->SetViolationHandler(
            [&]( const std::shared_ptr<DRC_ITEM>& aItem, const VECTOR2I& /*aPos*/, int aLayer,
                 const std::function<void( PCB_MARKER* )>& /*aPathGen*/ )
            {
                wxString reference = wxS( "?" );

                for( const KIID& id : { aItem->GetMainItemID(), aItem->GetAuxItemID() } )
                {
                    BOARD_ITEM* item = m_board->ResolveItem( id, true );

                    if( item && item->GetParentFootprint() )
                        reference = item->GetParentFootprint()->GetReference();
                }

                violations[reference]++;

                if( aLayer == F_Cu )
                    topViolations[reference]++;
            } );

    bds.m_DRCEngine->RunTests( EDA_UNITS::MM, true, false );
    bds.m_DRCEngine->ClearViolationHandler();

    for( const auto& [reference, count] : violations )
        BOOST_TEST_MESSAGE( wxString::Format( "%s: %d violations", reference, count ) );

    BOOST_CHECK_EQUAL( violations[wxS( "H1" )], 0 );
    BOOST_CHECK_EQUAL( violations[wxS( "H2" )], 0 );
    BOOST_CHECK_GT( violations[wxS( "H3" )], 0 );
    BOOST_CHECK_GT( topViolations[wxS( "H4" )], 0 );
    BOOST_CHECK_EQUAL( violations[wxS( "H4" )], topViolations[wxS( "H4" )] );
    BOOST_CHECK_EQUAL( violations[wxS( "?" )], 0 );
}

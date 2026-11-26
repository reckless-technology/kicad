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

/**
 * @file
 * The depth slices the Vulkan canvas gives the overlay's ranges, which have to order them as the OpenGL
 * canvas does: by depth, nearer first, and the range drawn first keeping the pixels two at one depth share
 * (the measure tool's label text over its shadow, #122).
 */

#include <boost/test/unit_test.hpp>

#include <gal/gal_display_options.h>
#include <gal/gal_modern.h>
#include <rhi/render_thread.h>
#include <rhi/rhi_device.h>
#include <rhi/rhi_device_proxy.h>

#include <algorithm>
#include <vector>


namespace
{
struct MODERN_GAL_FIXTURE
{
    MODERN_GAL_FIXTURE() :
            thread(
                    []()
                    {
                        return std::unique_ptr<RHI_DEVICE>();
                    } ),
            proxy( thread ),
            gal( options, proxy )
    {
        gal.SetPersistentTargets( true );

        // Retained layers, between which the overlay's ranges are in front or behind
        gal.SetSceneLayerState( 1, 10, true, false );
        gal.SetSceneLayerState( 2, 20, true, false );
    }

    /// Draw one dot in a range of the overlay at aDepth, with a fill so that it makes a polygon
    void Dot( double aDepth )
    {
        gal.SetTarget( KIGFX::TARGET_OVERLAY );
        gal.SetLayerDepth( aDepth );
        gal.SetIsStroke( false );
        gal.SetIsFill( true );
        gal.SetFillColor( KIGFX::COLOR4D( 1.0, 1.0, 1.0, 1.0 ) );
        gal.DrawRectangle( VECTOR2D( 0, 0 ), VECTOR2D( 10, 10 ) );
    }

    std::vector<KIGFX::LAYER_RANGE> Ranges()
    {
        std::vector<KIGFX::LAYER_RANGE> ranges;

        for( const KIGFX::LAYER_RANGE& range : gal.GetDynamicDrawDesc().layerRanges )
        {
            if( range.HasGeometry() && range.target == KIGFX::TARGET_OVERLAY )
                ranges.push_back( range );
        }

        return ranges;
    }

    KIGFX::GAL_DISPLAY_OPTIONS options;
    KIGFX::RENDER_THREAD       thread;
    KIGFX::RHI_DEVICE_PROXY    proxy;
    KIGFX::GAL_MODERN          gal;
};
} // namespace


BOOST_AUTO_TEST_SUITE( GalModernDepth )


BOOST_FIXTURE_TEST_CASE( TiedRangesKeepFirstDrawnNearest, MODERN_GAL_FIXTURE )
{
    // Both at the nearest depth, as the measure tool draws its label and the shadow behind it, with another
    // range drawn between them
    gal.BeginDrawing();
    Dot( -4096.0 );
    Dot( -2038.0 );
    Dot( -4096.0 );
    gal.EndDrawing();

    const std::vector<KIGFX::LAYER_RANGE> ranges = Ranges();
    BOOST_REQUIRE_EQUAL( ranges.size(), 3u );

    // Drawn first, so nearest: a LESS_EQUAL test lets the nearer pixels stand
    BOOST_CHECK_LT( ranges[0].depthFar, ranges[2].depthNear + 1e-6f );
    BOOST_CHECK_LT( ranges[0].depthNear, ranges[2].depthNear );

    // Nearer depth wins over drawn first
    BOOST_CHECK_LT( ranges[2].depthFar, ranges[1].depthNear + 1e-6f );
}


BOOST_FIXTURE_TEST_CASE( RangesInFrontAreOrderedByDepth, MODERN_GAL_FIXTURE )
{
    gal.BeginDrawing();
    Dot( -2038.0 );
    Dot( -2039.0 );
    gal.EndDrawing();

    const std::vector<KIGFX::LAYER_RANGE> ranges = Ranges();
    BOOST_REQUIRE_EQUAL( ranges.size(), 2u );

    // -2039 is the nearer, though it was drawn second
    BOOST_CHECK_LE( ranges[1].depthFar, ranges[0].depthNear + 1e-6f );
    BOOST_CHECK_LT( ranges[1].depthNear, ranges[0].depthNear );
}


BOOST_FIXTURE_TEST_CASE( SlicesOfRangesInFrontStayInTheMarginBeforeTheLayers, MODERN_GAL_FIXTURE )
{
    gal.BeginDrawing();
    Dot( -4096.0 );
    Dot( -4096.0 );
    gal.EndDrawing();

    // The first retained layer's slice starts at half a unit of the span, 0.5 / ( 20 - 10 + 2 )
    const float firstLayerNear = 0.5f / 12.0f;

    for( const KIGFX::LAYER_RANGE& range : Ranges() )
    {
        BOOST_CHECK_GE( range.depthNear, 0.0f );
        BOOST_CHECK_LE( range.depthFar, firstLayerNear + 1e-6f );
        BOOST_CHECK_LT( range.depthNear, range.depthFar );
    }
}


BOOST_AUTO_TEST_SUITE_END()

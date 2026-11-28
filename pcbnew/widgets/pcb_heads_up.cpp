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

#include "pcb_heads_up.h"

#include <algorithm>
#include <cmath>

#include <font/text_attributes.h>
#include <gal/graphics_abstraction_layer.h>
#include <geometry/eda_angle.h>
#include <layer_ids.h>
#include <math/util.h>
#include <view/view.h>


namespace
{
/// The glyph size the text is drawn at, in screen pixels. BitmapText() draws a line 1.4 times as tall as that.
constexpr double GLYPH_HEIGHT = 9.5;
constexpr double LINE_HEIGHT_FACTOR = 1.4;

/// The distance of the panel from the edge of the canvas, and of its text from its own edge, in screen pixels
constexpr double CANVAS_MARGIN = 8.0;
constexpr double PANEL_PADDING = 6.0;

/// The gap between a line and the next, and how far from the cursor the panel sits while it follows it
constexpr double LINE_GAP = 3.0;
constexpr double FOLLOW_OFFSET = 24.0;

/**
 * How wide a character of the canvas's overlay text is, in glyph sizes. Both faces BitmapText() draws on the overlay
 * (the built-in bitmap font and the outline overlay face, which is scaled to match it) are monospaced at about this
 * advance, so a line is as wide as it has characters.
 */
constexpr double CHARACTER_ADVANCE = 0.82;

/// The panel is never narrower than this many characters, which holds the cursor and delta lines at their longest, so
/// the digits changing do not make it twitch
constexpr int MINIMUM_CHARACTERS = 44;

constexpr double PANEL_OPACITY = 0.78;

double luminance( const KIGFX::COLOR4D& aColor )
{
    return 0.2126 * aColor.r + 0.7152 * aColor.g + 0.0722 * aColor.b;
}
} // namespace


PCB_HEADS_UP::PCB_HEADS_UP() :
        m_hasState( false ),
        m_isShown( false ),
        m_isFollowing( false ),
        m_background( 0.0, 0.0, 0.0, 1.0 ),
        m_widestLine( MINIMUM_CHARACTERS )
{
}


bool PCB_HEADS_UP::SetState( const HEADS_UP_STATE& aState )
{
    if( m_hasState && aState == m_state )
        return false;

    m_state = aState;
    m_hasState = true;
    m_lines = BuildHeadsUpLines( m_state );

    m_widestLine = MINIMUM_CHARACTERS;

    for( const HEADS_UP_LINE& line : m_lines )
        m_widestLine = std::max( m_widestLine, static_cast<int>( line.text.length() ) );

    return true;
}


bool PCB_HEADS_UP::SetShown( bool aIsShown )
{
    if( aIsShown == m_isShown )
        return false;

    m_isShown = aIsShown;
    return true;
}


bool PCB_HEADS_UP::SetFollowing( bool aIsFollowing, const VECTOR2D& aCursorInWorld )
{
    if( !aIsFollowing && !m_isFollowing )
        return false;

    if( aIsFollowing == m_isFollowing && aCursorInWorld == m_cursorInWorld )
        return false;

    m_isFollowing = aIsFollowing;
    m_cursorInWorld = aCursorInWorld;
    return true;
}


bool PCB_HEADS_UP::SetBackground( const KIGFX::COLOR4D& aBackground )
{
    if( aBackground == m_background )
        return false;

    m_background = aBackground;
    return true;
}


const BOX2I PCB_HEADS_UP::ViewBBox() const
{
    // The display is in screen space, so there is no place of the board it belongs to: make it always visible
    BOX2I bbox;
    bbox.SetMaximum();

    return bbox;
}


std::vector<int> PCB_HEADS_UP::ViewGetLayers() const
{
    return { LAYER_GP_OVERLAY };
}


double PCB_HEADS_UP::ViewGetLOD( int aLayer [[maybe_unused]], const KIGFX::VIEW* aView [[maybe_unused]] ) const
{
    return m_isShown && m_hasState ? LOD_SHOW : LOD_HIDE;
}


void PCB_HEADS_UP::ViewDraw( int aLayer [[maybe_unused]], KIGFX::VIEW* aView ) const
{
    if( !m_isShown || !m_hasState || m_lines.empty() )
        return;

    KIGFX::GAL* gal = aView->GetGAL();

    // Everything is laid out in screen pixels and drawn in world units: one pixel is this long
    const double pixel = std::abs( aView->ToWorld( VECTOR2D( 1.0, 1.0 ), false ).x );
    const double glyph = GLYPH_HEIGHT * pixel;

    const double lineHeight = GLYPH_HEIGHT * LINE_HEIGHT_FACTOR;
    const double rowHeight = lineHeight + LINE_GAP;
    const double chip = lineHeight - 2.0;
    const double chipSpace = chip + 6.0;

    const double textWidth = m_widestLine * CHARACTER_ADVANCE * GLYPH_HEIGHT;
    const double width = textWidth + chipSpace + PANEL_PADDING * 2.0;
    const double height = m_lines.size() * rowHeight + PANEL_PADDING * 2.0 - LINE_GAP;

    VECTOR2D origin( CANVAS_MARGIN, CANVAS_MARGIN );

    if( m_isFollowing )
    {
        // Below and to the right of the cursor, or on the other side of it where that would leave the canvas
        const VECTOR2D cursor = aView->ToScreen( m_cursorInWorld );
        const VECTOR2I canvas = gal->GetScreenPixelSize();

        origin = cursor + VECTOR2D( FOLLOW_OFFSET, FOLLOW_OFFSET );

        if( origin.x + width > canvas.x )
            origin.x = cursor.x - FOLLOW_OFFSET - width;

        if( origin.y + height > canvas.y )
            origin.y = cursor.y - FOLLOW_OFFSET - height;

        origin.x = std::max( 0.0, origin.x );
        origin.y = std::max( 0.0, origin.y );
    }

    // A dark canvas gets a black panel and white text, a light one a white panel and black text
    const bool           isDarkCanvas = luminance( m_background ) < 0.5;
    const KIGFX::COLOR4D panelColor = isDarkCanvas ? KIGFX::COLOR4D( 0.0, 0.0, 0.0, PANEL_OPACITY )
                                                   : KIGFX::COLOR4D( 1.0, 1.0, 1.0, PANEL_OPACITY );
    const KIGFX::COLOR4D textColor =
            isDarkCanvas ? KIGFX::COLOR4D( 1.0, 1.0, 1.0, 1.0 ) : KIGFX::COLOR4D( 0.0, 0.0, 0.0, 1.0 );

    auto toWorld = [&]( double aX, double aY )
    {
        return aView->ToWorld( VECTOR2D( aX, aY ) );
    };

    KIGFX::GAL_SCOPED_ATTRS scopedAttributes( *gal, KIGFX::GAL_SCOPED_ATTRS::ALL_ATTRS );

    // The panel is drawn first, and the text and the chip after it, a depth on, which puts them in front of it
    gal->SetIsFill( true );
    gal->SetIsStroke( true );
    gal->SetLineWidth( static_cast<float>( pixel ) );
    gal->SetFillColor( panelColor );
    gal->SetStrokeColor( textColor.WithAlpha( 0.5 ) );
    gal->DrawRectangle( toWorld( origin.x, origin.y ), toWorld( origin.x + width, origin.y + height ) );

    gal->AdvanceDepth();

    gal->ResetTextAttributes();
    gal->SetGlyphSize( VECTOR2I( KiROUND( glyph ), KiROUND( glyph ) ) );
    gal->SetHorizontalJustify( GR_TEXT_H_ALIGN_LEFT );
    gal->SetVerticalJustify( GR_TEXT_V_ALIGN_TOP );
    gal->SetFontBold( false );

    double y = origin.y + PANEL_PADDING;

    for( const HEADS_UP_LINE& line : m_lines )
    {
        double x = origin.x + PANEL_PADDING;

        if( line.isLayerLine )
        {
            gal->SetFillColor( m_state.layerColor.WithAlpha( 1.0 ) );
            gal->SetStrokeColor( textColor );
            gal->DrawRectangle( toWorld( x, y + 1.0 ), toWorld( x + chip, y + 1.0 + chip ) );

            x += chipSpace;
        }

        gal->SetStrokeColor( textColor );
        gal->SetFillColor( textColor );

        const VECTOR2D at = toWorld( x, y );

        gal->BitmapText( line.text, VECTOR2I( KiROUND( at.x ), KiROUND( at.y ) ), ANGLE_HORIZONTAL );
        y += rowHeight;
    }

    gal->ResetTextAttributes();
}

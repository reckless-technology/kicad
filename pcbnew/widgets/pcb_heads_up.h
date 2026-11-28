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

#ifndef PCB_HEADS_UP_H
#define PCB_HEADS_UP_H

#include <vector>

#include <gal/color4d.h>
#include <math/vector2d.h>
#include <view/view_item.h>

#include <widgets/pcb_heads_up_model.h>


/**
 * The heads-up display over the canvas of the PCB and footprint editors (interaction-profile note, 5.7).
 *
 * It is an item of the view on the general purpose overlay layer, drawn in screen space: it never moves with the
 * board, and it sits on the overlay target the cursor is already drawn on, so a change of what it says redraws that
 * target and not the scene. It says what #BuildHeadsUpLines makes of the state it is given, and says nothing new when
 * the state is the one it has.
 *
 * A window over the canvas was tried first and dropped: on a GTK canvas a child window of the GL canvas stayed
 * unpainted wherever it grew or moved, until it was hidden and shown again.
 *
 * It sits at the top left of the canvas, or near the cursor while it follows it. The panel is translucent, in black
 * or in white, whichever the canvas background is farther from, with text in the other of the two; text keeps a high
 * luminance contrast against it over a dark canvas, a light one or any color the board draws beneath. The layer's
 * color is shown in a chip with an outline in the text color, so it never has to be told from the panel by hue.
 */
class PCB_HEADS_UP : public KIGFX::VIEW_ITEM
{
public:
    PCB_HEADS_UP();

    /// Say what @a aState says. Returns false, and changes nothing, when it is what the display already says.
    bool SetState( const HEADS_UP_STATE& aState );

    /// Show or hide the display. Returns whether that changed anything.
    bool SetShown( bool aIsShown );
    bool IsShown() const { return m_isShown; }

    /**
     * Whether the display tracks the cursor, and where the cursor is in the view's world coordinates.
     * Returns whether that changed where the display is.
     */
    bool SetFollowing( bool aIsFollowing, const VECTOR2D& aCursorInWorld );

    /// The canvas background the panel is drawn against. Returns whether that changed anything.
    bool SetBackground( const KIGFX::COLOR4D& aBackground );

    // VIEW_ITEM
    wxString         GetClass() const override { return wxS( "PCB_HEADS_UP" ); }
    const BOX2I      ViewBBox() const override;
    void             ViewDraw( int aLayer, KIGFX::VIEW* aView ) const override;
    std::vector<int> ViewGetLayers() const override;
    double           ViewGetLOD( int aLayer, const KIGFX::VIEW* aView ) const override;

private:
    HEADS_UP_STATE             m_state;
    bool                       m_hasState;
    std::vector<HEADS_UP_LINE> m_lines;
    bool                       m_isShown;
    bool                       m_isFollowing;
    VECTOR2D                   m_cursorInWorld;
    KIGFX::COLOR4D             m_background;
    int                        m_widestLine; ///< the panel is this many characters wide
};

#endif // PCB_HEADS_UP_H

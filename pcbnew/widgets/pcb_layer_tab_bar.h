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

#ifndef PCB_LAYER_TAB_BAR_H
#define PCB_LAYER_TAB_BAR_H

#include <vector>

#include <wx/panel.h>
#include <wx/string.h>

#include <layer_ids.h>

class PCB_BASE_EDIT_FRAME;
class wxMenu;


/**
 * The layer tab bar docked under the canvas of the PCB and footprint editors (interaction-profile note, 5.7).
 *
 * A swatch in the active layer's color opens the layer presets. After it come one tab per visible layer in stack order,
 * each a color chip and the layer's name; the active tab is bold, drawn on a lighter background and underlined, so it
 * stands out by luminance and weight and not by hue alone. A click makes a layer active, and a right click offers
 * hiding the layer, showing all layers, showing only the copper layers and the presets. The bar scrolls when its tabs
 * do not fit.
 *
 * The bar holds no state of its own beyond how far it is scrolled. Every repaint reads the board's visible layers, the
 * color theme and the frame's active layer afresh, and the Appearance panel tells the bar when any of them changes.
 */
class PCB_LAYER_TAB_BAR : public wxPanel
{
public:
    explicit PCB_LAYER_TAB_BAR( PCB_BASE_EDIT_FRAME* aFrame );

    /// The visible layers, a color, the active layer, the layer names or the board changed: repaint and keep the
    /// active tab in view.
    void OnLayerStateChanged();

protected:
    wxSize DoGetBestSize() const override;

private:
    /// Where one tab sits when the bar is scrolled to the start
    struct TAB_SLOT
    {
        PCB_LAYER_ID layer;
        wxString     name;
        wxRect       rect;
    };

    /// The whole bar for the current state of the board, laid out for the window's present width
    struct LAYOUT
    {
        wxRect                swatch;
        wxRect                leftArrow;
        wxRect                rightArrow;
        wxRect                tabArea; ///< the part of the bar the tabs scroll in
        std::vector<TAB_SLOT> tabs;    ///< positions with no scroll offset applied
        int                   contentWidth = 0;
        bool                  isOverflowing = false;
    };

    LAYOUT computeLayout() const;

    int  maxScrollOffset( const LAYOUT& aLayout ) const;
    void scrollBy( int aPixels );
    void scrollActiveTabIntoView();

    void onPaint( wxPaintEvent& aEvent );
    void onSize( wxSizeEvent& aEvent );
    void onLeftDown( wxMouseEvent& aEvent );
    void onRightDown( wxMouseEvent& aEvent );
    void onMouseWheel( wxMouseEvent& aEvent );
    void onMenuCommand( wxCommandEvent& aEvent );

    void showPresetsMenu();
    void showContextMenu( PCB_LAYER_ID aLayer );
    void appendPresets( wxMenu* aMenu );

    PCB_BASE_EDIT_FRAME* m_frame;

    /// How far the tabs are scrolled, in pixels: the one thing the bar keeps
    int m_scrollOffset;

    /// Only for the length of one popup menu
    PCB_LAYER_ID          m_menuLayer;
    std::vector<wxString> m_menuPresetNames;
};

#endif // PCB_LAYER_TAB_BAR_H

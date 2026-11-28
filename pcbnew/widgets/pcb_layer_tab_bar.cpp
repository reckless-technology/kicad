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

#include "pcb_layer_tab_bar.h"

#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/menu.h>
#include <wx/settings.h>
#include <wx/translation.h>

#include <board.h>
#include <pcb_base_edit_frame.h>
#include <pcb_draw_panel_gal.h>
#include <settings/color_settings.h>
#include <widgets/appearance_controls.h>
#include <widgets/pcb_layer_tab_bar_model.h>


namespace
{
enum MENU_ID
{
    ID_HIDE_THIS_LAYER = wxID_HIGHEST + 4200,
    ID_SHOW_ALL_LAYERS,
    ID_SHOW_ONLY_COPPER_LAYERS,
    ID_PRESET_FIRST,
    ID_PRESET_LAST = ID_PRESET_FIRST + 255
};


double luminance( const wxColour& aColor )
{
    return ( 0.2126 * aColor.Red() + 0.7152 * aColor.Green() + 0.0722 * aColor.Blue() ) / 255.0;
}


wxColour blend( const wxColour& aFrom, const wxColour& aTo, double aAmount )
{
    auto mix = []( int aFromChannel, int aToChannel, double aWeight )
    {
        return static_cast<unsigned char>( aFromChannel + ( aToChannel - aFromChannel ) * aWeight + 0.5 );
    };

    return wxColour( mix( aFrom.Red(), aTo.Red(), aAmount ), mix( aFrom.Green(), aTo.Green(), aAmount ),
                     mix( aFrom.Blue(), aTo.Blue(), aAmount ) );
}
} // namespace


PCB_LAYER_TAB_BAR::PCB_LAYER_TAB_BAR( PCB_BASE_EDIT_FRAME* aFrame ) :
        wxPanel( aFrame, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE ),
        m_frame( aFrame ),
        m_scrollOffset( 0 ),
        m_menuLayer( UNDEFINED_LAYER )
{
    SetName( wxS( "LayerTabBar" ) );
    SetBackgroundStyle( wxBG_STYLE_PAINT );

    Bind( wxEVT_PAINT, &PCB_LAYER_TAB_BAR::onPaint, this );
    Bind( wxEVT_SIZE, &PCB_LAYER_TAB_BAR::onSize, this );
    Bind( wxEVT_LEFT_DOWN, &PCB_LAYER_TAB_BAR::onLeftDown, this );
    Bind( wxEVT_RIGHT_DOWN, &PCB_LAYER_TAB_BAR::onRightDown, this );
    Bind( wxEVT_MOUSEWHEEL, &PCB_LAYER_TAB_BAR::onMouseWheel, this );
    Bind( wxEVT_MENU, &PCB_LAYER_TAB_BAR::onMenuCommand, this, ID_HIDE_THIS_LAYER, ID_PRESET_LAST );
}


wxSize PCB_LAYER_TAB_BAR::DoGetBestSize() const
{
    wxClientDC dc( const_cast<PCB_LAYER_TAB_BAR*>( this ) );
    dc.SetFont( GetFont().Bold() );

    return wxSize( FromDIP( 240 ), dc.GetCharHeight() + FromDIP( 14 ) );
}


PCB_LAYER_TAB_BAR::LAYOUT PCB_LAYER_TAB_BAR::computeLayout() const
{
    LAYOUT layout;
    BOARD* board = m_frame->GetBoard();

    const wxSize client = GetClientSize();
    const int    edge = FromDIP( 6 );
    const int    arrowWidth = FromDIP( 18 );

    const int swatchSize = std::max( client.y - FromDIP( 8 ), FromDIP( 8 ) );
    layout.swatch = wxRect( edge, ( client.y - swatchSize ) / 2, swatchSize, swatchSize );

    const int tabsLeft = layout.swatch.GetRight() + 1 + FromDIP( 8 );

    if( !board )
    {
        layout.tabArea = wxRect( tabsLeft, 0, std::max( 0, client.x - tabsLeft - edge ), client.y );
        return layout;
    }

    wxClientDC dc( const_cast<PCB_LAYER_TAB_BAR*>( this ) );
    dc.SetFont( GetFont().Bold() ); // a tab is measured in bold so that making it active moves nothing

    const int tabPadding = FromDIP( 8 );
    const int chipSize = FromDIP( 10 );
    const int chipGap = FromDIP( 6 );
    const int tabTop = FromDIP( 3 );
    const int tabHeight = client.y - tabTop;

    const std::vector<PCB_LAYER_ID> order =
            LayerTabOrder( m_frame->GetAppearancePanel()->GetVisibleLayerSet(), board->GetEnabledLayers() );

    int x = 0;

    for( PCB_LAYER_ID layer : order )
    {
        const wxString name = board->GetLayerName( layer );
        const int      width = tabPadding + chipSize + chipGap + dc.GetTextExtent( name ).x + tabPadding;

        layout.tabs.push_back( { layer, name, wxRect( x, tabTop, width, tabHeight ) } );
        x += width + FromDIP( 2 );
    }

    layout.contentWidth = x;

    const int available = client.x - tabsLeft - edge;
    layout.isOverflowing = layout.contentWidth > available;

    if( layout.isOverflowing )
    {
        layout.leftArrow = wxRect( tabsLeft, 0, arrowWidth, client.y );
        layout.rightArrow = wxRect( client.x - edge - arrowWidth, 0, arrowWidth, client.y );
        layout.tabArea = wxRect( layout.leftArrow.GetRight() + 1, 0,
                                 std::max( 0, layout.rightArrow.x - layout.leftArrow.GetRight() - 1 ), client.y );
    }
    else
    {
        layout.tabArea = wxRect( tabsLeft, 0, std::max( 0, available ), client.y );
    }

    return layout;
}


int PCB_LAYER_TAB_BAR::maxScrollOffset( const LAYOUT& aLayout ) const
{
    return std::max( 0, aLayout.contentWidth - aLayout.tabArea.width );
}


void PCB_LAYER_TAB_BAR::scrollBy( int aPixels )
{
    const LAYOUT layout = computeLayout();

    m_scrollOffset = std::clamp( m_scrollOffset + aPixels, 0, maxScrollOffset( layout ) );
    Refresh();
}


void PCB_LAYER_TAB_BAR::scrollActiveTabIntoView()
{
    const LAYOUT       layout = computeLayout();
    const PCB_LAYER_ID active = m_frame->GetActiveLayer();

    for( const TAB_SLOT& tab : layout.tabs )
    {
        if( tab.layer != active )
            continue;

        if( tab.rect.x < m_scrollOffset )
            m_scrollOffset = tab.rect.x;
        else if( tab.rect.GetRight() + 1 > m_scrollOffset + layout.tabArea.width )
            m_scrollOffset = tab.rect.GetRight() + 1 - layout.tabArea.width;

        break;
    }

    m_scrollOffset = std::clamp( m_scrollOffset, 0, maxScrollOffset( layout ) );
}


void PCB_LAYER_TAB_BAR::OnLayerStateChanged()
{
    scrollActiveTabIntoView();
    Refresh();
}


void PCB_LAYER_TAB_BAR::onSize( wxSizeEvent& aEvent )
{
    m_scrollOffset = std::clamp( m_scrollOffset, 0, maxScrollOffset( computeLayout() ) );
    aEvent.Skip();
}


void PCB_LAYER_TAB_BAR::onPaint( wxPaintEvent& )
{
    wxAutoBufferedPaintDC dc( this );

    const wxColour background = wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE );
    const wxColour text = wxSystemSettings::GetColour( wxSYS_COLOUR_BTNTEXT );
    const bool     isDark = luminance( background ) < 0.5;

    // The active tab is lighter than the bar in light and dark themes alike, whatever the system colors are
    wxColour activeBackground =
            isDark ? background.ChangeLightness( 160 ) : wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW );

    if( luminance( activeBackground ) <= luminance( background ) )
        activeBackground = background.ChangeLightness( 115 );

    const wxColour hairline = blend( background, text, 0.3 );

    dc.SetBackground( wxBrush( background ) );
    dc.Clear();

    const wxSize client = GetClientSize();
    dc.SetPen( wxPen( hairline ) );
    dc.DrawLine( 0, 0, client.x, 0 );

    const LAYOUT       layout = computeLayout();
    BOARD*             board = m_frame->GetBoard();
    COLOR_SETTINGS*    theme = m_frame->GetColorSettings();
    const PCB_LAYER_ID active = m_frame->GetActiveLayer();
    const int          chipSize = FromDIP( 10 );

    auto layerColor = [&]( PCB_LAYER_ID aLayer )
    {
        return theme->GetColor( aLayer ).WithAlpha( 1.0 ).ToColour();
    };

    // The swatch of the active layer
    if( board )
    {
        dc.SetPen( wxPen( text ) );
        dc.SetBrush( wxBrush( layerColor( active ) ) );
        dc.DrawRectangle( layout.swatch );
    }

    dc.SetFont( GetFont() );

    if( layout.isOverflowing )
    {
        auto drawArrow = [&]( const wxRect& aRect, bool aIsLeft, bool aIsEnabled )
        {
            const int     centerX = aRect.x + aRect.width / 2;
            const int     centerY = aRect.y + aRect.height / 2;
            const int     half = FromDIP( 4 );
            const int     direction = aIsLeft ? -1 : 1;
            const wxPoint points[3] = { wxPoint( centerX + direction * half, centerY ),
                                        wxPoint( centerX - direction * half, centerY - half ),
                                        wxPoint( centerX - direction * half, centerY + half ) };

            const wxColour arrowColor = aIsEnabled ? text : blend( background, text, 0.35 );
            dc.SetPen( wxPen( arrowColor ) );
            dc.SetBrush( wxBrush( arrowColor ) );
            dc.DrawPolygon( 3, points );
        };

        drawArrow( layout.leftArrow, true, m_scrollOffset > 0 );
        drawArrow( layout.rightArrow, false, m_scrollOffset < maxScrollOffset( layout ) );
    }

    dc.SetClippingRegion( layout.tabArea );

    for( const TAB_SLOT& tab : layout.tabs )
    {
        wxRect rect = tab.rect;
        rect.x += layout.tabArea.x - m_scrollOffset;

        if( rect.GetRight() < layout.tabArea.x || rect.x > layout.tabArea.GetRight() )
            continue;

        const bool isActive = tab.layer == active;

        if( isActive )
        {
            dc.SetPen( *wxTRANSPARENT_PEN );
            dc.SetBrush( wxBrush( activeBackground ) );
            dc.DrawRectangle( rect );

            // The underline is the luminance cue that does not depend on telling colors apart
            dc.SetBrush( wxBrush( text ) );
            dc.DrawRectangle( rect.x, rect.GetBottom() + 1 - FromDIP( 3 ), rect.width, FromDIP( 3 ) );
        }

        const int chipX = rect.x + FromDIP( 8 );
        const int chipY = rect.y + ( rect.height - chipSize ) / 2;

        dc.SetPen( wxPen( text ) );
        dc.SetBrush( wxBrush( layerColor( tab.layer ) ) );
        dc.DrawRectangle( chipX, chipY, chipSize, chipSize );

        dc.SetFont( isActive ? GetFont().Bold() : GetFont() );
        dc.SetTextForeground( text );

        const wxSize extent = dc.GetTextExtent( tab.name );
        dc.DrawText( tab.name, chipX + chipSize + FromDIP( 6 ), rect.y + ( rect.height - extent.y ) / 2 );
    }

    dc.DestroyClippingRegion();
}


void PCB_LAYER_TAB_BAR::onLeftDown( wxMouseEvent& aEvent )
{
    const LAYOUT  layout = computeLayout();
    const wxPoint position = aEvent.GetPosition();

    if( layout.swatch.Contains( position ) )
    {
        showPresetsMenu();
        return;
    }

    if( layout.isOverflowing && layout.leftArrow.Contains( position ) )
    {
        scrollBy( -FromDIP( 120 ) );
        return;
    }

    if( layout.isOverflowing && layout.rightArrow.Contains( position ) )
    {
        scrollBy( FromDIP( 120 ) );
        return;
    }

    if( !layout.tabArea.Contains( position ) )
        return;

    for( const TAB_SLOT& tab : layout.tabs )
    {
        wxRect rect = tab.rect;
        rect.x += layout.tabArea.x - m_scrollOffset;

        if( rect.Contains( position ) )
        {
            m_frame->SetActiveLayer( tab.layer );
            m_frame->GetCanvas()->SetFocus();
            return;
        }
    }
}


void PCB_LAYER_TAB_BAR::onRightDown( wxMouseEvent& aEvent )
{
    const LAYOUT  layout = computeLayout();
    const wxPoint position = aEvent.GetPosition();

    PCB_LAYER_ID layer = UNDEFINED_LAYER;

    if( layout.tabArea.Contains( position ) )
    {
        for( const TAB_SLOT& tab : layout.tabs )
        {
            wxRect rect = tab.rect;
            rect.x += layout.tabArea.x - m_scrollOffset;

            if( rect.Contains( position ) )
                layer = tab.layer;
        }
    }

    showContextMenu( layer );
}


void PCB_LAYER_TAB_BAR::onMouseWheel( wxMouseEvent& aEvent )
{
    const int rotation = aEvent.GetWheelRotation();
    const int delta = std::max( 1, aEvent.GetWheelDelta() );

    // Wheel up scrolls toward the first tab
    scrollBy( -( rotation * FromDIP( 80 ) ) / delta );
}


void PCB_LAYER_TAB_BAR::appendPresets( wxMenu* aMenu )
{
    APPEARANCE_CONTROLS* appearance = m_frame->GetAppearancePanel();
    const wxString       current = appearance->GetActiveLayerPreset();

    m_menuPresetNames.clear();

    for( const auto& [name, label] : appearance->GetLayerPresetLabels() )
    {
        if( m_menuPresetNames.size() > ID_PRESET_LAST - ID_PRESET_FIRST )
            break;

        wxMenuItem* item =
                aMenu->AppendCheckItem( ID_PRESET_FIRST + static_cast<int>( m_menuPresetNames.size() ), label );
        item->Check( name == current );
        m_menuPresetNames.push_back( name );
    }
}


void PCB_LAYER_TAB_BAR::showPresetsMenu()
{
    wxMenu menu;
    appendPresets( &menu );

    m_menuLayer = UNDEFINED_LAYER;
    PopupMenu( &menu, wxPoint( 0, GetClientSize().y ) );
}


void PCB_LAYER_TAB_BAR::showContextMenu( PCB_LAYER_ID aLayer )
{
    wxMenu menu;

    if( aLayer != UNDEFINED_LAYER )
    {
        wxMenuItem* hide = menu.Append( ID_HIDE_THIS_LAYER, _( "Hide This Layer" ) );
        hide->Enable( m_frame->GetBoard() != nullptr );
    }

    menu.Append( ID_SHOW_ALL_LAYERS, _( "Show All Layers" ) );
    menu.Append( ID_SHOW_ONLY_COPPER_LAYERS, _( "Show Only Copper Layers" ) );
    menu.AppendSeparator();

    wxMenu* presets = new wxMenu;
    appendPresets( presets );
    menu.AppendSubMenu( presets, _( "Layer Presets" ) );

    m_menuLayer = aLayer;
    PopupMenu( &menu );
}


void PCB_LAYER_TAB_BAR::onMenuCommand( wxCommandEvent& aEvent )
{
    BOARD* board = m_frame->GetBoard();

    if( !board )
        return;

    APPEARANCE_CONTROLS* appearance = m_frame->GetAppearancePanel();
    const int            id = aEvent.GetId();

    if( id >= ID_PRESET_FIRST && id <= ID_PRESET_LAST )
    {
        const size_t index = static_cast<size_t>( id - ID_PRESET_FIRST );

        if( index < m_menuPresetNames.size() )
            appearance->ApplyLayerPreset( m_menuPresetNames[index] );

        return;
    }

    LAYER_TAB_COMMAND command = LAYER_TAB_COMMAND::SHOW_ALL_LAYERS;

    if( id == ID_HIDE_THIS_LAYER )
        command = LAYER_TAB_COMMAND::HIDE_THIS_LAYER;
    else if( id == ID_SHOW_ONLY_COPPER_LAYERS )
        command = LAYER_TAB_COMMAND::SHOW_ONLY_COPPER_LAYERS;

    const LSET visible = appearance->GetVisibleLayerSet();
    const LSET enabled = board->GetEnabledLayers();
    const LSET newVisible = ApplyLayerTabCommand( command, m_menuLayer, visible, enabled );

    // The active layer must stay one that has a tab
    const PCB_LAYER_ID active = m_frame->GetActiveLayer();

    if( !newVisible.test( active ) )
    {
        PCB_LAYER_ID next = LayerToActivateAfterHiding( active, visible, enabled );

        if( command != LAYER_TAB_COMMAND::HIDE_THIS_LAYER )
        {
            const std::vector<PCB_LAYER_ID> order = LayerTabOrder( newVisible, enabled );
            next = order.empty() ? UNDEFINED_LAYER : order.front();
        }

        if( next != UNDEFINED_LAYER )
            m_frame->SetActiveLayer( next );
    }

    appearance->SetVisibleLayerSet( newVisible );
}

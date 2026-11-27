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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <dialogs/key_sequence_popup.h>

#include <algorithm>
#include <memory>

#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/settings.h>

void KEY_SEQUENCE_POPUP_LAYOUT::Measure( const TEXT_EXTENT& aExtent, const wxFont& aFont, const wxString& aTitle,
                                         std::vector<KEY_SEQUENCE_ROW> aRows )
{
    m_font = aFont;
    m_title = aTitle;
    m_rows = std::move( aRows );

    m_lineHeight = aExtent( wxS( "Hg" ), false ).y;
    m_padding = std::max( 6, m_lineHeight / 2 );
    m_letterWidth = 0;
    m_labelWidth = 0;
    m_arrowWidth = 0;
    m_hotkeyWidth = 0;

    for( const KEY_SEQUENCE_ROW& row : m_rows )
    {
        if( row.m_isSeparator )
            continue;

        m_letterWidth = std::max( m_letterWidth,
                                  aExtent( row.m_letter ? wxString( row.m_letter ) : wxString( wxS( " " ) ), true ).x );
        m_labelWidth = std::max( m_labelWidth, aExtent( row.m_label, false ).x );
        m_hotkeyWidth = std::max( m_hotkeyWidth, aExtent( row.m_hotkeyText, false ).x );

        if( row.m_isSubmenu )
            m_arrowWidth = m_lineHeight;
    }

    m_titleHeight = m_title.IsEmpty() ? 0 : m_lineHeight + m_padding;

    int titleWidth = aExtent( m_title, true ).x;
    int rowsWidth = m_padding * 5 + m_letterWidth + m_labelWidth + m_arrowWidth + m_hotkeyWidth;
    int height = m_padding + m_titleHeight;

    for( const KEY_SEQUENCE_ROW& row : m_rows )
        height += rowHeight( row );

    m_size = wxSize( std::max( rowsWidth, titleWidth + m_padding * 2 ), height + m_padding );
}


KEY_SEQUENCE_ROW_COLORS KeySequenceRowColors( const KEY_SEQUENCE_ROW& aRow, bool aIsHovered,
                                              const KEY_SEQUENCE_POPUP_COLORS& aColors )
{
    KEY_SEQUENCE_ROW_COLORS colors;

    colors.m_text = aRow.m_isEnabled ? aColors.m_text : aColors.m_dimmed;

    if( aIsHovered && aRow.m_isEnabled )
    {
        // Highlight by luminance: the text color becomes the fill and the background the text
        colors.m_hasFill = true;
        colors.m_fill = aColors.m_text;
        colors.m_text = aColors.m_background;
    }

    return colors;
}


int KEY_SEQUENCE_POPUP_LAYOUT::rowHeight( const KEY_SEQUENCE_ROW& aRow ) const
{
    return aRow.m_isSeparator ? m_padding : m_lineHeight + m_padding / 2;
}


int KEY_SEQUENCE_POPUP_LAYOUT::RowAt( const wxPoint& aClientPosition ) const
{
    int y = m_padding / 2 + m_titleHeight;

    for( size_t i = 0; i < m_rows.size(); ++i )
    {
        int height = rowHeight( m_rows[i] );

        if( !m_rows[i].m_isSeparator && aClientPosition.y >= y && aClientPosition.y < y + height )
            return static_cast<int>( i );

        y += height;
    }

    return -1;
}


KEY_SEQUENCE_POPUP_COLORS KEY_SEQUENCE_POPUP_COLORS::FromSystem()
{
    return { wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ), wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOWTEXT ),
             wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT ) };
}


void KEY_SEQUENCE_POPUP_LAYOUT::Draw( wxDC& dc, int aHoveredRow, const KEY_SEQUENCE_POPUP_COLORS& aColors ) const
{
    const wxColour& background = aColors.m_background;
    const wxColour& text = aColors.m_text;
    const wxColour& dimmed = aColors.m_dimmed;
    wxSize   size = dc.GetSize();
    wxFont   bold = m_font.Bold();

    dc.SetBackground( wxBrush( background ) );
    dc.Clear();
    dc.SetFont( m_font );

    int y = m_padding / 2;

    if( !m_title.IsEmpty() )
    {
        dc.SetFont( bold );
        dc.SetTextForeground( text );
        dc.DrawText( m_title, m_padding, y );
        dc.SetFont( m_font );
        y += m_titleHeight;
    }

    int letterX = m_padding;
    int labelX = letterX + m_letterWidth + m_padding;
    int arrowX = labelX + m_labelWidth + m_padding;
    int hotkeyX = arrowX + m_arrowWidth + m_padding;

    for( size_t i = 0; i < m_rows.size(); ++i )
    {
        const KEY_SEQUENCE_ROW& row = m_rows[i];
        int                     height = rowHeight( row );

        if( row.m_isSeparator )
        {
            dc.SetPen( wxPen( dimmed ) );
            dc.DrawLine( m_padding, y + height / 2, size.x - m_padding, y + height / 2 );
            y += height;
            continue;
        }

        KEY_SEQUENCE_ROW_COLORS rowColors = KeySequenceRowColors( row, static_cast<int>( i ) == aHoveredRow, aColors );
        wxColour                rowText = rowColors.m_text;
        int                     textY = y + ( height - m_lineHeight ) / 2;

        if( rowColors.m_hasFill )
        {
            dc.SetPen( *wxTRANSPARENT_PEN );
            dc.SetBrush( wxBrush( rowColors.m_fill ) );
            dc.DrawRectangle( 0, y, size.x, height );
        }

        dc.SetTextForeground( rowText );
        dc.SetFont( bold );

        if( row.m_letter )
            dc.DrawText( wxString( row.m_letter ), letterX, textY );

        dc.SetFont( m_font );
        dc.DrawText( row.m_label, labelX, textY );

        if( row.m_isSubmenu )
        {
            int half = m_lineHeight / 4;
            int centerY = y + height / 2;

            wxPoint triangle[3] = { wxPoint( arrowX, centerY - half ), wxPoint( arrowX, centerY + half ),
                                    wxPoint( arrowX + half + 1, centerY ) };

            dc.SetPen( wxPen( rowText ) );
            dc.SetBrush( wxBrush( rowText ) );
            dc.DrawPolygon( 3, triangle );
        }

        if( !row.m_hotkeyText.IsEmpty() )
            dc.DrawText( row.m_hotkeyText, hotkeyX, textY );

        y += height;
    }
}


KEY_SEQUENCE_POPUP::KEY_SEQUENCE_POPUP( wxWindow* aFrame ) :
        wxPopupWindow( aFrame, wxBORDER_SIMPLE ),
        m_frame( aFrame )
{
    SetBackgroundStyle( wxBG_STYLE_PAINT );
    SetFont( aFrame->GetFont() );

    Bind( wxEVT_PAINT, &KEY_SEQUENCE_POPUP::onPaint, this );
    Bind( wxEVT_MOTION, &KEY_SEQUENCE_POPUP::onMouseMove, this );
    Bind( wxEVT_LEAVE_WINDOW, &KEY_SEQUENCE_POPUP::onMouseLeave, this );
    Bind( wxEVT_LEFT_UP, &KEY_SEQUENCE_POPUP::onLeftUp, this );
}


void KEY_SEQUENCE_POPUP::ShowRows( const wxString& aTitle, std::vector<KEY_SEQUENCE_ROW> aRows,
                                   const wxPoint& aScreenPosition )
{
    bool wasShown = IsShown();

    {
        wxClientDC dc( this );
        wxFont     font = GetFont();

        m_layout.Measure(
                [&]( const wxString& aText, bool aIsBold )
                {
                    dc.SetFont( aIsBold ? font.Bold() : font );
                    return dc.GetTextExtent( aText );
                },
                font, aTitle, std::move( aRows ) );
    }

    m_hoveredRow = -1;
    SetClientSize( m_layout.Size() );

    if( !wasShown )
    {
        wxPoint position = aScreenPosition;
        wxSize  size = GetSize();
        int     displayIndex = wxDisplay::GetFromPoint( aScreenPosition );
        wxRect  area = wxDisplay( displayIndex == wxNOT_FOUND ? 0 : displayIndex ).GetClientArea();

        position.x = std::max( area.GetLeft(), std::min( position.x, area.GetRight() - size.x ) );
        position.y = std::max( area.GetTop(), std::min( position.y, area.GetBottom() - size.y ) );
        Position( position, wxSize( 0, 0 ) );

        // A popup window does not take the focus when it is shown, which is the point of it
        Show( true );
    }

    Refresh();
}


void KEY_SEQUENCE_POPUP::HidePopup()
{
    if( IsShown() )
        Show( false );

    m_hoveredRow = -1;
}


void KEY_SEQUENCE_POPUP::SetHoveredRow( int aRow )
{
    if( aRow != m_hoveredRow )
    {
        m_hoveredRow = aRow;
        Refresh();
    }
}


void KEY_SEQUENCE_POPUP::onPaint( wxPaintEvent& )
{
    wxAutoBufferedPaintDC dc( this );

    m_layout.Draw( dc, m_hoveredRow, KEY_SEQUENCE_POPUP_COLORS::FromSystem() );
}


void KEY_SEQUENCE_POPUP::onMouseMove( wxMouseEvent& aEvent )
{
    SetHoveredRow( m_layout.RowAt( aEvent.GetPosition() ) );
    aEvent.Skip();
}


void KEY_SEQUENCE_POPUP::onMouseLeave( wxMouseEvent& aEvent )
{
    SetHoveredRow( -1 );
    aEvent.Skip();
}


void KEY_SEQUENCE_POPUP::onLeftUp( wxMouseEvent& aEvent )
{
    int row = m_layout.RowAt( aEvent.GetPosition() );

    if( row < 0 || !m_layout.Rows()[row].m_isEnabled || !m_layout.Rows()[row].m_letter || !m_onLetterClicked )
        return;

    // Through the owner and the engine, as if the letter had been typed
    m_onLetterClicked( m_layout.Rows()[row].m_letter );
}


bool KEY_SEQUENCE_POPUP::TryBefore( wxEvent& aEvent )
{
    wxEventType type = aEvent.GetEventType();

    if( type == wxEVT_CHAR_HOOK || type == wxEVT_CHAR || type == wxEVT_KEY_DOWN || type == wxEVT_KEY_UP )
    {
        // The popup is never given the focus, so this is only a safety net: the key goes to the window that has the
        // focus, or to the frame, and takes the path it would have taken had the popup not been there.
        wxWindow* target = wxWindow::FindFocus();

        if( !target || target == this || IsDescendant( target ) )
            target = m_frame;

        std::unique_ptr<wxEvent> copy( aEvent.Clone() );
        copy->SetEventObject( target );
        target->GetEventHandler()->ProcessEvent( *copy );
        return true;
    }

    return wxPopupWindow::TryBefore( aEvent );
}

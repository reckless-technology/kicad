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

// Colors come from the system's dialog colors and are never told apart by hue alone: the highlighted row swaps the
// text and background colors, which differ by luminance whatever the theme.


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


int KEY_SEQUENCE_POPUP::rowHeight( const KEY_SEQUENCE_ROW& aRow ) const
{
    return aRow.m_isSeparator ? m_padding : m_lineHeight + m_padding / 2;
}


void KEY_SEQUENCE_POPUP::measure()
{
    wxClientDC dc( this );
    dc.SetFont( GetFont() );

    wxFont bold = GetFont().Bold();

    m_lineHeight = dc.GetTextExtent( wxS( "Hg" ) ).y;
    m_padding = std::max( 6, m_lineHeight / 2 );
    m_letterWidth = 0;
    m_labelWidth = 0;
    m_arrowWidth = 0;
    m_hotkeyWidth = 0;

    for( const KEY_SEQUENCE_ROW& row : m_rows )
    {
        if( row.m_isSeparator )
            continue;

        dc.SetFont( bold );
        m_letterWidth = std::max(
                m_letterWidth, dc.GetTextExtent( row.m_letter ? wxString( row.m_letter ) : wxString( wxS( " " ) ) ).x );
        dc.SetFont( GetFont() );
        m_labelWidth = std::max( m_labelWidth, dc.GetTextExtent( row.m_label ).x );
        m_hotkeyWidth = std::max( m_hotkeyWidth, dc.GetTextExtent( row.m_hotkeyText ).x );

        if( row.m_isSubmenu )
            m_arrowWidth = m_lineHeight;
    }

    dc.SetFont( bold );
    m_titleHeight = m_title.IsEmpty() ? 0 : m_lineHeight + m_padding;

    int titleWidth = dc.GetTextExtent( m_title ).x;
    int rowsWidth = m_padding * 5 + m_letterWidth + m_labelWidth + m_arrowWidth + m_hotkeyWidth;
    int height = m_padding + m_titleHeight;

    for( const KEY_SEQUENCE_ROW& row : m_rows )
        height += rowHeight( row );

    SetClientSize( std::max( rowsWidth, titleWidth + m_padding * 2 ), height + m_padding );
}


void KEY_SEQUENCE_POPUP::ShowRows( const wxString& aTitle, std::vector<KEY_SEQUENCE_ROW> aRows,
                                   const wxPoint& aScreenPosition )
{
    bool wasShown = IsShown();

    m_title = aTitle;
    m_rows = std::move( aRows );
    m_hoveredRow = -1;
    measure();

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


int KEY_SEQUENCE_POPUP::RowAt( const wxPoint& aClientPosition ) const
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


void KEY_SEQUENCE_POPUP::onPaint( wxPaintEvent& )
{
    wxAutoBufferedPaintDC dc( this );

    wxColour background = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW );
    wxColour text = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOWTEXT );
    wxColour dimmed = wxSystemSettings::GetColour( wxSYS_COLOUR_GRAYTEXT );
    wxSize   size = GetClientSize();
    wxFont   bold = GetFont().Bold();

    dc.SetBackground( wxBrush( background ) );
    dc.Clear();
    dc.SetFont( GetFont() );

    int y = m_padding / 2;

    if( !m_title.IsEmpty() )
    {
        dc.SetFont( bold );
        dc.SetTextForeground( text );
        dc.DrawText( m_title, m_padding, y );
        dc.SetFont( GetFont() );
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

        bool     isHovered = static_cast<int>( i ) == m_hoveredRow && row.m_isEnabled;
        wxColour rowText = row.m_isEnabled ? text : dimmed;
        int      textY = y + ( height - m_lineHeight ) / 2;

        if( isHovered )
        {
            // Highlight by luminance: the text color becomes the fill and the background the text
            dc.SetPen( *wxTRANSPARENT_PEN );
            dc.SetBrush( wxBrush( text ) );
            dc.DrawRectangle( 0, y, size.x, height );
            rowText = background;
        }

        dc.SetTextForeground( rowText );
        dc.SetFont( bold );

        if( row.m_letter )
            dc.DrawText( wxString( row.m_letter ), letterX, textY );

        dc.SetFont( GetFont() );
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


void KEY_SEQUENCE_POPUP::onMouseMove( wxMouseEvent& aEvent )
{
    int row = RowAt( aEvent.GetPosition() );

    if( row != m_hoveredRow )
    {
        m_hoveredRow = row;
        Refresh();
    }

    aEvent.Skip();
}


void KEY_SEQUENCE_POPUP::onMouseLeave( wxMouseEvent& aEvent )
{
    if( m_hoveredRow != -1 )
    {
        m_hoveredRow = -1;
        Refresh();
    }

    aEvent.Skip();
}


void KEY_SEQUENCE_POPUP::onLeftUp( wxMouseEvent& aEvent )
{
    int row = RowAt( aEvent.GetPosition() );

    if( row < 0 || !m_rows[row].m_isEnabled || !m_rows[row].m_letter || !m_onLetterClicked )
        return;

    // Through the owner and the engine, as if the letter had been typed
    m_onLetterClicked( m_rows[row].m_letter );
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

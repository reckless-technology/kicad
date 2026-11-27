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

#ifndef KEY_SEQUENCE_POPUP_H
#define KEY_SEQUENCE_POPUP_H

#include <functional>
#include <vector>

#include <key_sequence_rows.h>

#include <wx/popupwin.h>

class wxPaintEvent;
class wxMouseEvent;


/**
 * The key-sequence hint drawn by the program instead of by the toolkit's native menu (interaction-profile note, 5.3),
 * for macOS, whose native menus run a modal loop that gives the key-sequence engine no keys and fire a bare-letter key
 * equivalent anywhere in the menu tree.
 *
 * It is a popup window that never takes keyboard focus, so keys keep reaching the frame's own key path, where the
 * engine sees them exactly as it does when a sequence is typed fast. A key that does arrive here is handed back to that
 * path. A click on an enabled row is reported through the click handler, which the owner turns into the row's letter.
 *
 * It is compiled on every platform and its drawing is tested on Linux: KEY_SEQUENCE_CONTROLLER::UsesPopupWindow()
 * says when it replaces the native menu.
 */
class KEY_SEQUENCE_POPUP : public wxPopupWindow
{
public:
    /// \a aFrame is the window the keys go back to, and the parent whose font and colors the popup uses.
    explicit KEY_SEQUENCE_POPUP( wxWindow* aFrame );

    /// Called with the letter of an enabled row the user clicked.
    void SetClickHandler( std::function<void( wxUniChar )> aHandler ) { m_onLetterClicked = std::move( aHandler ); }

    /// Draw \a aRows with \a aTitle above them, at the screen position \a aScreenPosition (kept inside the display), and
    /// show the window without activating it. Calling it again redraws the same window with another node.
    void ShowRows( const wxString& aTitle, std::vector<KEY_SEQUENCE_ROW> aRows, const wxPoint& aScreenPosition );

    /// Hide the window. Does nothing when it is hidden.
    void HidePopup();

    /// The rows now drawn
    const std::vector<KEY_SEQUENCE_ROW>& Rows() const { return m_rows; }

    /// The row under \a aClientPosition, or -1: separators and the title are not rows to hit.
    int RowAt( const wxPoint& aClientPosition ) const;

protected:
    bool TryBefore( wxEvent& aEvent ) override;

private:
    void onPaint( wxPaintEvent& aEvent );
    void onMouseMove( wxMouseEvent& aEvent );
    void onMouseLeave( wxMouseEvent& aEvent );
    void onLeftUp( wxMouseEvent& aEvent );
    void measure();
    int  rowHeight( const KEY_SEQUENCE_ROW& aRow ) const;

    wxWindow*                        m_frame;
    wxString                         m_title;
    std::vector<KEY_SEQUENCE_ROW>    m_rows;
    std::function<void( wxUniChar )> m_onLetterClicked;
    int                              m_hoveredRow = -1;

    // The measured layout, in pixels
    int m_padding = 0;
    int m_letterWidth = 0;
    int m_labelWidth = 0;
    int m_arrowWidth = 0;
    int m_hotkeyWidth = 0;
    int m_titleHeight = 0;
    int m_lineHeight = 0;
};

#endif // KEY_SEQUENCE_POPUP_H

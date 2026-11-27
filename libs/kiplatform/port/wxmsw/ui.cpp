/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 Ian McInerney <Ian.S.McInerney at ieee.org>
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

#include <windows.h>

#include <kiplatform/shared_state.h>
#include <kiplatform/ui.h>

#include <wx/cursor.h>
#include <wx/dataview.h>
#include <wx/dialog.h>
#include <wx/dir.h>
#include <wx/display.h>
#include <wx/filename.h>
#include <wx/font.h>
#include <wx/fontenum.h>
#include <wx/nonownedwnd.h>
#include <wx/window.h>
#include <wx/msw/registry.h>


bool KIPLATFORM::UI::IsDarkTheme()
{
#if wxCHECK_VERSION( 3, 3, 0 )
    wxSystemAppearance appearance = wxSystemSettings::GetAppearance();
    return appearance.IsDark();
#else
    wxColour bg = wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW );

    // Weighted W3C formula
    double brightness = ( bg.Red() / 255.0 ) * 0.299 +
        ( bg.Green() / 255.0 ) * 0.587 +
        ( bg.Blue() / 255.0 ) * 0.117;

    return brightness < 0.5;
#endif
}


wxColour KIPLATFORM::UI::GetDialogBGColour()
{
    return wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE );
}


void KIPLATFORM::UI::GetInfoBarColours( wxColour& aFGColour, wxColour& aBGColour )
{
    aBGColour = wxSystemSettings::GetColour( wxSYS_COLOUR_INFOBK );
    aFGColour = wxSystemSettings::GetColour( wxSYS_COLOUR_INFOTEXT );
}


wxWindow* KIPLATFORM::UI::CreateCanvasOverlay( wxWindow* )
{
    return nullptr;
}


void KIPLATFORM::UI::ForceFocus( wxWindow* aWindow )
{
    aWindow->SetFocus();
}


bool KIPLATFORM::UI::IsWindowActive( wxWindow* aWindow )
{
    if(! aWindow )
    {
	    return false;
    }

    return ( aWindow->GetHWND() == GetForegroundWindow() );
}


void KIPLATFORM::UI::RunWhenActive( wxWindow*, std::function<void()> aFunction )
{
    aFunction();
}


void KIPLATFORM::UI::SetFocusWhenActive( wxWindow* aWindow )
{
    aWindow->SetFocus();
}


bool KIPLATFORM::UI::ActivateWithoutPresenting( wxWindow* )
{
    return false;
}


void KIPLATFORM::UI::StabilizeWindowPosition( wxWindow* )
{
    // Not needed on this platform
}


wxPoint KIPLATFORM::UI::GetRestorablePosition( wxWindow* aWindow )
{
    return aWindow->GetPosition();
}


void KIPLATFORM::UI::ReparentModal( wxNonOwnedWindow* )
{
    // Not needed on this platform
}


void KIPLATFORM::UI::ReparentWindow( wxNonOwnedWindow*, wxTopLevelWindow* )
{
    // Not needed on this platform (used only on macOS for child window ordering)
}


void KIPLATFORM::UI::FixupCancelButtonCmdKeyCollision( wxWindow* )
{
    // Not needed on this platform
}


bool KIPLATFORM::UI::IsStockCursorOk( wxStockCursor aCursor )
{
    switch( aCursor )
    {
    case wxCURSOR_BULLSEYE:
    case wxCURSOR_HAND:
    case wxCURSOR_ARROW:
        return true;
    default:
        return false;
    }
}


void KIPLATFORM::UI::LargeChoiceBoxHack( wxChoice* )
{
    // Not implemented
}


void KIPLATFORM::UI::EllipsizeChoiceBox( wxChoice* )
{
    // Not implemented
}


double KIPLATFORM::UI::GetPixelScaleFactor( const wxWindow* aWindow )
{
    return aWindow->GetContentScaleFactor();
}


double KIPLATFORM::UI::GetContentScaleFactor( const wxWindow* aWindow )
{
    return aWindow->GetDPIScaleFactor();
}


double KIPLATFORM::UI::GetDisplayRefreshRate( const wxWindow* aWindow )
{
    // wx reports the hardware's default rate as 0
    return wxDisplay( aWindow ).GetCurrentMode().refresh;
}


wxSize KIPLATFORM::UI::GetUnobscuredSize( const wxWindow* aWindow )
{
    return aWindow->GetClientSize();
}


void KIPLATFORM::UI::SetOverlayScrolling( const wxWindow*, bool )
{
    // Not implemented
}


bool KIPLATFORM::UI::AllowIconsInMenus()
{
    return true;
}


bool KIPLATFORM::UI::SupportsKeyState()
{
    return true;
}


wxKeyEvent KIPLATFORM::UI::GetNativePopupDismissalEvent()
{
    return wxKeyEvent();
}


bool KIPLATFORM::UI::PrepareChoicePopup( wxWindow* )
{
    return true;
}


wxPoint KIPLATFORM::UI::GetMousePosition()
{
    return wxGetMousePosition();
}


bool KIPLATFORM::UI::WarpPointer( wxWindow* aWindow, int aX, int aY )
{
    aWindow->WarpPointer( aX, aY );
    return true;
}


void KIPLATFORM::UI::ImmControl( wxWindow* aWindow, bool aEnable )
{
    if ( !aEnable )
    {
        ImmAssociateContext( aWindow->GetHWND(), NULL );
    }
    else
    {
        ImmAssociateContextEx( aWindow->GetHWND(), 0, IACE_DEFAULT );
    }
}


void KIPLATFORM::UI::ImeNotifyCancelComposition( wxWindow* aWindow )
{
    const HIMC himc = ImmGetContext( aWindow->GetHWND() );
    ImmNotifyIME( himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0 );
    ImmReleaseContext( aWindow->GetHWND(), himc );
}


bool KIPLATFORM::UI::InfiniteDragPrepareWindow( wxWindow* )
{
    return true;
}


void KIPLATFORM::UI::InfiniteDragReleaseWindow()
{
    // Not needed on this platform
}


void KIPLATFORM::UI::SetFloatLevel( wxWindow* )
{
}

void KIPLATFORM::UI::ReleaseChildWindow( wxNonOwnedWindow* )
{
    // Not needed on this platform
}


void KIPLATFORM::UI::AllowNetworkFileSystems( wxDialog* )
{
    // Not needed on Windows - file dialogs show network filesystems by default
}


int KIPLATFORM::UI::GetColumnWidth( const wxDataViewColumn* aColumn )
{
    return aColumn->WXGetSpecifiedWidth();
}


void KIPLATFORM::UI::CancelPendingScroll( wxDataViewCtrl* )
{
    // Not needed on this platform; the native control does not race on model resets.
}


void KIPLATFORM::UI::SetWMClass( wxWindow*, const wxString& )
{
    // WM_CLASS is an X11/Wayland concept; taskbar identity on Windows is driven by the
    // AppUserModelID (see KIPLATFORM::ENV::SetAppDetailsForWindow).
}


void KIPLATFORM::UI::GetNativeWindowHandles( wxWindow* aWindow, void** aHandle, void** aDisplay,
                                              int* aDisplayType )
{
    *aHandle = nullptr;
    *aDisplay = nullptr;

    if( aDisplayType )
        *aDisplayType = NATIVE_DISPLAY_TYPE::DISPLAY_WIN32;

    if( aWindow )
        *aHandle = static_cast<void*>( aWindow->GetHWND() );
}


void KIPLATFORM::UI::RegisterPrivateFonts( const wxString& aDirectory )
{
    if( !wxDir::Exists( aDirectory ) )
        return;

#if wxUSE_PRIVATE_FONTS
    if( !wxFont::CanUsePrivateFonts() )
        return;
#endif

    wxDir dir( aDirectory );

    if( !dir.IsOpened() )
        return;

    for( const wxString& pattern : { wxS( "*.ttf" ), wxS( "*.otf" ) } )
    {
        wxString name;

        for( bool found = dir.GetFirst( &name, pattern, wxDIR_FILES ); found; found = dir.GetNext( &name ) )
        {
            const wxString path = wxFileName( aDirectory, name ).GetFullPath();

#if wxUSE_PRIVATE_FONTS
            wxFont::AddPrivateFont( path );
#else
            // A wx built without private fonts: this is the call it would make, private to this process
            ::AddFontResourceExW( path.wc_str(), FR_PRIVATE, nullptr );
#endif
        }
    }

    // wx lists the face names once and keeps the list, which would not have these
    wxFontEnumerator::InvalidateCache();
}


void KIPLATFORM::UI::SetInterfaceFontFamily( const char* aFamily )
{
    const wxString family = wxString::FromUTF8( aFamily );

    // wxMSW keeps its GUI default font to itself (from the system's message font), so there is nothing to set:
    // ApplyInterfaceFont() gives the family to each top-level window instead
    if( wxFontEnumerator::IsValidFacename( family ) )
        SHARED_STATE::Set( SHARED_STATE::INTERFACE_FONT_FAMILY, family );
}


void KIPLATFORM::UI::ApplyInterfaceFont( wxWindow* aTopLevel )
{
    const wxString family = GetInterfaceFontFamily();

    if( !aTopLevel || family.IsEmpty() )
        return;

    // A window not yet given a font has the system's GUI font at its own DPI. The controls made in it afterwards,
    // and the dialogs it is the parent of, inherit the font set here. Native menus and the title bar keep the
    // system's.
    wxFont font = aTopLevel->GetFont();

    if( font.IsOk() && font.SetFaceName( family ) )
        aTopLevel->SetFont( font );
}


wxString KIPLATFORM::UI::GetInterfaceFontFamily()
{
    return SHARED_STATE::Get( SHARED_STATE::INTERFACE_FONT_FAMILY );
}


void KIPLATFORM::UI::PrepareForExternalRendering( wxWindow* )
{
    // The GPU API owns Win32 surfaces natively
}


void KIPLATFORM::UI::KeepAboveExternalRendering( wxWindow* )
{
    // Child windows are native and clip their parent's surface
}


void KIPLATFORM::UI::RebindOnRealize( wxWindow* )
{
    // Win32 windows keep their HWND when reparented
}

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

#include <kiplatform/shared_state.h>
#include <kiplatform/ui.h>

#import <Cocoa/Cocoa.h>

#include <wx/dataview.h>
#include <wx/dialog.h>
#include <wx/display.h>
#include <wx/nonownedwnd.h>
#include <wx/toplevel.h>
#include <wx/button.h>
#include <wx/settings.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/osx/core/cfref.h>
#include <wx/osx/core/cfstring.h>


bool KIPLATFORM::UI::IsDarkTheme()
{
    // Disabled for now because it appears that the wxWidgets event goes out before the
    // NSAppearance name has been updated
#ifdef NOTYET
    // It appears the wxWidgets event goes out before the NSAppearance name has been updated
    NSString *appearanceName = [[NSAppearance currentAppearance] name];
    return !![appearanceName containsString:@"Dark"];
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
     wxColor bg = wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE );

    if( KIPLATFORM::UI::IsDarkTheme() )
        bg = bg.ChangeLightness( 80 );
    else
        bg = bg.ChangeLightness( 160 );

    return bg;
}


void KIPLATFORM::UI::GetInfoBarColours( wxColour& aFGColour, wxColour& aBGColour )
{
    aFGColour = wxSystemSettings::GetColour( wxSYS_COLOUR_INFOTEXT );

    // wxWidgets hard-codes wxSYS_COLOUR_INFOBK to { 0xFF, 0xFF, 0xD3 } on Mac.
    if( KIPLATFORM::UI::IsDarkTheme() )
        aBGColour = wxColour( 28, 27, 20 );
    else
        aBGColour = wxColour( 255, 249, 189 );
}


wxWindow* KIPLATFORM::UI::CreateCanvasOverlay( wxWindow* aCanvas [[maybe_unused]] )
{
    return nullptr;
}


void KIPLATFORM::UI::RegisterPrivateFonts( const wxString& aDirectory )
{
    // wxFont::AddPrivateFont() registers nothing on wxOSX: it only accepts a file already in the bundle's
    // Resources/Fonts, listed ahead of time in Info.plist. CoreText registers a file for this process alone, and
    // a face registered that way is found by its family name, by CoreText and so by wxFont.
    wxDir directory( aDirectory );

    if( !directory.IsOpened() )
        return;

    for( const wxString& pattern : { wxString( wxS( "*.ttf" ) ), wxString( wxS( "*.otf" ) ) } )
    {
        wxString name;

        for( bool found = directory.GetFirst( &name, pattern, wxDIR_FILES ); found;
             found = directory.GetNext( &name ) )
        {
            wxFileName file( aDirectory, name );
            NSURL*     url = [NSURL fileURLWithPath:wxCFStringRef( file.GetFullPath() ).AsNSString()];
            CFErrorRef error = nullptr;

            // A face already registered (a second call in one process) reports an error and stays registered
            if( !CTFontManagerRegisterFontsForURL( (__bridge CFURLRef) url, kCTFontManagerScopeProcess, &error )
                && error )
            {
                CFRelease( error );
            }
        }
    }
}


void KIPLATFORM::UI::SetInterfaceFontFamily( const char* aFamily )
{
    // Leave the system font alone if the family did not resolve: CoreText answers an unknown name with a
    // substitute, so compare the family it gave back
    wxCFRef<CTFontRef> font( CTFontCreateWithName( wxCFStringRef( wxString::FromUTF8( aFamily ) ), 0.0, nullptr ) );
    wxCFStringRef      resolved( CTFontCopyFamilyName( font ) );

    if( resolved.AsString().CmpNoCase( wxString::FromUTF8( aFamily ) ) != 0 )
        return;

    // wxOSX has no GUI default font to set: ApplyInterfaceFont() gives it to each top-level window instead
    SHARED_STATE::Set( SHARED_STATE::INTERFACE_FONT_FAMILY, resolved.AsString() );
}


void KIPLATFORM::UI::ApplyInterfaceFont( wxWindow* aTopLevel )
{
    const wxString family = GetInterfaceFontFamily();

    if( !aTopLevel || family.IsEmpty() )
        return;

    // A control made without a font of its own inherits its parent's (wxWindowMac::MacPostControlCreate calls
    // InheritAttributes() before falling back to the system font), so a font set here, before the window's
    // children are made, reaches all of them. The size is the one those controls would have had.
    wxFont font( wxOSX_SYSTEM_FONT_NORMAL );
    font.SetFaceName( family );

    if( font.IsOk() )
        aTopLevel->SetFont( font );
}


wxString KIPLATFORM::UI::GetInterfaceFontFamily()
{
    return SHARED_STATE::Get( SHARED_STATE::INTERFACE_FONT_FAMILY );
}


void KIPLATFORM::UI::ForceFocus( wxWindow* aWindow )
{
    // On OSX we need to forcefully give the focus to the window
    [[aWindow->GetHandle() window] makeFirstResponder: aWindow->GetHandle()];
}


bool KIPLATFORM::UI::IsWindowActive( wxWindow* aWindow [[maybe_unused]] )
{
    // Just always return true
    return true;
}


void KIPLATFORM::UI::RunWhenActive( wxWindow* aWindow [[maybe_unused]], std::function<void()> aFunction )
{
    aFunction();
}


void KIPLATFORM::UI::SetFocusWhenActive( wxWindow* aWindow )
{
    aWindow->SetFocus();
}


bool KIPLATFORM::UI::ActivateWithoutPresenting( wxWindow* aWindow [[maybe_unused]] )
{
    return false;
}


void KIPLATFORM::UI::StabilizeWindowPosition( wxWindow* aWindow [[maybe_unused]] )
{
    // Not needed on this platform
}


wxPoint KIPLATFORM::UI::GetRestorablePosition( wxWindow* aWindow )
{
    return aWindow->GetPosition();
}


void KIPLATFORM::UI::ReparentWindow( wxNonOwnedWindow* aWindow, wxTopLevelWindow* aParent )
{
    NSWindow* parentWindow = aParent->GetWXWindow();
    NSWindow* theWindow    = aWindow->GetWXWindow();

    if( parentWindow && theWindow )
        [parentWindow addChildWindow:theWindow ordered:NSWindowAbove];
}


void KIPLATFORM::UI::ReparentModal( wxNonOwnedWindow* aWindow )
{
    // Quietly return if no parent is found
    if( wxTopLevelWindow* parent = static_cast<wxTopLevelWindow*>( wxGetTopLevelParent( aWindow->GetParent() ) ) )
        ReparentWindow( aWindow, parent );
}


void KIPLATFORM::UI::FixupCancelButtonCmdKeyCollision( wxWindow *aWindow )
{
    wxButton* button = dynamic_cast<wxButton*>( wxWindow::FindWindowById( wxID_CANCEL, aWindow ) );

    if( button )
    {
        static const wxString placeholder = wxT( "{amp}" );

        wxString buttonLabel = button->GetLabel();
        buttonLabel.Replace( wxT( "&&" ), placeholder );
        buttonLabel.Replace( wxT( "&" ), wxEmptyString );
        buttonLabel.Replace( placeholder, wxT( "&" ) );
        button->SetLabel( buttonLabel );
    }
}


bool KIPLATFORM::UI::IsStockCursorOk( wxStockCursor aCursor )
{
    switch( aCursor )
    {
    case wxCURSOR_SIZING:
    case wxCURSOR_BULLSEYE:
    case wxCURSOR_HAND:
    case wxCURSOR_ARROW:
        return true;
    default:
        return false;
    }
}


void KIPLATFORM::UI::LargeChoiceBoxHack( wxChoice* aChoice [[maybe_unused]] )
{
    // Not implemented
}


void KIPLATFORM::UI::EllipsizeChoiceBox( wxChoice* aChoice [[maybe_unused]] )
{
    // Not implemented
}


double KIPLATFORM::UI::GetPixelScaleFactor( const wxWindow* aWindow )
{
    return aWindow->GetContentScaleFactor();
}


double KIPLATFORM::UI::GetContentScaleFactor( const wxWindow* aWindow )
{
    // Native GUI resolution on Retina displays
    return GetPixelScaleFactor( aWindow ) / 2.0;
}


double KIPLATFORM::UI::GetDisplayRefreshRate( const wxWindow* aWindow )
{
    NSView*   view = (NSView*) aWindow->GetHandle();
    NSScreen* screen = view.window.screen;

    if( !screen )
        return 0.0;

    if( @available( macOS 12.0, * ) )
        return static_cast<double>( screen.maximumFramesPerSecond );

    // Built-in panels report 0 here
    return wxDisplay( aWindow ).GetCurrentMode().refresh;
}


wxSize KIPLATFORM::UI::GetUnobscuredSize( const wxWindow* aWindow )
{
    return wxSize( aWindow->GetSize().GetX() - wxSystemSettings::GetMetric( wxSYS_VSCROLL_X ),
                   aWindow->GetSize().GetY() - wxSystemSettings::GetMetric( wxSYS_HSCROLL_Y ) );
}


void KIPLATFORM::UI::SetOverlayScrolling( const wxWindow* aWindow [[maybe_unused]], bool overlay [[maybe_unused]] )
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
    // wxOSX flips the pointer against screen 0, which raises NSRangeException with no window
    // server attached
    if( [[NSScreen screens] count] == 0 )
        return wxPoint( 0, 0 );

    return wxGetMousePosition();
}


bool KIPLATFORM::UI::WarpPointer( wxWindow* aWindow, int aX, int aY )
{
    aWindow->WarpPointer( aX, aY );
    return true;
}


void KIPLATFORM::UI::ImmControl( wxWindow* aWindow [[maybe_unused]], bool aEnable [[maybe_unused]] )
{
}


void KIPLATFORM::UI::ImeNotifyCancelComposition( wxWindow* aWindow [[maybe_unused]] )
{
}


bool KIPLATFORM::UI::InfiniteDragPrepareWindow( wxWindow* aWindow [[maybe_unused]] )
{
    return true;
}


void KIPLATFORM::UI::InfiniteDragReleaseWindow()
{
    // Not needed on this platform
}


void KIPLATFORM::UI::SetFloatLevel( wxWindow* aWindow )
{
    // On OSX we need to forcefully give the focus to the window
    [[aWindow->GetHandle() window] setLevel:NSFloatingWindowLevel];
}

void KIPLATFORM::UI::ReleaseChildWindow( wxNonOwnedWindow* aWindow )
{
    NSWindow* theWindow = aWindow->GetWXWindow();

    // ReparentWindow can attach to a window other than the wx parent, e.g. a report onto the DRC dialog
    if( NSWindow* parentWindow = [theWindow parentWindow] )
    {
        [parentWindow removeChildWindow:theWindow];
        [theWindow setLevel:NSFloatingWindowLevel];
    }
}


void KIPLATFORM::UI::AllowNetworkFileSystems( wxDialog* aDialog [[maybe_unused]] )
{
    // Not needed on macOS - file dialogs show network filesystems by default
}


int KIPLATFORM::UI::GetColumnWidth( const wxDataViewColumn* aColumn )
{
    return aColumn->WXGetSpecifiedWidth();
}


void KIPLATFORM::UI::CancelPendingScroll( wxDataViewCtrl* aCtrl [[maybe_unused]] )
{
    // Not needed on this platform; the native control does not race on model resets.
}


void KIPLATFORM::UI::SetWMClass( wxWindow* aWindow [[maybe_unused]], const wxString& aClass [[maybe_unused]] )
{
    // Not applicable on macOS; the bundle identifier provides the application identity.
}


void KIPLATFORM::UI::GetNativeWindowHandles( wxWindow* aWindow, void** aHandle, void** aDisplay,
                                             int* aDisplayType )
{
    *aHandle = nullptr;
    *aDisplay = nullptr;

    if( aDisplayType )
        *aDisplayType = NATIVE_DISPLAY_TYPE::DISPLAY_COCOA;

    if( aWindow )
        *aHandle = aWindow->GetHandle();
}


void KIPLATFORM::UI::PrepareForExternalRendering( wxWindow* aWindow [[maybe_unused]] )
{
    // macOS composites Metal layers natively
}


void KIPLATFORM::UI::KeepAboveExternalRendering( wxWindow* aChild [[maybe_unused]] )
{
    // Child views are composited above the Metal layer of their parent
}


void KIPLATFORM::UI::RebindOnRealize( wxWindow* aWindow [[maybe_unused]] )
{
    // An NSView keeps its identity when reparented
}

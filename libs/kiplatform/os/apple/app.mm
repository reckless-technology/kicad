/*
* This program source code file is part of KiCad, a free EDA CAD application.
*
* Copyright (C) 2020 Mark Roszko <mark.roszko@gmail.com>
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

#include <kiplatform/app.h>

#import <AppKit/AppKit.h>

#include <pthread.h>
#include <unistd.h>

#include <wx/string.h>
#include <wx/sysopt.h>


bool KIPLATFORM::APP::Init()
{
    // KiCad relies on showing the file type selector in a few places; force it to be shown
    wxSystemOptions::SetOption( wxS( "osx.openfiledialog.always-show-types" ), 1 );

    // Start in the light appearance. The programs call EnableDarkMode() for the DARK theme (forced) and for AUTO
    // (follow the system) but make no call for LIGHT, so this light start is what LIGHT gets - as on Windows, where
    // light is what a program gets unless it asks for dark. It runs before the first window. NSApp is nil in a
    // console program (kicad-cli, the QA test modules), where this does nothing.
    [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];

    return true;
}


void KIPLATFORM::APP::EnableDarkMode( bool aForce )
{
    // Forced: the dark appearance whatever the system's. Not forced: no appearance of our own, so the program follows
    // the system's and changes with it while running.
    [NSApp setAppearance:aForce ? [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua] : nil];
}


bool KIPLATFORM::APP::AttachConsole( bool aTryAlloc [[maybe_unused]] )
{
    // Not implemented on this platform
    return true;
}


bool KIPLATFORM::APP::IsOperatingSystemUnsupported()
{
    // Not implemented on this platform
    return false;
}


bool KIPLATFORM::APP::RegisterApplicationRestart( const wxString& aCommandLine [[maybe_unused]] )
{
    // Not implemented on this platform
    return true;
}


bool KIPLATFORM::APP::UnregisterApplicationRestart()
{
    // Not implemented on this platform
    return true;
}


bool KIPLATFORM::APP::SupportsShutdownBlockReason()
{
    return false;
}


void KIPLATFORM::APP::RemoveShutdownBlockReason( wxWindow* aWindow [[maybe_unused]] )
{
}


void KIPLATFORM::APP::SetShutdownBlockReason( wxWindow* aWindow [[maybe_unused]], const wxString& aReason [[maybe_unused]] )
{
}


void KIPLATFORM::APP::ForceTimerMessagesToBeCreatedIfNecessary()
{
}


void KIPLATFORM::APP::AddDynamicLibrarySearchPath( const wxString& aPath [[maybe_unused]] )
{
}


bool KIPLATFORM::APP::SetCurrentThreadPriority( bool aRaise )
{
    sched_param param{};
    int         policy = 0;

    if( pthread_getschedparam( pthread_self(), &policy, &param ) != 0 )
        return false;

    param.sched_priority = aRaise ? sched_get_priority_max( policy ) : sched_get_priority_min( policy );

    return pthread_setschedparam( pthread_self(), policy, &param ) == 0;
}


void KIPLATFORM::APP::TerminateWithCode( int aExitCode )
{
    _exit( aExitCode );
}

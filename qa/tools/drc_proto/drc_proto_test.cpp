/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
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

#include <cstdio>
#include <cstring>
#include <string>

#include <common.h>
#include <core/profile.h>

#include <wx/cmdline.h>
#include <wx/init.h>
#include <wx/log.h>

#include <properties/property_mgr.h>
#include <properties/property.h>

#include <pgm_base.h>
#include <settings/settings_manager.h>

#include "drc_proto.h"

// wxWidgets' default assert handler opens a modal dialog, which blocks for ever when nobody is there to dismiss it
// (a headless run, a script). Report the failure on stderr and carry on instead.
static void reportAssertOnStderr( const wxString& aFile, int aLine, const wxString& aFunc, const wxString& aCond,
                                  const wxString& aMsg )
{
    fprintf( stderr, "wx assert failed at %s:%d in %s(): '%s' %s\n", (const char*) aFile.ToUTF8(), aLine,
             (const char*) aFunc.ToUTF8(), (const char*) aCond.ToUTF8(), (const char*) aMsg.ToUTF8() );
}


int main( int argc, char** argv )
{
    const char* usage = "usage: %s <project-file/board-file> [drc-rules-file]\n";

    // Answer the help flags before any wx or program setup, so they never reach the board loader.
    if( argc >= 2 && ( !strcmp( argv[1], "--help" ) || !strcmp( argv[1], "-h" ) ) )
    {
        printf( usage, argv[0] );
        return 0;
    }

    if( argc < 2 )
    {
        fprintf( stderr, usage, argv[0] );
        return 2;
    }

    wxSetAssertHandler( reportAssertOnStderr );

    wxInitialize( argc, argv );

    // A command-line tool must never wait on a modal dialog: wxLogGui queues messages and shows them in a message
    // box when the program exits, which blocks for ever without a user.
    delete wxLog::SetActiveTarget( new wxLogStderr );

    Pgm().InitPgm( true );

    PROPERTY_MANAGER& propMgr = PROPERTY_MANAGER::Instance();
    propMgr.Rebuild();

    PROJECT_CONTEXT project = loadKicadProject( argv[1], argv[2] ? wxString( argv[2] ) : std::optional<wxString>() );

    // The project's settings must be unloaded here, while the DRC item tables they read when saving still exist. Left
    // to the program object's static destructor they run after those tables are gone, and the exit crashes.
    auto unloadProject = [&]()
    {
        project.board.reset();
        Pgm().GetSettingsManager().UnloadProject( project.project, false );
    };

    if( !project.board )
    {
        fprintf( stderr, "%s: cannot load a board from '%s' (missing or unreadable)\n", argv[0], argv[1] );
        unloadProject();
        Pgm().Destroy();
        wxUninitialize();
        return 1;
    }

    // This causes some glib warnings on GTK3 (http://trac.wxwidgets.org/ticket/18274)
    // but without it, Valgrind notices a lot of leaks from WX

    runDRCProto( project );

    unloadProject();

    Pgm().Destroy();

    wxUninitialize();

    return 0;
}
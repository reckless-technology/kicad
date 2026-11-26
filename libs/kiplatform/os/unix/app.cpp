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

#ifdef __linux__
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include <glib.h>

#ifdef KICAD_HOUSE_THEME
#include <gtk/gtk.h>

#include <cstdio>
#include <string>

#include <kiplatform/reckless_palette.h>
#endif

#include <wx/string.h>
#include <wx/utils.h>


/*
 * Function to attach to the glib logger to eat the output it gives so we don't
 * get the message spam on the terminal from wxWidget's abuse of the GTK API.
 */
static GLogWriterOutput nullLogWriter( GLogLevelFlags   log_level [[maybe_unused]],
                                       const GLogField* fields [[maybe_unused]], gsize n_fields [[maybe_unused]],
                                       gpointer user_data [[maybe_unused]] )
{
    return G_LOG_WRITER_HANDLED;
}


bool KIPLATFORM::APP::Init()
{
    // Set KICAD_SHOW_GTK_MESSAGES=1 env var to show GTK messages,
    // otherwise, we hide them to avoid message spam.

    wxString showMessages;
    wxGetEnv( wxT( "KICAD_SHOW_GTK_MESSAGES" ), &showMessages );

    if( showMessages.IsEmpty() || showMessages != "1" )
    {
        // Attach a logger that will consume the annoying GTK error messages
        g_log_set_writer_func( nullLogWriter, nullptr, nullptr );
    }

    return true;
}


#ifdef KICAD_HOUSE_THEME
namespace
{
/**
 * The Reckless window colors as GTK CSS, with {NAME} standing for a RECKLESS_PALETTE color.
 *
 * Explicit rules on the widget nodes, because @define-color names change nothing under Adwaita 3.24, which compiles
 * its rules with literal colors. wx answers wxSystemSettings::GetColour() from GTK's style context, so these rules
 * also color the controls wx draws itself (grids, trees, AUI toolbars and panes, the status bar).
 *
 * The rules are written over GTK's dark variant and leak light borders over the light one, so they are only added
 * on top of it.
 */
const char* const RECKLESS_GTK_CSS = R"CSS(
window, dialog, .background, messagedialog, notebook, notebook > stack, paned, scrolledwindow viewport, .titlebar,
headerbar
  { background-color: {WINDOW}; background-image: none; color: {INK}; }
label { color: {INK}; }
label:disabled, *:disabled { color: {DISABLED_TEXT}; }
entry, spinbutton, spinbutton text, textview text, .view, treeview.view, iconview, combobox button, .cell
  { background-color: {PANEL}; background-image: none; color: {INK}; border-color: {RULE}; }
entry:disabled, spinbutton:disabled { background-color: {WINDOW}; color: {DISABLED_TEXT}; }
button, toolbar button
  { background-image: none; background-color: {PLATE}; color: {INK}; border-color: {RULE}; }
button:hover { background-color: {RULE}; }
button:disabled { color: {DISABLED_TEXT}; }
button.default, button.suggested-action { background-color: {AMBER_DIM}; border-color: {AMBER}; }
*:focus { outline-color: {AMBER}; }
entry:focus, spinbutton:focus { border-color: {AMBER}; }
*:selected, selection, text selection, treeview.view:selected, row:selected, .view:selected
  { background-color: {SELECT}; color: {INK}; }
check, radio
  { background-image: none; background-color: {PANEL}; border-color: {AMBER_DIM}; color: {INK}; }
check:checked, radio:checked { background-color: {AMBER}; color: {PANEL}; border-color: {AMBER}; }
scale trough { background-color: {RULE}; }
scale highlight, progressbar progress { background-color: {AMBER}; background-image: none; }
scale slider { background-color: {INK}; background-image: none; }
notebook > header { background-color: {PANEL}; border-color: {RULE}; }
notebook > header > tabs > tab { background-color: {WINDOW}; color: {MUTED}; }
notebook > header > tabs > tab:checked
  { background-color: {WINDOW}; color: {INK}; box-shadow: inset 0 -2px {AMBER}; }
menubar, menu, .menu, menuitem { background-color: {WINDOW}; color: {INK}; border-color: {RULE}; }
menuitem:hover { background-color: {SELECT}; }
menuitem:disabled label { color: {DISABLED_TEXT}; }
tooltip, tooltip.background { background-color: {RULE}; color: {INK}; border: 1px solid {AMBER_DIM}; }
scrollbar { background-color: {PANEL}; }
scrollbar slider { background-color: {AMBER_DIM}; }
scrollbar slider:hover { background-color: {AMBER}; }
separator { background-color: {RULE}; }
frame > border, frame { border-color: {AMBER_DIM}; }
infobar, .info { background-color: {RULE}; }
)CSS";


std::string hexColor( unsigned aRgb )
{
    char text[8];
    std::snprintf( text, sizeof( text ), "#%06x", aRgb & 0xffffffu );
    return text;
}


/// The CSS with every {NAME} replaced by its palette color, so the palette header is the one place a color lives.
std::string recklessCss()
{
    namespace P = KIPLATFORM::RECKLESS_PALETTE;

    struct ENTRY
    {
        const char* name;
        unsigned    rgb;
    };

    static const ENTRY colors[] = {
        { "{WINDOW}", P::WINDOW },
        { "{PANEL}", P::PANEL },
        { "{PLATE}", P::PLATE },
        { "{RULE}", P::RULE },
        { "{INK}", P::INK },
        { "{MUTED}", P::MUTED },
        { "{DISABLED_TEXT}", P::DISABLED_TEXT },
        { "{AMBER}", P::AMBER },
        { "{AMBER_DIM}", P::AMBER_DIM },
        { "{SELECT}", P::SELECT },
    };

    std::string css = RECKLESS_GTK_CSS;

    for( const ENTRY& entry : colors )
    {
        const std::string name = entry.name;
        const std::string color = hexColor( entry.rgb );

        for( size_t at = css.find( name ); at != std::string::npos; at = css.find( name, at + color.size() ) )
            css.replace( at, name.size(), color );
    }

    return css;
}

} // namespace
#endif


void KIPLATFORM::APP::EnableDarkMode( bool aForce [[maybe_unused]] )
{
#ifdef KICAD_HOUSE_THEME
    // Automatic and Light follow the desktop's own theme, as upstream does.
    if( !aForce )
        return;

    static bool isApplied = false;

    GdkScreen*   screen = gdk_screen_get_default();
    GtkSettings* settings = gtk_settings_get_default();

    // No display (a command-line run) means no window to color.
    if( isApplied || !screen || !settings )
        return;

    isApplied = true;

    // The rules are written over Adwaita's dark variant, and GTK carries that variant inside the library: it needs no
    // theme package, and it comes from the prefer-dark switch even where the desktop's own theme is missing or has no
    // dark variant. (On a Mint desktop the switch alone gives that theme's dark variant, and in a session whose
    // named theme is not installed it changes nothing, so the theme is named as well.) A GTK_THEME set in the
    // environment is the user's own choice and still overrides both.
    g_object_set( settings, "gtk-theme-name", "Adwaita", "gtk-application-prefer-dark-theme", TRUE, nullptr );

    // APPLICATION priority beats the theme and still lets a user's own ~/.config/gtk-3.0/gtk.css (USER priority) win.
    GtkCssProvider*   provider = gtk_css_provider_new();
    const std::string css = recklessCss();

    if( gtk_css_provider_load_from_data( provider, css.c_str(), static_cast<gssize>( css.size() ), nullptr ) )
    {
        gtk_style_context_add_provider_for_screen( screen, GTK_STYLE_PROVIDER( provider ),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
    }

    g_object_unref( provider );

    // wx caches the system colors the first time it is asked, so a color read before this point stays stale. A
    // theme-name notification makes it drop the cache and ask GTK again.
    g_object_notify( G_OBJECT( settings ), "gtk-theme-name" );
#endif
}


bool KIPLATFORM::APP::GetCaptionColors( unsigned& aCaption [[maybe_unused]], unsigned& aText [[maybe_unused]],
                                        unsigned& aBorder [[maybe_unused]] )
{
    // The title bar is the window manager's
    return false;
}


void KIPLATFORM::APP::SetCaptionColors( wxWindow* aWindow [[maybe_unused]] )
{
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


void KIPLATFORM::APP::SetShutdownBlockReason( wxWindow*       aWindow [[maybe_unused]],
                                              const wxString& aReason [[maybe_unused]] )
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
#ifdef __linux__
    // Each Linux thread has its own nice value, and a negative one needs privilege
    const pid_t tid = static_cast<pid_t>( syscall( SYS_gettid ) );

    return setpriority( PRIO_PROCESS, tid, aRaise ? -5 : 5 ) == 0;
#else
    return false;
#endif
}


void KIPLATFORM::APP::TerminateWithCode( int aExitCode )
{
    _exit( aExitCode );
}

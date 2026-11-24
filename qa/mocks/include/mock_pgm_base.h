/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see CHANGELOG.TXT for contributors.
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

#pragma once

#include <qa_utils/mock_utils.h>

#include <pgm_base.h>
#include <config.h>

MOCK_BASE_CLASS( MOCK_PGM_BASE, PGM_BASE )
{
    MOCK_PGM_BASE() : PGM_BASE() {};
    virtual ~MOCK_PGM_BASE() {};

    KI_MOCK_METHOD_OVERRIDE( MacOpenFile, 1, void( const wxString& ) );
    KI_MOCK_METHOD_OVERRIDE( SetTextEditor, 1, void( const wxString& ) );
    KI_MOCK_METHOD_OVERRIDE( GetTextEditor, 1, wxString&( bool ) );
    KI_MOCK_METHOD_OVERRIDE( AskUserForPreferredEditor, 1, const wxString( const wxString& ) );
    KI_MOCK_CONST_METHOD_OVERRIDE( GetPdfBrowserName, 0, const wxString&() );
    KI_MOCK_METHOD_OVERRIDE( SetPdfBrowserName, 1, void( const wxString& ) );
    KI_MOCK_CONST_METHOD_OVERRIDE( UseSystemPdfBrowser, 0, bool() );
    KI_MOCK_METHOD_OVERRIDE( ForceSystemPdfBrowser, 1, void( bool ) );
    KI_MOCK_METHOD_OVERRIDE( SetLanguage, 2, bool( wxString&, bool ) );
    KI_MOCK_METHOD_OVERRIDE( SetLanguageIdentifier, 1, void( int ) );
    KI_MOCK_METHOD_OVERRIDE( ReadPdfBrowserInfos, 0, void() );
    KI_MOCK_METHOD_OVERRIDE( WritePdfBrowserInfos, 0, void() );
    KI_MOCK_METHOD_OVERRIDE( SetLocalEnvVariable, 2, bool( const wxString&, const wxString& ) );
    KI_MOCK_METHOD_OVERRIDE( SetLocalEnvVariables, 0, void() );

    int GetSelectedLanguageIdentifier() const override
    {
        return 0;
    }

    // following functions will not be mocked in order to mimic old qa behavior
//    MOCK_METHOD( App, 0, wxApp&() );
//    MOCK_METHOD( GetLocale, 0, wxLocale*() );
//    MOCK_METHOD( GetSettingsManager, 0, SETTINGS_MANAGER&() );
//    MOCK_METHOD( SetLanguagePath, 0, void() );
//    MOCK_CONST_METHOD( GetExecutablePath, 0, const wxString&() );
//    MOCK_METHOD( GetSettingsManager, 0, SETTINGS_MANAGER&() );
};

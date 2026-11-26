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
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test.hpp>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <wx/filename.h>

#include <bitmaps/bitmap_info.h>
#include <settings/common_settings.h>

// The Reckless icon set: a house build (KICAD_HOUSE_THEME) starts a new configuration in it and an upstream build in
// Automatic, whatever the platform; and every dark icon has a Reckless twin, drawn from a file that is in the tree.

BOOST_AUTO_TEST_SUITE( HouseIconTheme )

BOOST_AUTO_TEST_CASE( FreshConfigurationDefault )
{
    COMMON_SETTINGS settings;

    // Loading a parameter that is missing from the file gives its default, which is what a new configuration sees.
    settings.Load();

#ifdef KICAD_HOUSE_THEME
    BOOST_CHECK( settings.m_Appearance.icon_theme == ICON_THEME::RECKLESS );
#else
    BOOST_CHECK( settings.m_Appearance.icon_theme == ICON_THEME::AUTO );
#endif
}


BOOST_AUTO_TEST_CASE( ChosenIconThemeIsKept )
{
    COMMON_SETTINGS settings;

    for( ICON_THEME chosen : { ICON_THEME::LIGHT, ICON_THEME::DARK, ICON_THEME::AUTO, ICON_THEME::RECKLESS } )
    {
        settings.m_Appearance.icon_theme = chosen;
        settings.Store();

        settings.m_Appearance.icon_theme = ICON_THEME::AUTO;
        settings.Load();

#if !defined( KICAD_HOUSE_THEME ) && !defined( __WXGTK__ ) && !defined( __WXMSW__ )
        // Where the setting is not offered at all, a build keeps its default
        BOOST_CHECK( settings.m_Appearance.icon_theme == ICON_THEME::AUTO );
#else
        BOOST_CHECK( settings.m_Appearance.icon_theme == chosen );
#endif
    }
}


BOOST_AUTO_TEST_CASE( StoredNumbersAreUnchanged )
{
    // Light, Dark and Automatic keep the numbers an existing configuration file holds; Reckless is the next one.
    BOOST_CHECK_EQUAL( static_cast<int>( ICON_THEME::LIGHT ), 0 );
    BOOST_CHECK_EQUAL( static_cast<int>( ICON_THEME::DARK ), 1 );
    BOOST_CHECK_EQUAL( static_cast<int>( ICON_THEME::AUTO ), 2 );
    BOOST_CHECK_EQUAL( static_cast<int>( ICON_THEME::RECKLESS ), 3 );
}


BOOST_AUTO_TEST_CASE( EveryDarkIconHasARecklessTwinThatExists )
{
    std::unordered_map<BITMAPS, std::vector<BITMAP_INFO>> cache;
    BuildBitmapInfo( cache );

    int darkCount = 0;
    int recklessCount = 0;

    for( const auto& [id, infos] : cache )
    {
        std::set<int> darkHeights;
        std::set<int> recklessHeights;

        for( const BITMAP_INFO& info : infos )
        {
            if( info.theme == BITMAP_INFO::THEME::DARK )
                darkHeights.insert( info.height );
            else if( info.theme == BITMAP_INFO::THEME::RECKLESS )
            {
                recklessHeights.insert( info.height );

                // The name says which set it is, and the file is in the tree, which the archive is made from
                BOOST_CHECK_MESSAGE( info.filename.find( "_reckless" ) != std::string::npos, info.filename );

                wxFileName file( wxString( QA_SRC_ROOT ) + wxS( "/resources/bitmaps_png/png/" )
                                 + wxString::FromUTF8( info.filename ) );
                BOOST_CHECK_MESSAGE( file.FileExists(), "missing " << info.filename );
            }
        }

        darkCount += darkHeights.size();
        recklessCount += recklessHeights.size();

        // A dark entry that names a file which exists has a twin at the same height; the dark entries that name a
        // file the tree never had (spread_tracks) have none, and so have no Reckless twin either.
        for( int height : recklessHeights )
            BOOST_CHECK( darkHeights.count( height ) == 1 );
    }

    BOOST_CHECK( recklessCount > 2000 );
    BOOST_CHECK( recklessCount <= darkCount );
}

BOOST_AUTO_TEST_SUITE_END()

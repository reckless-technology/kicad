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

#include <settings/common_settings.h>

// A fresh configuration starts in the Dark app theme in a house build (KICAD_HOUSE_THEME) and in Automatic in an
// upstream build, whatever the platform.

BOOST_AUTO_TEST_SUITE( HouseAppTheme )

BOOST_AUTO_TEST_CASE( FreshConfigurationDefault )
{
    COMMON_SETTINGS settings;

    // Loading a parameter that is missing from the file gives its default, which is what a new configuration sees.
    settings.Load();

#ifdef KICAD_HOUSE_THEME
    BOOST_CHECK( settings.m_Appearance.app_theme == APP_THEME::DARK );
#else
    BOOST_CHECK( settings.m_Appearance.app_theme == APP_THEME::AUTO );
#endif
}

#if defined( __WXMSW__ ) || ( defined( __WXGTK__ ) && defined( KICAD_HOUSE_THEME ) )

BOOST_AUTO_TEST_CASE( ChosenThemeIsKept )
{
    // The setting is stored, so a configuration that chose Light keeps it over the house default.
    COMMON_SETTINGS settings;
    settings.m_Appearance.app_theme = APP_THEME::LIGHT;
    settings.Store();

    settings.m_Appearance.app_theme = APP_THEME::DARK;
    settings.Load();

    BOOST_CHECK( settings.m_Appearance.app_theme == APP_THEME::LIGHT );
}

#endif

BOOST_AUTO_TEST_SUITE_END()

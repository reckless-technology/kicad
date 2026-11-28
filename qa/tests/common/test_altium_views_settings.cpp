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

/**
 * @file test_altium_views_settings.cpp
 * The settings side of the Altium-style profile's views (design note, 5.7): the mask level steps and stops at both
 * ends, the Altium classic color theme is built in and carries its colors, it becomes an editor's theme once and never
 * again, and a new project's opacities follow the profile.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <interaction_profile.h>
#include <layer_ids.h>
#include <settings/app_settings.h>
#include <settings/color_settings.h>
#include <settings/common_settings.h>

#include <memory>
#include <vector>


namespace
{

/// Owns the built-in themes, and finds one by its key.
struct BUILTIN_THEMES
{
    BUILTIN_THEMES()
    {
        for( COLOR_SETTINGS* theme : COLOR_SETTINGS::CreateBuiltinColorSettings() )
            themes.emplace_back( theme );
    }

    COLOR_SETTINGS* Find( const wxString& aFilename ) const
    {
        for( const std::unique_ptr<COLOR_SETTINGS>& theme : themes )
        {
            if( theme->GetFilename() == aFilename )
                return theme.get();
        }

        return nullptr;
    }

    std::vector<std::unique_ptr<COLOR_SETTINGS>> themes;
};


KIGFX::COLOR4D rgb( int aRed, int aGreen, int aBlue )
{
    return KIGFX::COLOR4D( aRed / 255.0, aGreen / 255.0, aBlue / 255.0, 1.0 );
}

} // namespace


BOOST_AUTO_TEST_SUITE( AltiumViewsSettings )


BOOST_AUTO_TEST_CASE( TheMaskLevelStepsByTenthsAndStopsAtBothEnds )
{
    BOOST_CHECK_CLOSE( INTERACTION_PROFILE::SteppedDimmingFactor( 0.8, 1 ), 0.9, 1e-9 );
    BOOST_CHECK_CLOSE( INTERACTION_PROFILE::SteppedDimmingFactor( 0.8, -1 ), 0.7, 1e-9 );

    // The ends hold
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::SteppedDimmingFactor( 1.0, 1 ), 1.0 );
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::SteppedDimmingFactor( 0.95, 1 ), 1.0 );
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::SteppedDimmingFactor( 0.0, -1 ), 0.0 );
    BOOST_CHECK_EQUAL( INTERACTION_PROFILE::SteppedDimmingFactor( 0.04, -1 ), 0.0 );

    // A stored value that is off the tenths is brought onto them, and a long run of presses never drifts off them
    BOOST_CHECK_CLOSE( INTERACTION_PROFILE::SteppedDimmingFactor( 0.83, 1 ), 0.9, 1e-9 );

    double factor = 0.0;

    for( int press = 0; press < 3; ++press )
        factor = INTERACTION_PROFILE::SteppedDimmingFactor( factor, 1 );

    BOOST_CHECK_CLOSE( factor, 0.3, 1e-9 );

    for( int press = 0; press < 25; ++press )
        factor = INTERACTION_PROFILE::SteppedDimmingFactor( factor, 1 );

    BOOST_CHECK_EQUAL( factor, 1.0 );

    for( int press = 0; press < 25; ++press )
        factor = INTERACTION_PROFILE::SteppedDimmingFactor( factor, -1 );

    BOOST_CHECK_EQUAL( factor, 0.0 );
}


BOOST_AUTO_TEST_CASE( TheAltiumClassicThemeIsBuiltInAndReadOnlyBesideTheOthers )
{
    BUILTIN_THEMES  builtins;
    COLOR_SETTINGS* classic = builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_ALTIUM_CLASSIC );

    BOOST_REQUIRE( classic );
    BOOST_CHECK_EQUAL( classic->GetName(), wxString( wxS( "Altium classic" ) ) );
    BOOST_CHECK( classic->IsReadOnly() );

    BOOST_CHECK( builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT ) );
    BOOST_CHECK( builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_CLASSIC ) );
    BOOST_CHECK( builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS ) );
}


BOOST_AUTO_TEST_CASE( TheAltiumClassicThemeCarriesItsColorsOnKeyLayers )
{
    BUILTIN_THEMES  builtins;
    COLOR_SETTINGS* classic = builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_ALTIUM_CLASSIC );

    BOOST_REQUIRE( classic );

    // The PCB: a black canvas, the top layer red, the bottom layer blue, the overlay yellow, multi-layer gray
    BOOST_CHECK( classic->GetColor( LAYER_PCB_BACKGROUND ) == rgb( 0, 0, 0 ) );
    BOOST_CHECK( classic->GetColor( F_Cu ) == rgb( 255, 0, 0 ) );
    BOOST_CHECK( classic->GetColor( B_Cu ) == rgb( 51, 102, 255 ) );
    BOOST_CHECK( classic->GetColor( F_SilkS ) == rgb( 255, 255, 0 ) );
    BOOST_CHECK( classic->GetColor( LAYER_VIA_HOLES ) == rgb( 133, 133, 133 ) );

    // The schematic: a white sheet, blue wires and parts, dark-red labels
    BOOST_CHECK( classic->GetColor( LAYER_SCHEMATIC_BACKGROUND ) == rgb( 255, 255, 255 ) );
    BOOST_CHECK( classic->GetColor( LAYER_WIRE ) == rgb( 0, 0, 128 ) );
    BOOST_CHECK( classic->GetColor( LAYER_DEVICE ) == rgb( 0, 0, 255 ) );
    BOOST_CHECK( classic->GetColor( LAYER_LOCLABEL ) == rgb( 128, 0, 0 ) );
    BOOST_CHECK( classic->GetColor( LAYER_GLOBLABEL ) == rgb( 128, 0, 0 ) );

    // Neither of the two other dark themes
    BOOST_CHECK( classic->GetColor( F_Cu )
                 != builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS )->GetColor( F_Cu ) );
    BOOST_CHECK( classic->GetColor( F_Cu )
                 != builtins.Find( COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT )->GetColor( F_Cu ) );
}


BOOST_AUTO_TEST_CASE( TheProfileStartsAnEditorInTheThemeOnceAndNeverAgain )
{
    APP_SETTINGS_BASE settings( "altium_theme_apply_once_test", 0 );
    settings.m_ColorTheme = COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT;
    settings.m_AltiumColorThemeApplied = false;

    // The profile is the one of the common settings of the test program
    const INTERACTION_PROFILE_ID previous = INTERACTION_PROFILE::Active();

    // Stock: nothing, and the owed theme is not used up
    INTERACTION_PROFILE::SetActive( INTERACTION_PROFILE_ID::STOCK );
    BOOST_CHECK( !INTERACTION_PROFILE::ApplyColorThemeOnce( settings ) );
    BOOST_CHECK_EQUAL( settings.m_ColorTheme, COLOR_SETTINGS::COLOR_BUILTIN_DEFAULT );
    BOOST_CHECK( !settings.m_AltiumColorThemeApplied );

    // Altium-style: the theme, once
    INTERACTION_PROFILE::SetActive( INTERACTION_PROFILE_ID::ALTIUM );
    BOOST_CHECK( INTERACTION_PROFILE::ApplyColorThemeOnce( settings ) );
    BOOST_CHECK_EQUAL( settings.m_ColorTheme, COLOR_SETTINGS::COLOR_BUILTIN_ALTIUM_CLASSIC );
    BOOST_CHECK( settings.m_AltiumColorThemeApplied );

    // The user picks another theme: it stays, however often the profile is chosen again
    settings.m_ColorTheme = COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS;
    BOOST_CHECK( !INTERACTION_PROFILE::ApplyColorThemeOnce( settings ) );
    BOOST_CHECK_EQUAL( settings.m_ColorTheme, COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS );

    INTERACTION_PROFILE::SetActive( INTERACTION_PROFILE_ID::STOCK );
    INTERACTION_PROFILE::SetActive( INTERACTION_PROFILE_ID::ALTIUM );
    BOOST_CHECK( !INTERACTION_PROFILE::ApplyColorThemeOnce( settings ) );
    BOOST_CHECK_EQUAL( settings.m_ColorTheme, COLOR_SETTINGS::COLOR_BUILTIN_RECKLESS );

    // ... and across a restart
    settings.Store();
    settings.m_AltiumColorThemeApplied = false;
    settings.Load();
    BOOST_CHECK( settings.m_AltiumColorThemeApplied );

    INTERACTION_PROFILE::SetActive( previous );
}


BOOST_AUTO_TEST_CASE( TheOpacityLookIsOnlyTheProfilesUntilTheSettingSaysOtherwise )
{
    COMMON_SETTINGS settings;
    settings.ResetToDefaults();

    // The default follows the profile
    BOOST_CHECK_EQUAL( settings.m_BoardOpacityProfileLook, -1 );

    settings.m_BoardOpacityDefaults.m_PadOpacity = 0.4;

    settings.m_Input.interaction_profile = INTERACTION_PROFILE_ID::STOCK;
    BOOST_CHECK( !settings.DoesProjectOpenTransparent() );
    BOOST_CHECK_EQUAL( settings.GetNewProjectOpacities().m_PadOpacity, 0.4 );

    settings.m_Input.interaction_profile = INTERACTION_PROFILE_ID::ALTIUM;
    BOOST_CHECK( settings.DoesProjectOpenTransparent() );

    BOARD_OPACITY_DEFAULTS look = settings.GetNewProjectOpacities();

    BOOST_CHECK_EQUAL( look.m_PadOpacity, 0.80 );
    BOOST_CHECK_EQUAL( look.m_ViaOpacity, 1.00 );
    BOOST_CHECK_EQUAL( look.m_TrackOpacity, 0.85 );
    BOOST_CHECK_EQUAL( look.m_ZoneOpacity, 0.30 );
    BOOST_CHECK_EQUAL( look.m_ShapeOpacity, 0.60 );

    // Off, whatever the profile; on, whatever the profile
    settings.m_BoardOpacityProfileLook = 0;
    BOOST_CHECK( !settings.DoesProjectOpenTransparent() );
    BOOST_CHECK_EQUAL( settings.GetNewProjectOpacities().m_PadOpacity, 0.4 );

    settings.m_BoardOpacityProfileLook = 1;
    settings.m_Input.interaction_profile = INTERACTION_PROFILE_ID::STOCK;
    BOOST_CHECK( settings.DoesProjectOpenTransparent() );
}


BOOST_AUTO_TEST_SUITE_END()

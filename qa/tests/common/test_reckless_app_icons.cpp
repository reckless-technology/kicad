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
 * @file test_reckless_app_icons.cpp
 * The house build's window icons: which image ids stand for a program's own icon and at what height, and that the
 * toolbar, menu, launcher and nightly images are left alone. The icons' files are checked against their script by
 * the reckless_app_icons_are_current test.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <bitmap_store.h>
#include <bitmaps.h>

#ifdef KICAD_HOUSE_ICONS

BOOST_AUTO_TEST_SUITE( RecklessAppIcons )


BOOST_AUTO_TEST_CASE( SmallIdsKeepTheirHeight )
{
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_eeschema_16, -1 ), 16 );
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_pcbnew_32, -1 ), 32 );
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_modedit_16, -1 ), 16 );
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_pagelayout_editor_32, -1 ), 32 );
}


BOOST_AUTO_TEST_CASE( BaseIdsUseTheHeightAskedFor )
{
    for( int height : { 48, 128, 256 } )
        BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_gerbview, height ), height );

    // No height tag, or one with no house size, is the 48 pixel icon
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_kicad, -1 ), 48 );
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_cvpcb, 77 ), 48 );
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_3d, -1 ), 48 );
}


BOOST_AUTO_TEST_CASE( OtherImagesAreLeftAlone )
{
    // The menu and tree icons, the nightly icon, the plugin manager's glyph and a toolbar image
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_eeschema_24, 24 ), 0 );
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_kicad_nightly, 48 ), 0 );
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::icon_pcm, 48 ), 0 );
    BOOST_CHECK_EQUAL( BITMAP_STORE::HouseAppIconHeight( BITMAPS::zoom_in, 24 ), 0 );
}


BOOST_AUTO_TEST_SUITE_END()

#endif

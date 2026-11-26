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

/**
 * @file
 * The built-in bitmap font compiled into the GAL: Ubuntu Mono in an upstream build, Monaspace Xenon in the house
 * build (KICAD_HOUSE_FONTS), over the same codepoints.
 */

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <gal/opengl/gl_resources.h>
#include <rhi/rhi_glyph_atlas.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#ifdef KICAD_HOUSE_FONTS
#include <font/fontconfig.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#endif

using namespace KIGFX::BUILTIN_FONT;


namespace UPSTREAM_ATLAS
{
using KIGFX::BUILTIN_FONT::FONT_GLYPH_TYPE;
using KIGFX::BUILTIN_FONT::FONT_INFO_TYPE;
using KIGFX::BUILTIN_FONT::FONT_SPAN_TYPE;

// The upstream atlas's description, whatever the build compiles in, for its codepoint set
#include <gal/opengl/bitmap_font_desc.c>
} // namespace UPSTREAM_ATLAS


namespace
{
std::set<unsigned int> upstreamCodepoints()
{
    std::set<unsigned int> codepoints;

    for( const FONT_SPAN_TYPE& span : UPSTREAM_ATLAS::font_codepoint_spans )
    {
        for( unsigned int cp = span.start; cp < span.end; ++cp )
            codepoints.insert( cp );
    }

    return codepoints;
}


#ifdef KICAD_HOUSE_FONTS
// The decoded distance at an atlas texel: the median of its three channels, as the shaders take it
int atlasDistance( unsigned int aX, unsigned int aY )
{
    const unsigned char* texel = font_image.pixels + ( size_t( aY ) * font_image.width + aX ) * 3;
    const int            r = texel[0], g = texel[1], b = texel[2];

    return std::max( std::min( r, g ), std::min( std::max( r, g ), b ) );
}
#endif
} // namespace


BOOST_AUTO_TEST_SUITE( BuiltinFontAtlas )


BOOST_AUTO_TEST_CASE( AtlasCoversTheUpstreamCodepoints )
{
    const std::set<unsigned int> codepoints = upstreamCodepoints();

    BOOST_CHECK_EQUAL( GlyphCount(), codepoints.size() );
    BOOST_CHECK_EQUAL( GlyphCount(), std::size( UPSTREAM_ATLAS::font_codepoint_infos ) );

    // The Vulkan canvas's glyph table holds every glyph
    BOOST_CHECK_LE( GlyphCount(), RHI_GLYPH_TABLE_SIZE );

    int missing = 0;

    for( unsigned int cp : codepoints )
    {
        if( !LookupGlyph( cp ) )
            missing++;
    }

    BOOST_CHECK_EQUAL( missing, 0 );

    // And nothing beyond them
    BOOST_CHECK( LookupGlyph( ' ' ) == nullptr );
    BOOST_CHECK( LookupGlyph( 0x4E00 ) == nullptr );
}


BOOST_AUTO_TEST_CASE( GlyphsFitInTheAtlas )
{
    const FONT_GLYPH_TYPE* first = LookupGlyph( '!' );
    BOOST_REQUIRE( first );

    int outside = 0;

    for( unsigned int ii = 0; ii < GlyphCount(); ++ii )
    {
        const FONT_GLYPH_TYPE& glyph = first[ii];

        // Every cell holds at least its one-texel border on each side
        if( glyph.atlas_x + glyph.atlas_w > font_image.width || glyph.atlas_y + glyph.atlas_h > font_image.height
            || glyph.atlas_w < 2 || glyph.atlas_h < 2 )
        {
            outside++;
        }
    }

    BOOST_CHECK_EQUAL( outside, 0 );
}


#ifdef KICAD_HOUSE_FONTS

BOOST_AUTO_TEST_CASE( HouseAtlasIsMonospacedAtXenonsAdvance )
{
    // Monaspace Xenon advances 0.62 em, and the atlas has 48 texels per em. Glyphs Xenon lacks keep Ubuntu Mono's
    // shapes, centered in the same advance, so the painters' character-count sizing holds. The three pictographs the
    // upstream atlas already draws wider than its own advance stay wider.
    const std::set<unsigned int> wide = { 0x2316, 0x2705, 0x274E };

    for( const FONT_SPAN_TYPE& span : UPSTREAM_ATLAS::font_codepoint_spans )
    {
        for( unsigned int cp = span.start; cp < span.end; ++cp )
        {
            const FONT_GLYPH_TYPE* glyph = LookupGlyph( cp );
            BOOST_REQUIRE( glyph );

            if( wide.count( cp ) )
                BOOST_CHECK_GT( glyph->advance, 29.76f );
            else if( std::abs( glyph->advance - 29.76f ) > 0.001f )
                BOOST_ERROR( "U+" << std::hex << cp << " advances " << glyph->advance );
        }
    }
}


BOOST_AUTO_TEST_CASE( HouseAtlasDrawsXenonsShapes )
{
    // The atlas's inside, decoded as the shaders decode it, against Xenon Regular rasterized by FreeType at the
    // atlas's 48 pixels per em
    wxString fontFile;
    int      faceIndex = 0;

    if( Fontconfig()->FindFont( wxS( "Monaspace Xenon" ), fontFile, faceIndex, false, false )
        != fontconfig::FONTCONFIG::FF_RESULT::FF_OK )
    {
        BOOST_TEST_MESSAGE( "Monaspace Xenon is not installed; skipped" );
        return;
    }

    FT_Library library = nullptr;
    FT_Face    face = nullptr;
    BOOST_REQUIRE( FT_Init_FreeType( &library ) == 0 );
    BOOST_REQUIRE( FT_New_Face( library, fontFile.utf8_str(), faceIndex, &face ) == 0 );
    BOOST_REQUIRE( FT_Set_Pixel_Sizes( face, 0, 48 ) == 0 );

    for( char32_t ch : std::u32string( U"0OagM&@R1lI" ) )
    {
        BOOST_TEST_CONTEXT( "U+" << std::hex << unsigned( ch ) )
        {
            BOOST_REQUIRE( FT_Load_Char( face, ch, FT_LOAD_RENDER | FT_LOAD_NO_HINTING ) == 0 );
            const FT_GlyphSlot slot = face->glyph;

            const FONT_GLYPH_TYPE* glyph = LookupGlyph( ch );
            BOOST_REQUIRE( glyph );

            BOOST_CHECK_CLOSE( glyph->advance, slot->advance.x / 64.0, 0.5 );

            const GLYPH_BOX box = ComputeGlyphBox( *glyph );
            int             both = 0, either = 0;

            for( int jj = 0; jj < int( box.h ); ++jj )
            {
                for( int ii = 0; ii < int( box.w ); ++ii )
                {
                    const bool atlasInside =
                            atlasDistance( unsigned( box.atlasX ) + ii, unsigned( box.atlasY ) + jj ) > 127;

                    // The texel's center in font units, x right and y up from the pen position. Cells are stored
                    // upside down, so the cell's first row is the bottom of the glyph.
                    const double x = glyph->minx + ii + 0.5;
                    const double y = glyph->miny + jj + 0.5;
                    const int    column = int( std::floor( x - slot->bitmap_left ) );
                    const int    row = int( std::floor( slot->bitmap_top - y ) );
                    bool         fontInside = false;

                    if( column >= 0 && row >= 0 && column < int( slot->bitmap.width )
                        && row < int( slot->bitmap.rows ) )
                    {
                        fontInside = slot->bitmap.buffer[row * slot->bitmap.pitch + column] > 127;
                    }

                    both += atlasInside && fontInside;
                    either += atlasInside || fontInside;
                }
            }

            BOOST_REQUIRE_GT( either, 0 );
            // Xenon scores 0.9 to 1 here; Ubuntu Mono's glyphs score under 0.5
            BOOST_CHECK_GT( double( both ) / either, 0.8 );
        }
    }

    FT_Done_Face( face );
    FT_Done_FreeType( library );
}


BOOST_AUTO_TEST_CASE( HouseAtlasKeepsGlyphsXenonLacks )
{
    // U+0180 (Latin Extended-B) and U+1F00 (Greek Extended) are not in Monaspace Xenon 1.400
    for( unsigned int cp : { 0x0180u, 0x1F00u, 0xFFFDu } )
    {
        BOOST_TEST_CONTEXT( "U+" << std::hex << cp )
        {
            const FONT_GLYPH_TYPE* glyph = LookupGlyph( cp );
            BOOST_REQUIRE( glyph );

            int ink = 0;

            for( unsigned int jj = 0; jj < glyph->atlas_h; ++jj )
            {
                for( unsigned int ii = 0; ii < glyph->atlas_w; ++ii )
                    ink += atlasDistance( glyph->atlas_x + ii, glyph->atlas_y + jj ) > 127;
            }

            BOOST_CHECK_GT( ink, 10 );
        }
    }
}

#else

BOOST_AUTO_TEST_CASE( UpstreamAtlasIsUbuntuMono )
{
    // The upstream atlas, unchanged: 1024x1107, a 24-unit advance
    BOOST_CHECK_EQUAL( font_image.width, 1024u );
    BOOST_CHECK_EQUAL( font_image.height, 1107u );
    BOOST_REQUIRE( LookupGlyph( '0' ) );
    BOOST_CHECK_EQUAL( LookupGlyph( '0' )->advance, 24.0f );
}

#endif


BOOST_AUTO_TEST_SUITE_END()

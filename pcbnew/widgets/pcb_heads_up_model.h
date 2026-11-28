/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
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

#ifndef PCB_HEADS_UP_MODEL_H
#define PCB_HEADS_UP_MODEL_H

#include <optional>
#include <vector>

#include <wx/string.h>

#include <eda_units.h>
#include <gal/color4d.h>
#include <interaction_profile.h>
#include <layer_ids.h>
#include <math/vector2d.h>

class BASE_SCREEN;

namespace KIGFX
{
class VIEW;
}

namespace PNS
{
class ROUTING_SETTINGS;
class SIZES_SETTINGS;
} // namespace PNS

/**
 * The decisions behind the heads-up display that need no window (interaction-profile note, 5.7): what the display says
 * for a given state of the editor, whether it is shown, and which item is under the cursor. The window asks for the
 * lines on a change of state and keeps nothing of the answers.
 */

/// The values of the heads-up display setting: it follows the interaction profile, or it is off, or it is on.
enum HEADS_UP_SETTING
{
    HEADS_UP_FOLLOW_PROFILE = -1,
    HEADS_UP_OFF = 0,
    HEADS_UP_ON = 1
};

/**
 * Whether the display is shown. A setting that follows the profile is on in the Altium-style profile and off in stock
 * KiCad. A stored 0 or 1 wins over the profile, and a value that is none of the three counts as following it.
 */
bool IsHeadsUpShown( int aSetting, INTERACTION_PROFILE_ID aProfile );

/// The value to store when the display is toggled: the opposite of what is showing now, stored explicitly.
int ToggledHeadsUpSetting( int aSetting, INTERACTION_PROFILE_ID aProfile );

/// The corner styles of a route. Free angle is a mode of its own in the router, apart from the four corner modes.
enum class HEADS_UP_CORNER_MODE
{
    FREE_ANGLE,
    MITERED_45,
    ROUNDED_45,
    MITERED_90,
    ROUNDED_90
};

/// How the router treats what is in the way
enum class HEADS_UP_ROUTER_MODE
{
    WALK_AROUND,
    SHOVE,
    HIGHLIGHT_COLLISIONS
};

/// Where the track width being routed with came from
enum class HEADS_UP_WIDTH_SOURCE
{
    NET_CLASS, ///< the net class of the net, through the design rules
    USER,      ///< a width chosen from the track and via size list
    CUSTOM     ///< the custom track and via sizes of the router's size dialog
};

/**
 * Which of the three width sources a router is on.
 *
 * @param aIsCustomSizeInUse the board design settings use the custom track and via size.
 * @param aIsNetClassWidthInUse the board design settings take the width from the net class.
 */
HEADS_UP_WIDTH_SOURCE HeadsUpWidthSource( bool aIsCustomSizeInUse, bool aIsNetClassWidthInUse );

/// What the router is doing, as the display's routing line shows it. Read from the router, never kept beside it.
struct HEADS_UP_ROUTE
{
    HEADS_UP_CORNER_MODE  cornerMode = HEADS_UP_CORNER_MODE::MITERED_45;
    HEADS_UP_ROUTER_MODE  routerMode = HEADS_UP_ROUTER_MODE::SHOVE;
    bool isLookAheadOn = false; ///< the segment ahead of the last click is shown but not fixed (fix_all_segments off)
    int                   trackWidth = 0; ///< internal units
    HEADS_UP_WIDTH_SOURCE widthSource = HEADS_UP_WIDTH_SOURCE::NET_CLASS;
    int                   viaDiameter = 0; ///< internal units
    int                   viaDrill = 0;    ///< internal units
    wxString              layerFrom;
    wxString              layerTo;       ///< the layer a via takes the route to
    long long             netLength = 0; ///< internal units

    bool operator==( const HEADS_UP_ROUTE& aOther ) const = default;
};

/**
 * Build the routing line's state from the router's own settings.
 *
 * @param aIsDiffPair the router is laying a differential pair, so the width is the pair's.
 */
HEADS_UP_ROUTE HeadsUpRouteFromRouter( const PNS::ROUTING_SETTINGS& aSettings, const PNS::SIZES_SETTINGS& aSizes,
                                       bool aIsDiffPair, HEADS_UP_WIDTH_SOURCE aWidthSource, const wxString& aLayerFrom,
                                       const wxString& aLayerTo, long long aNetLength );

/// The net and the footprint reference of what is under the cursor. An empty string is "none".
struct HEADS_UP_ITEM
{
    wxString netName;
    wxString reference;

    bool operator==( const HEADS_UP_ITEM& aOther ) const = default;
};

/// Everything the display says. Two equal states say the same thing, so the window repaints only when they differ.
struct HEADS_UP_STATE
{
    EDA_UNITS units = EDA_UNITS::MM;

    VECTOR2D cursor; ///< absolute, as the status bar shows it: internal units with the user origin applied
    VECTOR2D delta;  ///< from the local origin, internal units with the user origin's axes applied

    wxString       layerName;
    KIGFX::COLOR4D layerColor;

    double gridX = 0.0; ///< internal units
    double gridY = 0.0; ///< internal units
    bool   isSnapEnabled = false;

    HEADS_UP_ITEM                 item;
    std::optional<HEADS_UP_ROUTE> route;

    bool operator==( const HEADS_UP_STATE& aOther ) const = default;
};

/// One line of the display. The line that names the layer is drawn with a swatch in the layer's color.
struct HEADS_UP_LINE
{
    wxString text;
    bool     isLayerLine = false;
};

/// The most characters of the routing line in one row of the display
constexpr int HEADS_UP_ROUTE_ROW_CHARACTERS = 60;

/**
 * Break a line of comma-separated parts into rows of at most @a aMaxCharacters, breaking after a comma and a space
 * only. A part longer than that stays whole on a row of its own. Joined with a space, the rows say what the line did.
 */
std::vector<wxString> WrapAtCommas( const wxString& aLine, int aMaxCharacters );

/**
 * The lines the display says, top to bottom. The routing line comes last, and only while there is a route; it is as
 * wide as the router's state needs, so it is wrapped at commas into rows that fit the canvas.
 */
std::vector<HEADS_UP_LINE> BuildHeadsUpLines( const HEADS_UP_STATE& aState );

/// The routing line alone
wxString BuildHeadsUpRouteLine( const HEADS_UP_ROUTE& aRoute, EDA_UNITS aUnits );

/// The cursor's offset from the local origin (what #ACTIONS::resetLocalCoords sets to the cursor)
VECTOR2D HeadsUpDelta( const VECTOR2D& aCursor, const BASE_SCREEN& aScreen );

/**
 * The net and reference of what is under a point, by asking the view's spatial index once.
 *
 * Pads win over vias, vias over tracks, tracks over zones and zones over footprint bodies, and within a kind an item
 * on the active layer wins over one that is not. Items on layers the view does not draw are not hit.
 *
 * @param aMargin how far from an item the point may be and still hit it, in internal units.
 */
HEADS_UP_ITEM FindHeadsUpItem( const KIGFX::VIEW& aView, const VECTOR2I& aPosition, int aMargin,
                               PCB_LAYER_ID aActiveLayer );

#endif // PCB_HEADS_UP_MODEL_H

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

#ifndef PCB_STACK_DRAW_ORDER_H
#define PCB_STACK_DRAW_ORDER_H

#include <vector>

#include <interaction_profile.h>
#include <layer_ids.h>

/**
 * The draw order that follows the board's layer stack (interaction-profile note, 5.7): the active layer on top, and
 * every other board layer in physical order as seen from the side being viewed, so that what is nearer the viewer draws
 * over what is further away. It needs no window: it says which view layers go in which order, and the canvas applies it.
 */

/// The values of the stack draw order setting: it follows the interaction profile, or it is off, or it is on.
enum STACK_DRAW_ORDER_SETTING
{
    STACK_DRAW_ORDER_FOLLOW_PROFILE = -1,
    STACK_DRAW_ORDER_OFF = 0,
    STACK_DRAW_ORDER_ON = 1
};

/**
 * Whether the stack draw order is used. A setting that follows the profile is on in the Altium-style profile and off in
 * stock KiCad. A stored 0 or 1 wins over the profile, and a value that is none of the three counts as following it.
 */
bool IsStackDrawOrderOn( int aSetting, INTERACTION_PROFILE_ID aProfile );

/**
 * The board layers a physical stack has, from the one nearest a viewer above the board to the one furthest from it:
 * the front documentation layers (courtyard, fabrication), silkscreen, mask, paste and adhesive, then the copper layers
 * from the top down, then the back adhesive, paste, mask, silkscreen, fabrication and courtyard. Flipped, it is that
 * list reversed. Edge.Cuts, the margin and the user layers are not part of a stack and are not in it.
 *
 * @param aCopperLayerCount the board's copper layer count, 2 or more.
 * @param aIsFlipped the board is viewed from the bottom.
 */
std::vector<PCB_LAYER_ID> StackLayersNearestFirst( int aCopperLayerCount, bool aIsFlipped );

/**
 * The view layers that draw with one board layer in the stack, nearest the viewer first: for copper its net names, pad
 * and via copper, clearance, point and zone sub-layers, for a technical layer its zone sub-layer.
 */
std::vector<int> StackLayerGroup( PCB_LAYER_ID aLayer );

/**
 * The view layers of the stack in drawing order, nearest the viewer first: the active layer's group first when the
 * active layer is part of the stack, then the others in stack order. The caller gives the slots these take in the
 * view's layer table to them in this order.
 *
 * Nothing is returned when the setting is off for the profile, so a caller that applies the result applies nothing and
 * the canvas keeps KiCad's fixed order.
 *
 * @param aCopperLayerCount the board's copper layer count.
 * @param aActiveLayer the layer being edited; one that is not in the stack changes nothing here.
 * @param aIsFlipped the board is viewed from the bottom.
 * @param aSetting the stored stack draw order setting.
 * @param aProfile the interaction profile.
 */
std::vector<int> StackDrawOrder( int aCopperLayerCount, PCB_LAYER_ID aActiveLayer, bool aIsFlipped, int aSetting,
                                 INTERACTION_PROFILE_ID aProfile );

#endif // PCB_STACK_DRAW_ORDER_H

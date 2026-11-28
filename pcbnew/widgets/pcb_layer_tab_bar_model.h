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

#ifndef PCB_LAYER_TAB_BAR_MODEL_H
#define PCB_LAYER_TAB_BAR_MODEL_H

#include <vector>

#include <interaction_profile.h>
#include <layer_ids.h>
#include <lseq.h>
#include <lset.h>

/**
 * The decisions behind the layer tab bar that need no window (interaction-profile note, 5.7): which tabs there are and
 * in what order, whether the bar is shown, and what each right-click command does to the visible layers. The widget
 * asks these on every repaint and keeps nothing of the answers.
 */

/**
 * The layers that get a tab, in stack order: the copper layers from top to bottom, then the paired technical layers
 * (silkscreen, mask, paste, adhesive, courtyard and fabrication, each front then back), then Edge.Cuts, the margin
 * and the user layers in layer order.
 *
 * @param aVisible the layers drawn now.
 * @param aEnabled the layers the board has, so a layer the board does not have gets no tab even if it is marked visible.
 */
std::vector<PCB_LAYER_ID> LayerTabOrder( const LSET& aVisible, const LSET& aEnabled );

/// The values of the layer tab bar setting: it follows the interaction profile, or it is off, or it is on.
enum LAYER_TAB_BAR_SETTING
{
    LAYER_TAB_BAR_FOLLOW_PROFILE = -1,
    LAYER_TAB_BAR_OFF = 0,
    LAYER_TAB_BAR_ON = 1
};

/**
 * Whether the bar is shown. A setting that follows the profile is on in the Altium-style profile and off in stock
 * KiCad. A stored 0 or 1 wins over the profile, and a value that is none of the three counts as following it.
 */
bool IsLayerTabBarShown( int aSetting, INTERACTION_PROFILE_ID aProfile );

/// The value to store when the View menu item is chosen: the opposite of what is showing now, stored explicitly.
int ToggledLayerTabBarSetting( int aSetting, INTERACTION_PROFILE_ID aProfile );

/// The right-click commands that change which layers are visible. Choosing a preset is not one of them.
enum class LAYER_TAB_COMMAND
{
    HIDE_THIS_LAYER,
    SHOW_ALL_LAYERS,
    SHOW_ONLY_COPPER_LAYERS
};

/**
 * The visible layers after a command.
 *
 * @param aLayer the tab that was right-clicked; only HIDE_THIS_LAYER reads it.
 */
LSET ApplyLayerTabCommand( LAYER_TAB_COMMAND aCommand, PCB_LAYER_ID aLayer, const LSET& aVisible,
                           const LSET& aEnabled );

/**
 * The layer to make active when the active layer is hidden: the next tab, or the one before when it was the last, or
 * UNDEFINED_LAYER when no other tab is left.
 */
PCB_LAYER_ID LayerToActivateAfterHiding( PCB_LAYER_ID aHidden, const LSET& aVisible, const LSET& aEnabled );

#endif // PCB_LAYER_TAB_BAR_MODEL_H

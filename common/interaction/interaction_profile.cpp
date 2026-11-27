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

#include <interaction_profile.h>

#include <hotkeys_basic.h>
#include <paths.h>
#include <pgm_base.h>
#include <settings/common_settings.h>
#include <tool/tool_action.h>
#include <wildcards_and_files_ext.h>

#include <wx/filename.h>

/// The Altium-style profile's table, from common/interaction/altium.hotkeys.
static const char s_altiumHotKeyTable[] =
#include "altium_hotkeys.h"
        ;


INTERACTION_PROFILE_ID INTERACTION_PROFILE::Active()
{
    COMMON_SETTINGS* settings = Pgm().GetCommonSettings();

    return settings ? settings->m_Input.interaction_profile : INTERACTION_PROFILE_ID::STOCK;
}


void INTERACTION_PROFILE::SetActive( INTERACTION_PROFILE_ID aProfile )
{
    COMMON_SETTINGS* settings = Pgm().GetCommonSettings();

    if( settings )
        settings->m_Input.interaction_profile = aProfile;
}


wxString INTERACTION_PROFILE::DisplayName( INTERACTION_PROFILE_ID aProfile )
{
    switch( aProfile )
    {
    case INTERACTION_PROFILE_ID::ALTIUM: return _( "Altium-style" );
    case INTERACTION_PROFILE_ID::STOCK:
    default: return _( "KiCad" );
    }
}


std::vector<INTERACTION_PROFILE_ID> INTERACTION_PROFILE::AllProfiles()
{
    return { INTERACTION_PROFILE_ID::STOCK, INTERACTION_PROFILE_ID::ALTIUM };
}


wxString INTERACTION_PROFILE::UserHotKeyFileName( INTERACTION_PROFILE_ID aProfile )
{
    // Built as text: wxFileName::SetExt() would replace the "altium" of user.altium as an extension
    wxString stem = aProfile == INTERACTION_PROFILE_ID::ALTIUM ? wxS( "user.altium" ) : wxS( "user" );

    return stem + wxS( "." ) + wxString( FILEEXT::HotkeyFileExtension );
}


wxString INTERACTION_PROFILE::UserHotKeyFilePath( INTERACTION_PROFILE_ID aProfile, const wxString& aDirectory )
{
    wxFileName fileName( aDirectory.IsEmpty() ? PATHS::GetUserSettingsPath() : aDirectory,
                         UserHotKeyFileName( aProfile ) );

    return fileName.GetFullPath();
}


std::string INTERACTION_PROFILE::HotKeyTableText( INTERACTION_PROFILE_ID aProfile )
{
    return aProfile == INTERACTION_PROFILE_ID::ALTIUM ? std::string( s_altiumHotKeyTable ) : std::string();
}


void INTERACTION_PROFILE::ParseHotKeyTable( const std::string& aText, PROFILE_HOTKEY_MAP& aBindings )
{
    std::string uncommented;
    size_t      lineStart = 0;

    while( lineStart <= aText.size() )
    {
        size_t lineEnd = aText.find( '\n', lineStart );

        if( lineEnd == std::string::npos )
            lineEnd = aText.size();

        if( aText.compare( lineStart, 1, "#" ) != 0 )
            uncommented.append( aText, lineStart, lineEnd - lineStart ).push_back( '\n' );

        lineStart = lineEnd + 1;
    }

    ParseHotKeyConfig( wxString::FromUTF8( uncommented ), aBindings );
}


PROFILE_HOTKEY_LAYER INTERACTION_PROFILE::BuildHotKeyLayerFromText( const std::string& aTableText )
{
    PROFILE_HOTKEY_LAYER layer;

    layer.m_isActive = true;
    ParseHotKeyTable( aTableText, layer.m_bindings );

    for( const auto& [name, keys] : layer.m_bindings )
    {
        std::set<int>& takenInScope = layer.m_takenKeysByScope[ScopeOf( name )];

        for( int key : { keys.first, keys.second } )
        {
            if( key > 0 )
            {
                layer.m_takenKeys.insert( key );
                takenInScope.insert( key );
            }
        }
    }

    return layer;
}


PROFILE_HOTKEY_LAYER INTERACTION_PROFILE::BuildHotKeyLayer( INTERACTION_PROFILE_ID aProfile )
{
    if( aProfile == INTERACTION_PROFILE_ID::STOCK )
        return PROFILE_HOTKEY_LAYER();

    return BuildHotKeyLayerFromText( HotKeyTableText( aProfile ) );
}


std::string INTERACTION_PROFILE::ScopeOf( const std::string& aActionName )
{
    size_t dot = aActionName.find( '.' );

    return dot == std::string::npos ? std::string() : aActionName.substr( 0, dot + 1 );
}


std::pair<int, int> INTERACTION_PROFILE::ApplyHotKeyLayer( const PROFILE_HOTKEY_LAYER& aLayer,
                                                           const std::string&          aActionName,
                                                           std::pair<int, int>         aBaseKeys,
                                                           const std::string& aEditorScope, bool aIsContextAction )
{
    // Only real actions are profiled: the editor's read-only gesture and platform entries have no name
    if( !aLayer.m_isActive || aActionName.empty() )
        return aBaseKeys;

    auto binding = aLayer.m_bindings.find( aActionName );

    if( binding != aLayer.m_bindings.end() )
        return binding->second;

    if( aIsContextAction )
        return aBaseKeys;

    // The keys the table takes from this action: those its own scope binds, and those `common.` binds (in every
    // editor). An action that is in every editor also loses the keys of the editor it is resolved for, or of every
    // editor when that is not known.
    static const std::string commonScope = "common.";

    std::string actionScope = ScopeOf( aActionName );

    auto isTakenIn = [&]( const std::string& aScope, int aKey )
    {
        auto taken = aLayer.m_takenKeysByScope.find( aScope );

        return taken != aLayer.m_takenKeysByScope.end() && taken->second.count( aKey );
    };

    auto isTaken = [&]( int aKey )
    {
        if( aKey <= 0 )
            return false;

        if( isTakenIn( commonScope, aKey ) )
            return true;

        if( actionScope != commonScope )
            return isTakenIn( actionScope, aKey );

        if( !aEditorScope.empty() )
            return isTakenIn( aEditorScope, aKey );

        return aLayer.m_takenKeys.count( aKey ) > 0;
    };

    if( isTaken( aBaseKeys.first ) )
        aBaseKeys.first = 0;

    if( isTaken( aBaseKeys.second ) )
        aBaseKeys.second = 0;

    return aBaseKeys;
}


std::vector<std::string> INTERACTION_PROFILE::UnregisteredActionNames( const PROFILE_HOTKEY_LAYER&      aLayer,
                                                                       const std::vector<TOOL_ACTION*>& aActions,
                                                                       const std::vector<std::string>&  aNamePrefixes )
{
    std::set<std::string> registeredNames;

    for( const TOOL_ACTION* action : aActions )
        registeredNames.insert( action->GetName() );

    std::vector<std::string> unregistered;

    for( const auto& [name, keys] : aLayer.m_bindings )
    {
        bool isChecked = false;

        for( const std::string& prefix : aNamePrefixes )
            isChecked = isChecked || name.compare( 0, prefix.size(), prefix ) == 0;

        if( isChecked && !registeredNames.count( name ) )
            unregistered.push_back( name );
    }

    return unregistered;
}

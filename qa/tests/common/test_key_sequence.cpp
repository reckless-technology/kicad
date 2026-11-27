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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <key_sequence.h>
#include <tool/actions.h>

#include <wx/defs.h>

// The key-sequence state machine (interaction-profile note, 5.3), on small synthetic trees, with no GUI. The real
// trees' sequences are driven through it in qa_eeschema and qa_pcbnew.

namespace
{
using OUTCOME = KEY_SEQUENCE_RESULT::OUTCOME;

/// A bar with the shapes the engine has to tell apart: a menu with leaves and a nested submenu, the same mnemonic as a
/// submenu of another menu, a leaf that runs something else when typed, a key-only entry, a key-only popup and an
/// entry that is not available.
std::shared_ptr<const MENU_DEF> testTree()
{
    using M = MENU_DEF;

    M place = M::Submenu( wxS( "&Place" ),
                          { M::Item( wxS( "&Wire" ), ACTIONS::undo ), M::Item( wxS( "&Bus" ), ACTIONS::redo ),
                            M::Separator(),
                            M::Submenu( wxS( "&Directives" ),
                                        { M::Item( wxS( "&No Connect" ), ACTIONS::copy ),
                                          M::Item( wxS( "&Blanket" ), ACTIONS::paste ),
                                          M::Submenu( wxS( "&Deep" ), { M::Item( wxS( "&Leaf" ), ACTIONS::cut ) } ) } ),
                            M::Item( wxS( "&Hidden" ), ACTIONS::find )
                                    .When(
                                            []()
                                            {
                                                return false;
                                            } ),
                            M::Slot( wxS( "&Language" ), "languages" ) } );

    M edit = M::Submenu(
            wxS( "&Edit" ),
            { M::Item( wxS( "&Delete" ), ACTIONS::deleteTool ),
              M::Submenu( wxS( "&Move" ), { M::Item( wxS( "&Move" ), ACTIONS::zoomRedraw ),
                                            M::Item( wxS( "Move &Selection" ), ACTIONS::zoomRedraw ).KeyOnly(),
                                            M::Item( wxS( "&Drag" ), ACTIONS::zoomCenter ) } ) } );

    M view = M::Submenu( wxS( "&View" ),
                         { M::Item( wxS( "Zoom &In" ), ACTIONS::zoomInCenter ).ByKey( ACTIONS::zoomIn ),
                           M::Item( wxS( "Zoom &Out" ), ACTIONS::zoomOutCenter ).ByKey( ACTIONS::zoomOut ) } );

    M zoom = M::Submenu( wxS( "&Zoom" ), { M::Item( wxS( "&Window" ), ACTIONS::zoomTool ) } ).KeyOnly();

    // A top-level menu and a submenu of another menu both opened by M: the top-level one is the root
    M move = M::Submenu( wxS( "&More" ), { M::Item( wxS( "&Alpha" ), ACTIONS::selectAll ) } );

    return std::make_shared<const MENU_DEF>( M::Submenu( wxEmptyString, { place, edit, view, zoom, move } ) );
}


/// An engine on the test tree, with the root entered
KEY_SEQUENCE engineAt( wxUniChar aRoot )
{
    KEY_SEQUENCE engine;

    engine.SetTree( testTree() );
    BOOST_REQUIRE( engine.Enter( aRoot ).m_outcome == OUTCOME::CONSUMED );
    return engine;
}
} // namespace


BOOST_AUTO_TEST_SUITE( KeySequenceEngine )

BOOST_AUTO_TEST_CASE( ACompleteSequenceRunsItsActionAndClears )
{
    KEY_SEQUENCE engine = engineAt( 'P' );

    BOOST_CHECK( engine.IsPending() );
    BOOST_CHECK_EQUAL( engine.KeysTyped().ToStdString(), std::string( "P \xE2\x80\xBA" ) );

    KEY_SEQUENCE_RESULT result = engine.HandleKey( 'W', false );

    BOOST_CHECK( result.m_outcome == OUTCOME::RUN_ACTION );
    BOOST_CHECK( result.m_action == &ACTIONS::undo );
    BOOST_CHECK( !engine.IsPending() );
    BOOST_CHECK( engine.KeysTyped().IsEmpty() );
}


BOOST_AUTO_TEST_CASE( ASubmenuDescendsAndALeafBelowItRuns )
{
    KEY_SEQUENCE engine = engineAt( 'P' );

    KEY_SEQUENCE_RESULT result = engine.HandleKey( 'D', false );

    BOOST_CHECK( result.m_outcome == OUTCOME::CONSUMED );
    BOOST_CHECK( engine.IsPending() );
    BOOST_CHECK_EQUAL( engine.KeysTyped().ToStdString(), std::string( "P D \xE2\x80\xBA" ) );

    result = engine.HandleKey( 'N', false );
    BOOST_CHECK( result.m_outcome == OUTCOME::RUN_ACTION );
    BOOST_CHECK( result.m_action == &ACTIONS::copy );
    BOOST_CHECK( !engine.IsPending() );

    // Two levels of submenu
    engine = engineAt( 'P' );
    BOOST_CHECK( engine.HandleKey( 'D', false ).m_outcome == OUTCOME::CONSUMED );
    BOOST_CHECK( engine.HandleKey( 'D', false ).m_outcome == OUTCOME::CONSUMED );
    BOOST_CHECK_EQUAL( engine.KeysTyped().ToStdString(), std::string( "P D D \xE2\x80\xBA" ) );

    result = engine.HandleKey( 'L', false );
    BOOST_CHECK( result.m_outcome == OUTCOME::RUN_ACTION );
    BOOST_CHECK( result.m_action == &ACTIONS::cut );
}


BOOST_AUTO_TEST_CASE( APartialSequenceStaysPendingAndRunsNothing )
{
    KEY_SEQUENCE engine = engineAt( 'P' );

    KEY_SEQUENCE_RESULT result = engine.HandleKey( 'D', false );

    BOOST_CHECK( result.m_outcome == OUTCOME::CONSUMED );
    BOOST_CHECK( result.m_action == nullptr );
    BOOST_CHECK( engine.IsPending() );
}


BOOST_AUTO_TEST_CASE( AMistypedKeyClearsAndIsPassedOn )
{
    KEY_SEQUENCE engine = engineAt( 'P' );

    KEY_SEQUENCE_RESULT result = engine.HandleKey( 'Q', false );

    BOOST_CHECK( result.m_outcome == OUTCOME::NOT_MINE );
    BOOST_CHECK( result.m_action == nullptr );
    BOOST_CHECK( !engine.IsPending() );

    // With nothing pending, the next key is not the engine's either: it is dispatched as a hotkey
    BOOST_CHECK( engine.HandleKey( 'W', false ).m_outcome == OUTCOME::NOT_MINE );

    // A separator, a slot and an entry that is not available are not entries to type
    for( int key : { 'L', 'H' } )
    {
        engine = engineAt( 'P' );
        BOOST_CHECK_MESSAGE( engine.HandleKey( key, false ).m_outcome == OUTCOME::NOT_MINE,
                             "the key " << key << " selected an entry that is not there" );
        BOOST_CHECK( !engine.IsPending() );
    }
}


BOOST_AUTO_TEST_CASE( EscapeClearsAndIsConsumed )
{
    KEY_SEQUENCE engine = engineAt( 'P' );

    BOOST_CHECK( engine.HandleKey( 'D', false ).m_outcome == OUTCOME::CONSUMED );

    KEY_SEQUENCE_RESULT result = engine.HandleKey( WXK_ESCAPE, false );

    BOOST_CHECK( result.m_outcome == OUTCOME::CONSUMED );
    BOOST_CHECK( !engine.IsPending() );

    // Esc with nothing pending is not the engine's: it cancels the tool as usual
    BOOST_CHECK( engine.HandleKey( WXK_ESCAPE, false ).m_outcome == OUTCOME::NOT_MINE );
}


BOOST_AUTO_TEST_CASE( ModifierOnlyKeysAreIgnored )
{
    for( int modifier : { WXK_SHIFT, WXK_CONTROL, WXK_ALT, WXK_RAW_CONTROL, WXK_WINDOWS_LEFT, WXK_WINDOWS_RIGHT } )
    {
        KEY_SEQUENCE engine = engineAt( 'P' );
        unsigned     token = engine.Token();

        KEY_SEQUENCE_RESULT result = engine.HandleKey( modifier, false );

        BOOST_CHECK_MESSAGE( result.m_outcome == OUTCOME::CONSUMED, "modifier " << modifier << " was not ignored" );
        BOOST_CHECK( engine.IsPending() );
        BOOST_CHECK_EQUAL( engine.Token(), token );
        BOOST_CHECK( engine.HandleKey( 'W', false ).m_outcome == OUTCOME::RUN_ACTION );
    }

    BOOST_CHECK( KEY_SEQUENCE::IsModifierOnly( WXK_SHIFT ) );
    BOOST_CHECK( !KEY_SEQUENCE::IsModifierOnly( 'P' ) );
}


BOOST_AUTO_TEST_CASE( ALetterIsAMnemonicInEitherCaseAndWithShiftButNotWithACommandModifier )
{
    for( int key : { 'w', 'W' } )
    {
        KEY_SEQUENCE engine = engineAt( 'P' );
        BOOST_CHECK( engine.HandleKey( key, false ).m_outcome == OUTCOME::RUN_ACTION );
    }

    // Ctrl, Alt or Meta down: not a mnemonic, so the key clears the sequence and is dispatched as the hotkey it is
    KEY_SEQUENCE        engine = engineAt( 'P' );
    KEY_SEQUENCE_RESULT result = engine.HandleKey( 'W', true );

    BOOST_CHECK( result.m_outcome == OUTCOME::NOT_MINE );
    BOOST_CHECK( !engine.IsPending() );
}


BOOST_AUTO_TEST_CASE( AKeyThatIsAMnemonicGoesToTheChildOnlyWhilePending )
{
    // W is the mnemonic of Place > Wire. The engine takes it only after a root: with nothing pending it is not its key,
    // so whatever hotkey W has is dispatched
    KEY_SEQUENCE engine;

    engine.SetTree( testTree() );
    BOOST_CHECK( engine.HandleKey( 'W', false ).m_outcome == OUTCOME::NOT_MINE );

    BOOST_REQUIRE( engine.Enter( 'P' ).m_outcome == OUTCOME::CONSUMED );

    KEY_SEQUENCE_RESULT result = engine.HandleKey( 'W', false );

    BOOST_CHECK( result.m_outcome == OUTCOME::RUN_ACTION );
    BOOST_CHECK( result.m_action == &ACTIONS::undo );
}


BOOST_AUTO_TEST_CASE( TheHintTimerShowsTheMenuOnlyIfTheSequenceIsStillWaiting )
{
    KEY_SEQUENCE engine = engineAt( 'P' );

    // Fired while the root's menu is the state: show it, and the menu takes over
    unsigned            token = engine.Token();
    KEY_SEQUENCE_RESULT result = engine.HintTimerFired( token );

    BOOST_REQUIRE( result.m_outcome == OUTCOME::SHOW_NODE );
    BOOST_REQUIRE( result.m_node != nullptr );
    BOOST_CHECK_EQUAL( result.m_node->m_label.ToStdString(), std::string( "&Place" ) );
    BOOST_CHECK( !engine.IsPending() );

    // A second ring of the same timer shows nothing
    BOOST_CHECK( engine.HintTimerFired( token ).m_outcome == OUTCOME::NOT_MINE );

    // Typed fast: the sequence completes before the timer, so the late timer shows nothing
    engine = engineAt( 'P' );
    token = engine.Token();
    BOOST_CHECK( engine.HandleKey( 'W', false ).m_outcome == OUTCOME::RUN_ACTION );

    result = engine.HintTimerFired( token );
    BOOST_CHECK( result.m_outcome == OUTCOME::NOT_MINE );
    BOOST_CHECK( result.m_node == nullptr );

    // A timer started before a descent is stale: only the one started at the submenu shows the submenu
    engine = engineAt( 'P' );
    unsigned atRoot = engine.Token();
    BOOST_CHECK( engine.HandleKey( 'D', false ).m_outcome == OUTCOME::CONSUMED );
    unsigned atSubmenu = engine.Token();

    BOOST_CHECK_NE( atRoot, atSubmenu );
    BOOST_CHECK( engine.HintTimerFired( atRoot ).m_outcome == OUTCOME::NOT_MINE );
    BOOST_CHECK( engine.IsPending() );

    result = engine.HintTimerFired( atSubmenu );
    BOOST_REQUIRE( result.m_outcome == OUTCOME::SHOW_NODE );
    BOOST_CHECK_EQUAL( result.m_node->m_label.ToStdString(), std::string( "&Directives" ) );

    // An abandoned sequence shows nothing either
    engine = engineAt( 'P' );
    token = engine.Token();
    engine.HandleKey( WXK_ESCAPE, false );
    BOOST_CHECK( engine.HintTimerFired( token ).m_outcome == OUTCOME::NOT_MINE );
}


BOOST_AUTO_TEST_CASE( AKeyOnlyEntryRunsAndAKeyActionReplacesTheMenusAction )
{
    // Edit > Move > Move Selection exists for the keys only, and runs the same action as Move
    KEY_SEQUENCE engine = engineAt( 'M' );

    BOOST_REQUIRE( KEY_SEQUENCE::FindRoot( *testTree(), 'M' ) != nullptr );
    BOOST_CHECK_EQUAL( KEY_SEQUENCE::FindRoot( *testTree(), 'M' )->m_label.ToStdString(), std::string( "&More" ) );

    // The key-only entry is under Edit's Move, whose root letter M the top-level More menu holds: go through Edit
    engine = engineAt( 'E' );
    BOOST_CHECK( engine.HandleKey( 'M', false ).m_outcome == OUTCOME::CONSUMED );

    KEY_SEQUENCE_RESULT result = engine.HandleKey( 'S', false );
    BOOST_CHECK( result.m_outcome == OUTCOME::RUN_ACTION );
    BOOST_CHECK( result.m_action == &ACTIONS::zoomRedraw );

    // View > Zoom In: the menu's action zooms about the window center, the key sequence's about the cursor
    engine = engineAt( 'V' );
    result = engine.HandleKey( 'I', false );
    BOOST_CHECK( result.m_action == &ACTIONS::zoomIn );
    BOOST_CHECK( result.m_action != &ACTIONS::zoomInCenter );
}


BOOST_AUTO_TEST_CASE( ARootIsATopLevelMenuElseASubmenuOfOneAndACanBeKeyOnly )
{
    std::shared_ptr<const MENU_DEF> tree = testTree();

    // Top-level menus first, so the first letter of More wins over Edit's Move
    const MENU_DEF* more = KEY_SEQUENCE::FindRoot( *tree, 'M' );
    BOOST_REQUIRE( more );
    BOOST_CHECK_EQUAL( more->m_label.ToStdString(), std::string( "&More" ) );

    // Directives is a submenu of Place and nothing at the top has D: it is a root
    const MENU_DEF* directives = KEY_SEQUENCE::FindRoot( *tree, 'D' );
    BOOST_REQUIRE( directives );
    BOOST_CHECK_EQUAL( directives->m_label.ToStdString(), std::string( "&Directives" ) );

    // A popup that no menu bar shows is a root too
    const MENU_DEF* zoom = KEY_SEQUENCE::FindRoot( *tree, 'Z' );
    BOOST_REQUIRE( zoom );
    BOOST_CHECK( zoom->m_isKeyOnly );

    BOOST_CHECK( KEY_SEQUENCE::FindRoot( *tree, 'Q' ) == nullptr );
    BOOST_CHECK( KEY_SEQUENCE::FindRoot( *tree, 0 ) == nullptr );

    // The whole path through the tree
    const MENU_DEF* leaf = KEY_SEQUENCE::Resolve( *tree, { 'P', 'D', 'D', 'L' } );
    BOOST_REQUIRE( leaf );
    BOOST_CHECK( leaf->m_action == &ACTIONS::cut );
    BOOST_CHECK( KEY_SEQUENCE::Resolve( *tree, { 'P', 'D', 'X' } ) == nullptr );
    BOOST_CHECK( KEY_SEQUENCE::Resolve( *tree, {} ) == nullptr );

    // The root letter of a leaf, a missing tree or an unknown menu enters nothing
    KEY_SEQUENCE engine;
    BOOST_CHECK( engine.Enter( 'P' ).m_outcome == OUTCOME::NOT_MINE ); // no tree yet
    engine.SetTree( tree );
    BOOST_CHECK( engine.Enter( 'Q' ).m_outcome == OUTCOME::NOT_MINE );
    BOOST_CHECK( !engine.IsPending() );
    BOOST_CHECK( engine.Enter( 'Z' ).m_outcome == OUTCOME::CONSUMED );
    BOOST_CHECK( engine.HandleKey( 'W', false ).m_action == &ACTIONS::zoomTool );
}


BOOST_AUTO_TEST_CASE( EnteringARootAgainStartsOver )
{
    KEY_SEQUENCE engine = engineAt( 'P' );

    BOOST_CHECK( engine.HandleKey( 'D', false ).m_outcome == OUTCOME::CONSUMED );
    BOOST_CHECK( engine.Enter( 'E' ).m_outcome == OUTCOME::CONSUMED );
    BOOST_CHECK_EQUAL( engine.KeysTyped().ToStdString(), std::string( "E \xE2\x80\xBA" ) );
    BOOST_CHECK( engine.HandleKey( 'D', false ).m_action == &ACTIONS::deleteTool );
}


BOOST_AUTO_TEST_CASE( LetterHintsShowTheMnemonicBesideEachEntry )
{
    MENU_DEF        hinted = MENU_DEF::WithLetterHints( *testTree() );
    const MENU_DEF* wire = KEY_SEQUENCE::Resolve( hinted, { 'P', 'W' } );
    const MENU_DEF* deep = KEY_SEQUENCE::Resolve( hinted, { 'P', 'D', 'D', 'L' } );

    BOOST_REQUIRE( wire );
    BOOST_REQUIRE( deep );
    BOOST_CHECK_EQUAL( wire->m_label.ToStdString(), std::string( "&Wire\tW" ) );
    BOOST_CHECK_EQUAL( deep->m_label.ToStdString(), std::string( "&Leaf\tL" ) );

    // The original is untouched, and a separator and a slot get no hint
    BOOST_CHECK_EQUAL( KEY_SEQUENCE::Resolve( *testTree(), { 'P', 'W' } )->m_label.ToStdString(),
                       std::string( "&Wire" ) );
    BOOST_CHECK( hinted.m_children[0].m_children[2].IsSeparator() );
    BOOST_CHECK( hinted.m_children[0].m_children[2].m_label.IsEmpty() );
}

BOOST_AUTO_TEST_SUITE_END()

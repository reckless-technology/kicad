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

#include <qa_utils/wx_utils/unit_test_utils.h>

#include <menu_def.h>
#include <tool/action_manager.h>
#include <tool/action_menu.h>
#include <tool/actions.h>
#include <tool/conditional_menu.h>
#include <tool/selection.h>

#include "menu_tree_checks.h"

// MENU_DEF, its walkers and the builders that turn a tree into menus (interaction-profile note, 5.4), on small
// synthetic trees. The editors' real trees are checked in qa_eeschema and qa_pcbnew, where their actions are linked.

namespace
{
/// An action that registers nothing and has no name: the default constructor is protected for this reason.
class UNREGISTERED_ACTION : public TOOL_ACTION
{
public:
    UNREGISTERED_ACTION() :
            TOOL_ACTION()
    {
    }
};

/// A tree that uses each kind of node once: a menu with an entry, a separator, a submenu and a slot.
MENU_DEF syntheticBar()
{
    using M = MENU_DEF;

    return M::Submenu( wxEmptyString,
                       { M::Submenu( wxS( "&Edit" ),
                                     { M::Item( wxS( "&Undo" ), ACTIONS::undo ),
                                       M::Item( wxS( "&Redo" ), ACTIONS::redo ), M::Separator(),
                                       M::Submenu( wxS( "&More" ), { M::Item( wxS( "Cu&t" ), ACTIONS::cut ),
                                                                     M::CheckItem( wxS( "&Copy" ), ACTIONS::copy ) } ),
                                       M::Slot( wxS( "&Language" ), "languages" ) } ),
                         M::Submenu( wxS( "&View" ), { M::Item( wxS( "Zoom &In" ), ACTIONS::zoomIn ) } ) } );
}
} // namespace


BOOST_AUTO_TEST_SUITE( MenuDef )

BOOST_AUTO_TEST_CASE( MnemonicIsTheLetterAfterTheFirstSingleAmpersand )
{
    BOOST_CHECK( MENU_DEF::Item( wxS( "&Undo" ), ACTIONS::undo ).Mnemonic() == 'U' );
    BOOST_CHECK( MENU_DEF::Item( wxS( "Cu&t" ), ACTIONS::cut ).Mnemonic() == 'T' );
    BOOST_CHECK( MENU_DEF::Item( wxS( "&t" ), ACTIONS::cut ).Mnemonic() == 'T' ); // case does not matter
    BOOST_CHECK( MENU_DEF::Item( wxS( "Save && Close" ), ACTIONS::save ).Mnemonic() == 0 );
    BOOST_CHECK( MENU_DEF::Item( wxS( "Save && &Close" ), ACTIONS::save ).Mnemonic() == 'C' );
    BOOST_CHECK( MENU_DEF::Item( wxS( "Copy" ), ACTIONS::copy ).Mnemonic() == 0 );
    BOOST_CHECK( MENU_DEF::Item( wxS( "Trailing &" ), ACTIONS::copy ).Mnemonic() == 0 );
    BOOST_CHECK( MENU_DEF::Separator().Mnemonic() == 0 );
}


BOOST_AUTO_TEST_CASE( WalkVisitsEveryNodeParentsFirst )
{
    MENU_DEF bar = syntheticBar();

    std::vector<wxString> labels;
    std::vector<wxString> paths;

    bar.Walk(
            [&]( const MENU_DEF& aNode, const std::vector<wxString>& aPath )
            {
                labels.push_back( aNode.IsSeparator() ? wxString( wxS( "-" ) ) : aNode.m_label );
                paths.push_back( aPath.empty() ? wxString( wxS( "." ) ) : aPath.back() );
            } );

    BOOST_REQUIRE_EQUAL( labels.size(), 10u );
    BOOST_CHECK_EQUAL( labels[0], wxS( "&Edit" ) );
    BOOST_CHECK_EQUAL( labels[1], wxS( "&Undo" ) );
    BOOST_CHECK_EQUAL( labels[4], wxS( "&More" ) );
    BOOST_CHECK_EQUAL( labels[5], wxS( "Cu&t" ) );
    BOOST_CHECK_EQUAL( paths[5], wxS( "&More" ) );
    BOOST_CHECK_EQUAL( paths[0], wxS( "." ) );

    BOOST_CHECK_EQUAL( bar.Actions().size(), 5u );
    BOOST_CHECK( bar.Contains( ACTIONS::cut ) );
    BOOST_CHECK( !bar.Contains( ACTIONS::paste ) );
    BOOST_REQUIRE_EQUAL( bar.Slots().size(), 1u );
    BOOST_CHECK_EQUAL( bar.Slots()[0], "languages" );
}


BOOST_AUTO_TEST_CASE( ASyntheticTreeIsClean )
{
    MENU_DEF bar = syntheticBar();

    BOOST_CHECK( bar.DuplicateMnemonics().empty() );
    BOOST_CHECK( bar.UnresolvedActions( ACTION_MANAGER::GetActionList() ).empty() );
}


BOOST_AUTO_TEST_CASE( ADuplicateMnemonicIsFoundAtEveryLevel )
{
    using M = MENU_DEF;

    // The two entries of the submenu share U, and the two menus of the bar share E
    MENU_DEF bar =
            M::Submenu( wxEmptyString,
                        { M::Submenu( wxS( "&Edit" ),
                                      { M::Submenu( wxS( "&Sub" ), { M::Item( wxS( "&Undo" ), ACTIONS::undo ),
                                                                     M::Item( wxS( "Red&u" ), ACTIONS::redo ) } ) } ),
                          M::Submenu( wxS( "&Extra" ), { M::Item( wxS( "&Cut" ), ACTIONS::cut ) } ) } );

    std::vector<wxString> messages = bar.DuplicateMnemonics();

    BOOST_REQUIRE_EQUAL( messages.size(), 2u );

    bool isTopLevelFound = false;
    bool isSubmenuFound = false;

    for( const wxString& message : messages )
    {
        isTopLevelFound |= message.Contains( wxS( "(top level)" ) ) && message.Contains( wxS( "&Extra" ) );
        isSubmenuFound |= message.Contains( wxS( "&Edit > &Sub" ) ) && message.Contains( wxS( "Red&u" ) );
    }

    BOOST_CHECK( isTopLevelFound );
    BOOST_CHECK( isSubmenuFound );
}


BOOST_AUTO_TEST_CASE( AnActionThatIsNotRegisteredIsNamedOrDescribed )
{
    using M = MENU_DEF;

    UNREGISTERED_ACTION unregistered;

    MENU_DEF bar = M::Submenu( wxEmptyString, { M::Submenu( wxS( "&Edit" ), { M::Item( wxS( "&Undo" ), ACTIONS::undo ),
                                                                              M::Item( wxS( "&Ghost" ), unregistered ),
                                                                              M::Submenu( wxS( "&Empty" ), {} ) } ) } );

    std::vector<wxString> messages = bar.UnresolvedActions( ACTION_MANAGER::GetActionList() );

    BOOST_REQUIRE_EQUAL( messages.size(), 2u );
    BOOST_CHECK( messages[0].Contains( wxS( "&Ghost" ) ) );
    BOOST_CHECK( messages[1].Contains( wxS( "&Empty" ) ) );

    // A named action that is missing from the list of registered ones is named in the message
    std::list<TOOL_ACTION*> withoutUndo;

    for( TOOL_ACTION* action : ACTION_MANAGER::GetActionList() )
    {
        if( action != &ACTIONS::undo )
            withoutUndo.push_back( action );
    }

    messages = bar.UnresolvedActions( withoutUndo );

    BOOST_REQUIRE_EQUAL( messages.size(), 3u );
    BOOST_CHECK( messages[0].Contains( wxString( ACTIONS::undo.GetName() ) ) );
}


BOOST_AUTO_TEST_CASE( AnEmptyOrFaultyTreeFailsTheEditorChecks )
{
    MENU_DEF empty = MENU_DEF::Submenu( wxEmptyString, {} );

    BOOST_CHECK( empty.Actions().empty() );
    BOOST_CHECK( empty.Slots().empty() );
    BOOST_CHECK( !MENU_TREE_CHECKS::ActionProblems( empty, 1 ).empty() );
    BOOST_CHECK( !MENU_TREE_CHECKS::MnemonicProblems( empty, 1 ).empty() );
    BOOST_CHECK( !MENU_TREE_CHECKS::DisappearanceProblems( empty, {}, {} ).empty() );

    // The same checks pass on a clean tree that is large enough
    MENU_DEF bar = syntheticBar();

    BOOST_CHECK( MENU_TREE_CHECKS::ActionProblems( bar, 5 ).empty() );
    BOOST_CHECK( MENU_TREE_CHECKS::MnemonicProblems( bar, 2 ).empty() );
    BOOST_CHECK_EQUAL( MENU_TREE_CHECKS::ActionProblems( bar, 6 ).size(), 1u ); // one more than it has

    // A row whose action is nowhere in the tree and not omitted is a problem, and an omission makes it none
    std::vector<MENU_OMISSION> omitted = { { "E P", nullptr, "reason" } };
    std::vector<MENU_TREE_ROW> rows = { { "E P", "common.Interactive.selectAll", false } };

    BOOST_REQUIRE( MENU_TREE_CHECKS::FindRegistered( "common.Interactive.selectAll" ) );
    BOOST_CHECK( !bar.Contains( *MENU_TREE_CHECKS::FindRegistered( "common.Interactive.selectAll" ) ) );
    BOOST_CHECK( !MENU_TREE_CHECKS::DisappearanceProblems( bar, omitted, rows ).empty() ); // fewer than 20 rows

    std::vector<MENU_TREE_ROW> manyRows( 20, rows[0] );
    std::vector<MENU_TREE_ROW> keptRows( 20, MENU_TREE_ROW{ "E U", "common.Interactive.undo", true } );
    std::vector<MENU_TREE_ROW> wrongKeys( 20, MENU_TREE_ROW{ "E R", "common.Interactive.undo", true } );

    BOOST_CHECK_EQUAL( MENU_TREE_CHECKS::DisappearanceProblems( bar, omitted, manyRows ).size(), 20u );
    BOOST_CHECK_EQUAL( MENU_TREE_CHECKS::DisappearanceProblems(
                               bar, { { "E P", "common.Interactive.selectAll", "reason" } }, manyRows )
                               .size(),
                       0u );

    // Rows whose action is in the tree pass, and keys that select another entry are a problem
    BOOST_CHECK_EQUAL( MENU_TREE_CHECKS::DisappearanceProblems( bar, omitted, keptRows ).size(), 0u );
    BOOST_CHECK_EQUAL( MENU_TREE_CHECKS::DisappearanceProblems( bar, omitted, wrongKeys ).size(), 20u );
}


BOOST_AUTO_TEST_CASE( TheActionMenuHasTheTreesLabelsSeparatorsAndSubmenus )
{
    KI_TEST::DISPLAY_FOR_WIDGETS display;

    if( !display.IsAvailable() )
    {
        BOOST_TEST_MESSAGE( "Skipping test - no display available" );
        return;
    }

    ACTION_MANAGER actionManager( nullptr ); // gives every action its id, which a menu entry needs

    MENU_DEF bar = syntheticBar();

    std::unique_ptr<ACTION_MENU> menu(
            BuildActionMenu( bar.m_children[0], nullptr,
                             []( const std::string& aSlot, const MENU_DEF& aNode, ACTION_MENU& aParent )
                             {
                                 BOOST_CHECK_EQUAL( aSlot, "languages" );
                                 aParent.Append( wxID_ANY, aNode.m_label );
                             } ) );

    BOOST_CHECK_EQUAL( menu->GetTitle(), wxS( "&Edit" ) );

    wxMenuItemList& items = menu->GetMenuItems();
    BOOST_REQUIRE_EQUAL( items.GetCount(), 5u );

    auto itemAt = [&]( size_t aIndex )
    {
        return items.Item( aIndex )->GetData();
    };

    BOOST_CHECK_EQUAL( itemAt( 0 )->GetItemLabel(), wxS( "&Undo" ) );
    BOOST_CHECK_EQUAL( itemAt( 0 )->GetId(), ACTIONS::undo.GetUIId() );
    BOOST_CHECK_EQUAL( itemAt( 1 )->GetItemLabel(), wxS( "&Redo" ) );
    BOOST_CHECK( itemAt( 2 )->IsSeparator() );

    BOOST_REQUIRE( itemAt( 3 )->IsSubMenu() );
    wxMenu* submenu = itemAt( 3 )->GetSubMenu();
    BOOST_CHECK_EQUAL( itemAt( 3 )->GetItemLabel(), wxS( "&More" ) );
    BOOST_REQUIRE_EQUAL( submenu->GetMenuItemCount(), 2u );
    BOOST_CHECK_EQUAL( submenu->FindItemByPosition( 0 )->GetItemLabel(), wxS( "Cu&t" ) );
    BOOST_CHECK_EQUAL( submenu->FindItemByPosition( 0 )->GetKind(), wxITEM_NORMAL );
    BOOST_CHECK_EQUAL( submenu->FindItemByPosition( 1 )->GetKind(), wxITEM_CHECK );

    // The slot's filler put its entry where the slot stands
    BOOST_CHECK_EQUAL( itemAt( 4 )->GetItemLabel(), wxS( "&Language" ) );
}


BOOST_AUTO_TEST_CASE( AnEntryThatIsNotAvailableIsLeftOut )
{
    KI_TEST::DISPLAY_FOR_WIDGETS display;

    if( !display.IsAvailable() )
    {
        BOOST_TEST_MESSAGE( "Skipping test - no display available" );
        return;
    }

    ACTION_MANAGER actionManager( nullptr ); // gives every action its id, which a menu entry needs

    using M = MENU_DEF;

    bool isOn = false;

    MENU_DEF menu = M::Submenu( wxS( "&Test" ),
                                { M::Item( wxS( "&Undo" ), ACTIONS::undo ), M::Item( wxS( "&Redo" ), ACTIONS::redo )
                                                                                    .When(
                                                                                            [&]()
                                                                                            {
                                                                                                return isOn;
                                                                                            } ) } );

    std::unique_ptr<ACTION_MENU> off( BuildActionMenu( menu, nullptr ) );
    BOOST_CHECK_EQUAL( off->GetMenuItemCount(), 1u );

    isOn = true;

    std::unique_ptr<ACTION_MENU> on( BuildActionMenu( menu, nullptr ) );
    BOOST_CHECK_EQUAL( on->GetMenuItemCount(), 2u );

    // The tree still has the entry: the key-sequence engine and the tests see it either way
    BOOST_CHECK( menu.Contains( ACTIONS::redo ) );
}


BOOST_AUTO_TEST_CASE( AKeyOnlyEntryIsInTheTreeAndInNoMenu )
{
    using M = MENU_DEF;

    // The tree: what the engine and the tests read
    MENU_DEF bar = M::Submenu(
            wxEmptyString,
            { M::Submenu( wxS( "&Test" ),
                          { M::Item( wxS( "&Undo" ), ACTIONS::undo ),
                            M::Item( wxS( "&Redo" ), ACTIONS::redo ).KeyOnly(),
                            M::Submenu( wxS( "&Hidden" ), { M::Item( wxS( "&Cut" ), ACTIONS::cut ) } ).KeyOnly(),
                            M::Item( wxS( "Zoom &In" ), ACTIONS::zoomInCenter ).ByKey( ACTIONS::zoomIn ) } ),
              M::Submenu( wxS( "&Popup" ), { M::Item( wxS( "&Find" ), ACTIONS::find ) } ).KeyOnly() } );

    BOOST_CHECK( bar.Contains( ACTIONS::redo ) );
    BOOST_CHECK( bar.Contains( ACTIONS::cut ) );
    BOOST_CHECK( bar.Contains( ACTIONS::find ) );

    // A key action is in the tree beside the menu's own, and one that is not in it is not
    BOOST_CHECK( bar.Contains( ACTIONS::zoomInCenter ) );
    BOOST_CHECK( bar.Contains( ACTIONS::zoomIn ) );
    BOOST_CHECK( !bar.Contains( ACTIONS::zoomOut ) );

    BOOST_CHECK( bar.m_children[0].m_children[3].KeyAction() == &ACTIONS::zoomIn );
    BOOST_CHECK( bar.m_children[0].m_children[0].KeyAction() == &ACTIONS::undo );
    BOOST_CHECK( !bar.m_children[0].m_children[0].m_isKeyOnly );
    BOOST_CHECK( bar.m_children[0].m_children[1].m_isKeyOnly );

    // The mnemonic of a key-only entry counts for duplicates, so the engine never has two ways to a letter
    MENU_DEF twin = M::Submenu(
            wxEmptyString, { M::Submenu( wxS( "&Test" ), { M::Item( wxS( "&Undo" ), ACTIONS::undo ),
                                                           M::Item( wxS( "&Under" ), ACTIONS::redo ).KeyOnly() } ) } );
    BOOST_CHECK_EQUAL( twin.DuplicateMnemonics().size(), 1u );

    // An action the key sequence runs must be registered too
    UNREGISTERED_ACTION unregistered;
    MENU_DEF            faulty = M::Submenu(
            wxEmptyString,
            { M::Submenu( wxS( "&Test" ), { M::Item( wxS( "&Undo" ), ACTIONS::undo ).ByKey( unregistered ) } ) } );

    std::vector<wxString> messages = faulty.UnresolvedActions( ACTION_MANAGER::GetActionList() );

    BOOST_REQUIRE_EQUAL( messages.size(), 1u );
    BOOST_CHECK( messages[0].Contains( wxS( "key sequence" ) ) );

    // The menus leave it out
    KI_TEST::DISPLAY_FOR_WIDGETS display;

    if( !display.IsAvailable() )
    {
        BOOST_TEST_MESSAGE( "Skipping the menus of the test - no display available" );
        return;
    }

    ACTION_MANAGER actionManager( nullptr );

    std::unique_ptr<ACTION_MENU> menu( BuildActionMenu( bar.m_children[0], nullptr ) );

    BOOST_CHECK_EQUAL( menu->GetMenuItemCount(), 2u ); // Undo and Zoom In
    BOOST_CHECK( menu->FindItem( ACTIONS::redo.GetUIId() ) == nullptr );

    std::unique_ptr<CONDITIONAL_MENU> conditional( BuildConditionalMenu( bar.m_children[0], nullptr ) );
    SELECTION                         selection;

    conditional->Evaluate( selection );
    BOOST_CHECK_EQUAL( conditional->GetMenuItemCount(), 2u );
}


BOOST_AUTO_TEST_CASE( TheConditionalMenuKeepsEachEntrysCondition )
{
    KI_TEST::DISPLAY_FOR_WIDGETS display;

    if( !display.IsAvailable() )
    {
        BOOST_TEST_MESSAGE( "Skipping test - no display available" );
        return;
    }

    ACTION_MANAGER actionManager( nullptr ); // gives every action its id, which a menu entry needs

    using M = MENU_DEF;

    bool isRedoShown = false;

    MENU_DEF tree = M::Submenu( wxS( "&Test" ),
                                { M::Item( wxS( "&Undo" ), ACTIONS::undo ),
                                  M::Item( wxS( "&Redo" ), ACTIONS::redo,
                                           [&]( const SELECTION& )
                                           {
                                               return isRedoShown;
                                           } ),
                                  M::Separator(),
                                  M::Submenu( wxS( "&More" ),
                                              []( const SELECTION& )
                                              {
                                                  return false;
                                              },
                                              { M::Item( wxS( "Cu&t" ), ACTIONS::cut ) } ),
                                  M::Submenu( wxS( "&Other" ), { M::CheckItem( wxS( "&Copy" ), ACTIONS::copy ) } ) } );

    std::unique_ptr<CONDITIONAL_MENU> menu( BuildConditionalMenu( tree, nullptr ) );
    SELECTION                         selection;

    menu->Evaluate( selection );

    // Undo, the separator and the submenu Other: Redo and More fail their conditions. A conditional menu shows each
    // action under the action's own label, so the entries are told apart by their ids.
    BOOST_REQUIRE_EQUAL( menu->GetMenuItemCount(), 3u );
    BOOST_CHECK_EQUAL( menu->FindItemByPosition( 0 )->GetId(), ACTIONS::undo.GetUIId() );
    BOOST_CHECK( menu->FindItemByPosition( 1 )->IsSeparator() );
    BOOST_CHECK( menu->FindItemByPosition( 2 )->IsSubMenu() );
    BOOST_CHECK_EQUAL( menu->FindItemByPosition( 2 )->GetItemLabel(), wxS( "&Other" ) );

    isRedoShown = true;
    menu->Evaluate( selection );

    BOOST_REQUIRE_EQUAL( menu->GetMenuItemCount(), 4u );
    BOOST_CHECK_EQUAL( menu->FindItemByPosition( 0 )->GetId(), ACTIONS::undo.GetUIId() );
    BOOST_CHECK_EQUAL( menu->FindItemByPosition( 1 )->GetId(), ACTIONS::redo.GetUIId() );
    BOOST_CHECK( menu->FindItemByPosition( 2 )->IsSeparator() );
    BOOST_CHECK_EQUAL( menu->FindItemByPosition( 3 )->GetItemLabel(), wxS( "&Other" ) );
}


BOOST_AUTO_TEST_SUITE_END()

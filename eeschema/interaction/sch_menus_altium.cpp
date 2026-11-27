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

#include "sch_menus_altium.h"

#include <memory>

#include <advanced_config.h>
#include <bitmaps.h>
#include <file_history.h>
#include <key_sequence.h>
#include <kiface_base.h>
#include <pgm_base.h>
#include <sch_edit_frame.h>
#include <settings/common_settings.h>
#include <tool/action_menu.h>
#include <tool/actions.h>
#include <tools/sch_actions.h>
#include <widgets/panel_remote_symbol.h>
#include <widgets/wx_menubar.h>
#include <wx/translation.h>


// On macOS, wxWidgets moves a Preferences item into the application menu, and the bar has two (Tools and Options). The
// one at the end of Tools is kept out of the menus there, with its separator, so the one moved is the one in Options
// and Tools does not end in a separator. It stays in the tree as a key-only entry, so `T` `P` works on every platform.
static bool isNotMacOS()
{
#ifdef __WXMAC__
    return false;
#else
    return true;
#endif
}


static MENU_DEF keyOnlyOnMacOS( const MENU_DEF& aEntry )
{
#ifdef __WXMAC__
    return aEntry.KeyOnly();
#else
    return aEntry;
#endif
}


MENU_DEF SchAltiumMenuBar()
{
    using M = MENU_DEF;

    // The entries follow the interaction-profile note, 6.1: the letter of an entry is the letter that follows the
    // menu's own in the note's key sequence, so the key-sequence engine can read this tree.

    M file = M::Submenu(
            _( "&File" ),
            { M::Item( _( "&New..." ), ACTIONS::doNew )
                      .When(
                              []()
                              {
                                  return Kiface().IsSingle();
                              } ),
              M::Item( _( "&Open..." ), ACTIONS::open )
                      .When(
                              []()
                              {
                                  return Kiface().IsSingle();
                              } ),
              M::Slot( _( "Open &Recent" ), SCH_MENU_SLOT::OPEN_RECENT ),
              M::Separator().When(
                      []()
                      {
                          return Kiface().IsSingle();
                      } ),
              M::Item( _( "&Save" ), ACTIONS::save ),
              M::Item( _( "Save &As..." ), ACTIONS::saveAs )
                      .When(
                              []()
                              {
                                  return Kiface().IsSingle();
                              } ),
              M::Item( _( "Save &Copy As..." ), SCH_ACTIONS::saveCurrSheetCopyAs )
                      .When(
                              []()
                              {
                                  return !Kiface().IsSingle();
                              } ),
              M::Item( _( "Re&vert" ), ACTIONS::revert ), M::Separator(),
              M::Submenu( _( "&Import" ),
                          { M::Item( _( "&Non-KiCad Schematic..." ), SCH_ACTIONS::importNonKicadSchematic ),
                            M::Item( _( "&Footprint Assignments..." ), SCH_ACTIONS::importFPAssignments ),
                            M::Item( _( "&Graphics..." ), SCH_ACTIONS::importGraphics ) } ),
              M::Submenu( _( "&Export" ),
                          { M::Item( _( "&Drawing to Clipboard" ), SCH_ACTIONS::drawSheetOnClipboard ),
                            M::Item( _( "&Symbols..." ), SCH_ACTIONS::exportSymbolsToLibrary ),
                            M::Item( _( "For &Older KiCad Version..." ), ACTIONS::exportForOlderVersion ) } ),
              M::Separator(), M::Item( _( "Co&mpare with File..." ), SCH_ACTIONS::compareSchematicWithFile ),
              M::Item( _( "Compare with Histor&y..." ), SCH_ACTIONS::compareSchematicWithHistory ), M::Separator(),
              M::Item( _( "&Print..." ), ACTIONS::print ), M::Item( _( "P&lot..." ), ACTIONS::plot ), M::Separator(),
              M::Slot( _( "Quit" ), SCH_MENU_SLOT::EXIT ) } );

    M edit = M::Submenu(
            _( "&Edit" ),
            { M::Item( _( "&Undo" ), ACTIONS::undo ),
              M::Item( _( "&Redo" ), ACTIONS::redo ),
              M::Separator(),
              M::Item( _( "Cu&t" ), ACTIONS::cut ),
              M::Item( _( "&Copy" ), ACTIONS::copy ),
              M::Item( _( "Copy as T&ext" ), ACTIONS::copyAsText ),
              M::Item( _( "&Paste" ), ACTIONS::paste ),
              M::Item( _( "Paste Specia&l..." ), ACTIONS::pasteSpecial ),
              M::Item( _( "Delete Selecti&on" ), ACTIONS::doDelete ),
              M::Separator(),
              M::Item( _( "&Delete" ), ACTIONS::deleteTool ),
              M::Item( _( "Break &Wire" ), SCH_ACTIONS::breakWire ),
              M::Separator(),
              M::Submenu( _( "&Move" ),
                          { M::Item( _( "&Move" ), SCH_ACTIONS::move ), M::Item( _( "&Drag" ), SCH_ACTIONS::drag ),
                            M::Item( _( "Move &Selection" ), SCH_ACTIONS::move ).KeyOnly(),
                            M::Item( _( "D&rag Selection" ), SCH_ACTIONS::drag ).KeyOnly(),
                            M::Item( _( "Rotat&e Selection" ), SCH_ACTIONS::rotateCCW ),
                            M::Item( _( "Rotate C&lockwise" ), SCH_ACTIONS::rotateCW ) } ),
              M::Submenu( _( "&Select" ), { M::Item( _( "&All" ), ACTIONS::selectAll ),
                                            M::Item( _( "&Connection" ), SCH_ACTIONS::selectConnection ),
                                            M::Item( _( "&Inside Area" ), ACTIONS::selectSetRect ),
                                            M::Item( _( "Lasso S&elect" ), ACTIONS::selectSetLasso ) } ),
              M::Submenu( _( "DeSelect (&X)" ), { M::Item( _( "&All" ), ACTIONS::unselectAll ) } ),
              M::Submenu( _( "&Align" ), { M::Item( _( "&Left" ), SCH_ACTIONS::alignLeft ),
                                           M::Item( _( "&Right" ), SCH_ACTIONS::alignRight ),
                                           M::Item( _( "&Top" ), SCH_ACTIONS::alignTop ),
                                           M::Item( _( "&Bottom" ), SCH_ACTIONS::alignBottom ),
                                           M::Item( _( "Horizontal &Centers" ), SCH_ACTIONS::alignCenterX ),
                                           M::Item( _( "&Vertical Centers" ), SCH_ACTIONS::alignCenterY ),
                                           M::Item( _( "To &Grid" ), SCH_ACTIONS::alignToGrid ) } ),
              M::Submenu( _( "&Jump" ), { M::Item( _( "To &Component..." ), ACTIONS::find ) } ),
              M::Separator(),
              M::Item( _( "&Find..." ), ACTIONS::find ),
              M::Item( _( "F&ind and Replace..." ), ACTIONS::findAndReplace ),
              M::Separator(),
              M::Item( _( "Text and &Graphics Properties..." ), SCH_ACTIONS::editTextAndGraphics ),
              M::Item( _( "Change S&ymbols..." ), SCH_ACTIONS::changeSymbols ),
              M::Item( _( "Page &Number..." ), SCH_ACTIONS::editPageNumber ),
              M::Submenu( _( "Attri&butes" ),
                          { M::CheckItem( _( "Exclude from &Simulation" ), SCH_ACTIONS::setExcludeFromSim ),
                            M::CheckItem( _( "Exclude from &BOM" ), SCH_ACTIONS::setExcludeFromBOM ),
                            M::CheckItem( _( "Exclude from Boa&rd" ), SCH_ACTIONS::setExcludeFromBoard ),
                            M::CheckItem( _( "Exclude from &Position Files" ), SCH_ACTIONS::setExcludeFromPosFiles ),
                            M::CheckItem( _( "&Do Not Populate" ), SCH_ACTIONS::setDNP ) } ) } );

    M view = M::Submenu(
            _( "&View" ),
            { M::Item( _( "Fit &Document" ), ACTIONS::zoomFitScreen ),
              M::Item( _( "&Fit All Objects" ), ACTIONS::zoomFitObjects ),
              M::Item( _( "Zoom &Area" ), ACTIONS::zoomTool ),
              M::Item( _( "S&elected Objects" ), ACTIONS::zoomFitSelection ),
              M::Item( _( "Zoom &In" ), ACTIONS::zoomInCenter ).ByKey( ACTIONS::zoomIn ),
              M::Item( _( "Zoom &Out" ), ACTIONS::zoomOutCenter ).ByKey( ACTIONS::zoomOut ),
              M::Item( _( "&Zoom Last" ), ACTIONS::zoomUndo ), M::Item( _( "&Redraw" ), ACTIONS::zoomRedraw ),
              M::Separator(),
              M::Submenu( _( "&Grids" ), { M::Item( _( "Next &Grid" ), ACTIONS::gridNext ),
                                           M::Item( _( "&Reverse Grid" ), ACTIONS::gridPrev ),
                                           M::Item( _( "Grid &Visible" ), ACTIONS::toggleGrid ),
                                           M::Item( _( "&Set Grid..." ), ACTIONS::gridProperties ) } ),
              M::Item( _( "Toggle &Units" ), ACTIONS::toggleUnits ), M::Separator(),
              M::Submenu( _( "&Panels" ),
                          { M::CheckItem( _( "&Properties" ), ACTIONS::showProperties ),
                            M::CheckItem( _( "&Git Commit" ), ACTIONS::showGitCommitPane ),
                            M::CheckItem( _( "&Search" ), ACTIONS::showSearch ),
                            M::CheckItem( _( "&Hierarchy" ), SCH_ACTIONS::showHierarchy ),
                            M::CheckItem( _( "&Net Navigator" ), SCH_ACTIONS::showNetNavigator )
                                    .When(
                                            []()
                                            {
                                                return ADVANCED_CFG::GetCfg().m_IncrementalConnectivity;
                                            } ),
                            M::CheckItem( _( "Design &Blocks" ), SCH_ACTIONS::showDesignBlockPanel ),
                            M::CheckItem( _( "&Remote Symbols" ), SCH_ACTIONS::showRemoteSymbolPanel ) } ),
              M::Submenu( _( "&Navigate" ), { M::Item( _( "&Back" ), SCH_ACTIONS::navigateBack ),
                                              M::Item( _( "&Forward" ), SCH_ACTIONS::navigateForward ),
                                              M::Item( _( "&Up" ), SCH_ACTIONS::navigateUp ),
                                              M::Item( _( "&Previous" ), SCH_ACTIONS::navigatePrevious ),
                                              M::Item( _( "&Next" ), SCH_ACTIONS::navigateNext ) } ),
              M::Submenu( _( "Disp&lay" ),
                          { M::CheckItem( _( "Hidden &Pins" ), SCH_ACTIONS::toggleHiddenPins ),
                            M::CheckItem( _( "Hidden &Fields" ), SCH_ACTIONS::toggleHiddenFields ),
                            M::CheckItem( _( "&Directive Labels" ), SCH_ACTIONS::toggleDirectiveLabels ),
                            M::CheckItem( _( "ERC &Errors" ), SCH_ACTIONS::toggleERCErrors ),
                            M::CheckItem( _( "ERC &Warnings" ), SCH_ACTIONS::toggleERCWarnings ),
                            M::CheckItem( _( "ERC E&xclusions" ), SCH_ACTIONS::toggleERCExclusions ),
                            M::CheckItem( _( "&Simulation Exclusions" ), SCH_ACTIONS::markSimExclusions ),
                            M::CheckItem( _( "OP &Voltages" ), SCH_ACTIONS::toggleOPVoltages ),
                            M::CheckItem( _( "OP &Currents" ), SCH_ACTIONS::toggleOPCurrents ),
                            M::CheckItem( _( "Pin &Alternate Icons" ), SCH_ACTIONS::togglePinAltIcons ) } ) } );

    M project = M::Submenu( _( "Proje&ct" ), { M::Item( _( "&Compile (Run ERC)..." ), SCH_ACTIONS::runERC ),
                                               M::Item( _( "Project &Options..." ), SCH_ACTIONS::schematicSetup ),
                                               M::Separator(), M::Item( _( "&Next Marker" ), ACTIONS::nextMarker ),
                                               M::Item( _( "&Previous Marker" ), ACTIONS::prevMarker ),
                                               M::Item( _( "&Exclude Marker" ), ACTIONS::excludeMarker ) } );

    M place = M::Submenu(
            _( "&Place" ),
            { M::Item( _( "&Part..." ), SCH_ACTIONS::placeSymbol ),
              M::Item( _( "&Wire" ), SCH_ACTIONS::drawWire ),
              M::Item( _( "&Bus" ), SCH_ACTIONS::drawBus ),
              M::Item( _( "B&us Entry" ), SCH_ACTIONS::placeBusWireEntry ),
              M::Item( _( "&Net Label" ), SCH_ACTIONS::placeLabel ),
              M::Item( _( "P&ower Port" ), SCH_ACTIONS::placePower ),
              M::Item( _( "Hierarchical Po&rt" ), SCH_ACTIONS::placeHierLabel ),
              M::Item( _( "Off-Sheet &Connector" ), SCH_ACTIONS::placeGlobalLabel ),
              M::Item( _( "&Junction" ), SCH_ACTIONS::placeJunction ),
              M::Submenu( _( "Directi&ves" ),
                          { M::Item( _( "&No Connect Flag" ), SCH_ACTIONS::placeNoConnect ),
                            M::Item( _( "Para&meter Set (Directive Label)" ), SCH_ACTIONS::placeClassLabel ),
                            M::Item( _( "Ru&le Area" ), SCH_ACTIONS::drawRuleArea ) } ),
              M::Separator(),
              M::Item( _( "&Sheet Symbol" ), SCH_ACTIONS::drawSheet ),
              M::Item( _( "Sheet &Entry" ), SCH_ACTIONS::placeSheetPin ),
              M::Item( _( "S&ynchronize Sheet Pins" ), SCH_ACTIONS::syncAllSheetsPins ),
              M::Item( _( "&Import Sheet..." ), SCH_ACTIONS::importSheet ),
              M::Separator(),
              M::Item( _( "&Text String" ), SCH_ACTIONS::placeSchematicText ),
              M::Item( _( "Text &Frame" ), SCH_ACTIONS::drawTextBox ),
              M::Item( _( "T&able" ), SCH_ACTIONS::drawTable ),
              M::Submenu( _( "&Drawing Tools" ), { M::Item( _( "&Line" ), SCH_ACTIONS::drawLines ),
                                                   M::Item( _( "&Rectangle" ), SCH_ACTIONS::drawRectangle ),
                                                   M::Item( _( "&Arc" ), SCH_ACTIONS::drawArc ),
                                                   M::Item( _( "F&ull Circle" ), SCH_ACTIONS::drawCircle ),
                                                   M::Item( _( "&Ellipse" ), SCH_ACTIONS::drawEllipse ),
                                                   M::Item( _( "Ellipse Ar&c" ), SCH_ACTIONS::drawEllipseArc ),
                                                   M::Item( _( "Pol&ygon" ), SCH_ACTIONS::drawPolygon ),
                                                   M::Item( _( "&Bezier" ), SCH_ACTIONS::drawBezier ),
                                                   M::Item( _( "&Graphic..." ), SCH_ACTIONS::placeImage ) } ) } );

    M design = M::Submenu( _( "&Design" ),
                           { M::Item( _( "&Update PCB Document" ), ACTIONS::updatePcbFromSchematic ),
                             M::Item( _( "Update &Schematic from PCB..." ), ACTIONS::updateSchematicFromPcb ),
                             M::Separator(), M::Item( _( "&Browse Library..." ), ACTIONS::showSymbolBrowser ),
                             M::Item( _( "&Netlist..." ), SCH_ACTIONS::exportNetlist ) } );

    M tools = M::Submenu(
            _( "&Tools" ),
            { M::Submenu( _( "&Annotate" ),
                          { M::Item( _( "&Annotate Schematics..." ), SCH_ACTIONS::annotate ),
                            M::Item( _( "&Increment Annotations..." ), SCH_ACTIONS::incrementAnnotations ) } ),
              M::Item( _( "Footprint Mana&ger..." ), SCH_ACTIONS::assignFootprints ),
              M::Item( _( "Symbol Fields Table (Pa&rameter Manager)..." ), SCH_ACTIONS::editSymbolFields ),
              M::Item( _( "Update From &Libraries..." ), SCH_ACTIONS::updateSymbols ),
              M::Item( _( "Symbol Library Lin&ks..." ), SCH_ACTIONS::editSymbolLibraryLinks ),
              M::Item( _( "Rescue S&ymbols..." ), SCH_ACTIONS::rescueSymbols ),
              M::Item( _( "Re&map Symbols..." ), SCH_ACTIONS::remapSymbols ),
              M::Item( _( "Create &Net Chain..." ), SCH_ACTIONS::createNetChain ),
              M::Submenu( _( "&Variants" ),
                          { M::Item( _( "&Add..." ), SCH_ACTIONS::addVariant ),
                            M::Item( _( "&Remove..." ), SCH_ACTIONS::removeVariant ),
                            M::Item( _( "&Edit Description..." ), SCH_ACTIONS::editVariantDescription ),
                            M::Item( _( "Re&name..." ), SCH_ACTIONS::renameVariant ),
                            M::Item( _( "&Copy..." ), SCH_ACTIONS::copyVariant ) } ),
              M::Separator(),
              M::Item( _( "&Cross Probe to PCB" ), SCH_ACTIONS::selectOnPCB ),
              M::Item( _( "&Simulator..." ), SCH_ACTIONS::showSimulator ),
              M::Separator(),
              M::Item( _( "Symbol &Editor" ), ACTIONS::showSymbolEditor ),
              M::Item( _( "PCB Edi&tor" ), SCH_ACTIONS::showPcbNew ),
              M::Item( _( "Pro&ject Manager" ), ACTIONS::showProjectManager )
                      .When(
                              []()
                              {
                                  return !Kiface().IsSingle();
                              } ),
              M::Item( _( "Calc&ulator Tools" ), ACTIONS::showCalculatorTools ),
              M::Slot( _( "External Plugins" ), SCH_MENU_SLOT::PLUGINS ),
              M::Separator().When( isNotMacOS ),
              keyOnlyOnMacOS( M::Item( _( "&Preferences..." ), ACTIONS::openPreferences ) ) } );

    M reports = M::Submenu( _( "&Reports" ),
                            { M::Item( _( "B&ill of Materials..." ), SCH_ACTIONS::generateBOM ),
                              M::Item( _( "&Legacy Bill of Materials..." ), SCH_ACTIONS::generateBOMLegacy ),
                              M::Separator(), M::Item( _( "&Measure Distance" ), ACTIONS::measureTool ),
                              M::Item( _( "Symbol Dif&f..." ), SCH_ACTIONS::diffSymbol ),
                              M::Item( _( "Bus S&yntax Help" ), SCH_ACTIONS::showBusSyntaxHelp ) } );

    M options = M::Submenu( _( "&Options" ),
                            { M::Item( _( "Page &Settings..." ), ACTIONS::pageSettings ),
                              M::Item( _( "&Preferences..." ), ACTIONS::openPreferences ),
                              M::Item( _( "&Grids..." ), ACTIONS::gridProperties ),
                              M::Item( _( "&Document Options..." ), ACTIONS::pageSettings ).KeyOnly(), M::Separator(),
                              M::Item( _( "Configure Pa&ths..." ), ACTIONS::configurePaths ),
                              M::Item( _( "Symbol &Libraries..." ), ACTIONS::showSymbolLibTable ),
                              M::Item( _( "Design &Block Libraries..." ), ACTIONS::showDesignBlockLibTable ),
                              M::Slot( _( "Language" ), SCH_MENU_SLOT::LANGUAGES ) } );

    // The zoom popup of the key sequences (Z): the same entries as View, to be typed with the cursor where the zoom is
    M zoom = M::Submenu( _( "&Zoom" ),
                         { M::Item( _( "&Window" ), ACTIONS::zoomTool ), M::Item( _( "Zoom &In" ), ACTIONS::zoomIn ),
                           M::Item( _( "Zoom &Out" ), ACTIONS::zoomOut ),
                           M::Item( _( "Selected Objects (&E)" ), ACTIONS::zoomFitSelection ),
                           M::Item( _( "&Last" ), ACTIONS::zoomUndo ),
                           M::Item( _( "&All Objects" ), ACTIONS::zoomFitObjects ),
                           M::Item( _( "&Sheet" ), ACTIONS::zoomFitScreen ) } )
                     .KeyOnly();

    return M::Submenu( wxEmptyString, { file, edit, view, project, place, design, tools, reports, options, zoom } );
}


const std::vector<MENU_OMISSION>& SchAltiumMenuOmissions()
{
    static const std::vector<MENU_OMISSION> omissions = {
        { "E N, Shift+F", nullptr, "Find Similar Objects: the schematic has no such command" },
        { "T H", nullptr, "Up/Down Hierarchy: a new action, built with the sequence engine" },
    };

    return omissions;
}


WX_MENUBAR* SCH_EDIT_FRAME::createInteractionProfileMenuBar( TOOL_INTERACTIVE* aTool )
{
    // Open Recent is made once and cloned into each new bar, as the stock bar does
    static ACTION_MENU* openRecentMenu = nullptr;

    MENU_SLOT_FILLER fillSlot = [&]( const std::string& aSlot, const MENU_DEF& aNode, ACTION_MENU& aParent )
    {
        if( aSlot == SCH_MENU_SLOT::OPEN_RECENT )
        {
            if( !Kiface().IsSingle() )
                return;

            FILE_HISTORY& fileHistory = GetFileHistory();

            if( !openRecentMenu )
            {
                openRecentMenu = new ACTION_MENU( false, aTool );
                openRecentMenu->SetIcon( BITMAPS::recent );

                fileHistory.UseMenu( openRecentMenu );
                fileHistory.AddFilesToMenu( openRecentMenu );
            }

            openRecentMenu->SetTitle( aNode.m_label );
            fileHistory.UpdateClearText( openRecentMenu, _( "Clear Recent Files" ) );

            wxMenuItem*       item = aParent.Add( openRecentMenu->Clone() );
            ACTION_CONDITIONS cond;
            cond.Enable( FILE_HISTORY::FileHistoryNotEmpty( fileHistory ) );
            RegisterUIUpdateHandler( item->GetId(), cond );
        }
        else if( aSlot == SCH_MENU_SLOT::EXIT )
        {
            aParent.AddQuitOrClose( &Kiface(), _( "Schematic Editor" ) );
        }
        else if( aSlot == SCH_MENU_SLOT::PLUGINS )
        {
            ACTION_MENU* plugins = new ACTION_MENU( false, aTool );
            plugins->SetTitle( aNode.m_label );
            plugins->SetIcon( BITMAPS::puzzle_piece );

            if( Pgm().GetCommonSettings()->m_Api.enable_server && AddApiPluginMenuItems( plugins ) > 0 )
                plugins->AppendSeparator();

            plugins->Add( ACTIONS::pluginsReload );
            plugins->Add( ACTIONS::pluginsShowFolder );
            aParent.Add( plugins );
        }
        else if( aSlot == SCH_MENU_SLOT::LANGUAGES )
        {
            AddMenuLanguageList( &aParent, aTool );
        }
    };

    WX_MENUBAR* menuBar = BuildMenuBar( SchAltiumMenuBar(), aTool, fillSlot );

    // Entries that exist under a project manager only, or that depend on a setting read when the bar is shown
    if( wxMenuItem* item = menuBar->FindItem( ACTIONS::updatePcbFromSchematic.GetUIId() ) )
        item->Enable( !Kiface().IsSingle() );

    if( wxMenuItem* item = menuBar->FindItem( ACTIONS::updateSchematicFromPcb.GetUIId() ) )
        item->Enable( !Kiface().IsSingle() );

    if( wxMenuItem* item = menuBar->FindItem( SCH_ACTIONS::compareSchematicWithHistory.GetUIId() ) )
    {
        ACTION_CONDITIONS historyCond;
        historyCond.Enable(
                []( const SELECTION& )
                {
                    COMMON_SETTINGS* cs = Pgm().GetCommonSettings();
                    return cs && cs->AutosaveUsesLocalHistory();
                } );
        RegisterUIUpdateHandler( item->GetId(), historyCond );
    }

    if( wxMenuItem* item = menuBar->FindItem( SCH_ACTIONS::showRemoteSymbolPanel.GetUIId() );
        item && m_remoteSymbolPane && !m_remoteSymbolPane->HasDataSources() )
    {
        item->Enable( false );
        item->SetHelp( _( "Search signed-in remote symbol providers and download verified libraries." ) );
    }

    AddStandardHelpMenu( menuBar );

    return menuBar;
}


const std::vector<std::pair<const TOOL_ACTION*, wxUniChar>>& SchAltiumSequenceRoots()
{
    static const std::vector<std::pair<const TOOL_ACTION*, wxUniChar>> roots = {
        { &ACTIONS::sequencePlace, 'P' },   { &ACTIONS::sequenceEdit, 'E' },     { &ACTIONS::sequenceMove, 'M' },
        { &ACTIONS::sequenceSelect, 'S' },  { &ACTIONS::sequenceDeselect, 'X' }, { &ACTIONS::sequenceAlign, 'A' },
        { &ACTIONS::sequenceJump, 'J' },    { &ACTIONS::sequenceView, 'V' },     { &ACTIONS::sequenceProject, 'C' },
        { &ACTIONS::sequenceDesign, 'D' },  { &ACTIONS::sequenceTools, 'T' },    { &ACTIONS::sequenceReports, 'R' },
        { &ACTIONS::sequenceOptions, 'O' }, { &ACTIONS::sequenceFile, 'F' },     { &SCH_ACTIONS::sequenceZoom, 'Z' }
    };

    return roots;
}


bool SCH_EDIT_FRAME::GetKeySequenceSpec( KEY_SEQUENCE_SPEC& aSpec ) const
{
    aSpec.m_tree = std::make_shared<const MENU_DEF>( SchAltiumMenuBar() );
    aSpec.m_roots = SchAltiumSequenceRoots();
    return true;
}

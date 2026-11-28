# Interaction profile: Altium-style menus, keys and views on KiCad's functions

Status: design note, nothing implemented. Written 2026-11-26 against `reckless/main` at `8bc6620158`.
Audience: the agent (or person) who implements it. Read `RECKLESS.md` first; every rule there applies.

## 1. Goal

A long-time Altium Designer (and Protel) user should be able to sit down at this build and work from finger memory:
the same menu grouping, the same two- and three-key sequences (`P` `W` places a wire, `P` `T` routes a track,
`T` `G` `A` refills all pours), the same keys inside a command (`Space` rotates, `Shift+Space` changes corner style,
`Tab` edits what is being placed, `Backspace` takes back the last segment), the same way of switching layers and of
looking at a board (layer tabs, a transparent 2D stack, single-layer mode, `2` and `3` for 2D and 3D).

KiCad keeps doing the work. We change how the user reaches a function, not the function. Where KiCad does the same
job in a slightly different way, we map to it and say so; we don't re-implement Altium's engine.

Priorities follow how often the user does a thing, most frequent first:

1. Schematic: place parts, wire them, edit designators and values, drag parts and wires with connections kept, place
   and edit net labels, power ports and ports. Buses now and then. One sheet template, one sheet size.
2. PCB: 2- and 4-layer boards, mostly through-hole, routed entirely by hand. Interactive routing, routing modes,
   layer changes with automatic vias. Views: which layers show, how transparent each kind of object is, how pours
   draw, single-layer mode.
3. 3D view of the board and 3D models on footprints, which the user is doing more of.

### Non-goals

- No Altium file-format, project-model or library-model work here. The importer is separate work.
- No copy of Altium's artwork, icons, splash, help text or dialog designs. We reproduce functional conventions only:
  where a command sits in a menu tree, which key reaches it, what a key does inside a command. Icons stay KiCad's.
  Don't paste Altium resource files, screenshots or documentation text into this repository.
- No change to stock behavior. With the profile off, the build behaves exactly as upstream KiCad does.
- Features with no KiCad function behind them (ActiveRoute, rooms, xSignals, unions, snippets, harnesses, device
  sheets, variants UI, the Situs autorouter) are left out of the menus, not shown disabled.

## 2. What was studied

- Altium Designer 21.7.2, driven live on a 2-layer through-hole board with a small hierarchical schematic.
- Its menu, popup, toolbar and shortcut definitions, which ship as text resource files beside the program. They
  define every command (`PL` entries: command, caption with mnemonic, up to two shortcuts) and every menu tree
  (`Tree` entries, with the `Popup` letter that opens the tree at the cursor). The tables in section 6 come from
  those definitions, checked against the running program. The in-command routing keys are not in those files; they
  were read from the program's own in-command shortcut list (`Shift+F1` while routing).
- A second live session on 2026-11-27, on a larger two-layer board and its schematic, read the in-command key lists
  of the other commands (moving a component, placing lines and dimensions, wiring, placing labels and ports,
  dragging), the width, via and grid pickers, and the heads-up display; then the right-click menus of both editors
  (6.6), polygon pours, buses, sheet symbols, ports, moving a selection, the Route menu, length tuning and
  differential-pair routing. The in-command lists are in 6.7.
- This tree: the tool framework, both editors, the router and the 3D viewer. File and line references below are to
  `8bc6620158` and will drift; search for the symbol.

## 3. How Altium's interaction works (the parts that matter)

**3.1 Popup keys and sequences.** Almost every unmodified letter opens a menu at the cursor. The letter is the
menu's own mnemonic: `F` File, `E` Edit, `V` View, `C` Project, `P` Place, `D` Design, `T` Tools, `U` Route (PCB),
`R` Reports, `W` Window, `H` Help. More letters open menus that are not on the menu bar: `S` Select, `X` DeSelect,
`M` Move, `A` Align, `J` Jump, `Z` Zoom, `O` Options, `Y` Filter, and on the PCB `G` Snap Grid, `N` Netlist
(connection lines) and `I` Component Placement. The next letter picks an item by its underlined mnemonic, and
submenus nest, so commands are typed as sequences: `P` `W`, `P` `T`, `E` `D`, `M` `S`, `S` `A`, `X` `A`, `V` `F`,
`J` `C`, `T` `G` `A`, `U` `U` `A`, `D` `R`. Users type them without looking; the menu is there for when they
hesitate. This is the single most important thing to reproduce.

**3.2 A few direct keys** sit beside the sequences: `PgUp`/`PgDn` zoom about the cursor, `Home` re-centers on the
cursor, `End` redraws, `Ctrl+PgDn` fits everything, `Ctrl+M` measures, `Shift+C` clears the current filter or
highlight, `Shift+F` finds similar objects, `F11` shows the Properties panel, `Ctrl+R` rubber-stamps (duplicate).
On the PCB: `L` opens View Configuration, `Ctrl+D` its View Options tab, `Q` toggles mm and mil, `G` opens the grid
popup, `Ctrl+G` grid properties, `Shift+E` cycles hotspot snapping, `Shift+S` cycles single-layer mode, `Shift+R`
cycles the routing conflict mode, numpad `+` and `-` step through layers, numpad `*` steps through signal layers,
`Ctrl+F` flips the board, `2` and `3` switch 2D and 3D, `[` and `]` change how far masked objects are dimmed. In the
schematic: `G` and `Shift+G` cycle the snap grid, `Space` rotates the selection, `Ctrl+W` places a wire.

**3.3 In-command keys.** While a command runs (placing, wiring, moving, routing) a second, fixed set of keys
applies. Its core is shared by every command:

| Key | Meaning in every command |
|---|---|
| `Space` / `Shift+Space` | rotate counterclockwise / clockwise what is on the cursor; while wiring or routing, `Space` flips the corner direction and `Shift+Space` cycles the corner style |
| `X` / `Y` | flip (schematic) |
| `Tab` | pause the command and edit the properties of what is being placed or routed, before it is placed |
| `Backspace` | remove the last vertex or the last committed segment |
| `Enter` | same as a left click |
| `Esc` or right-click | back out one level: drop the current wire or route but stay in the command; again to leave the command |
| `Shift+F1` or `` ` `` | show the list of keys this command accepts, as a menu at the cursor |
| arrows, `Shift+`arrows | move the cursor one grid step, ten grid steps |
| `PgUp` `PgDn` `Home` `End` | zoom and pan still work mid-command |

Commands are sticky: after placing a part or finishing a wire the command stays active for the next one.

Around that core each command adds keys of its own, listed in 6.7: `Ins` inserts a vertex while wiring and copies
the attributes of the object under the cursor while placing; moving a component on the PCB has `L`, `X`, `Y`, a
pushing mode and alignment held on `Ctrl` and `Shift`; placing a PCB line has nearly the router's set. And the list
itself has two forms, a menu at the cursor or a modal dialog (key, action, Close), and the form is not fixed per
command: moving one component showed the menu and moving a selection the dialog, with the same entries. A menu at the
cursor everywhere (5.5) is the consistent choice.

**3.4 Routing.** One command (`P` `T`, `U` `T` or `Ctrl+W`). Click a pad or track to start, click to commit
corners. The last segment is drawn hatched and is not committed by the click (look-ahead, toggled by `1`). The full
key list read from the program:

| Key | Routing function |
|---|---|
| `Space` | toggle elbow side |
| `Shift+Space` | cycle corner style (45, 45 with arc, 90, 90 with arc, any angle) |
| `Shift+R` | cycle conflict resolution mode (the user works in HugNPush; others are Ignore, Push, Walkaround, Stop at first obstacle) |
| `Backspace` | undo the last commit |
| `Enter` | commit |
| `Ctrl+Click` | autocomplete the segments to the target |
| `1` | look-ahead mode |
| `Shift+D` | toggle loop removal |
| `Shift+F` | toggle follow mode |
| `Ctrl+W` | show clearance boundaries |
| `Shift+W` / `Shift+V` | choose a favorite width / via size from a list at the cursor |
| `3` / `4` | cycle the source of the track width / via size (user choice, rule minimum, rule preferred, rule maximum) |
| numpad `+` `*` / numpad `-` | next / previous layer, inserting a via |
| `Ctrl+L` | select layer from a list |
| `L` | switch the layer of the trace being drawn |
| `2` | add a via without changing layer |
| `/` | add a fanout via and suspend |
| `6` / `8` | change / select the routing transition (via type) |
| `7` / `9` | next routing target / swap to the opposite route point |
| `Shift+A`, `Shift+G`, `Shift+Ctrl+G`, `Shift+C`, `Shift+T` | accordions, length gauge, glossing effort, subnet swapping |
| `Tab` | pause; the Properties panel shows net, layer, via size, width, conflict mode and corner style, each with its key beside it |

While routing, the heads-up display and the status bar read, for example:
`Track 45:HugNPush Obstacles [Width From: Rule Preferred] [Via-Size From: Rule Preferred][Thru 1:2] Gloss: Strong`,
then net length and segment size. Objects not on the routed net are dimmed, and the layer tabs for non-signal layers
gray out.

**3.5 Views.** The View Configuration panel (`L`) has two tabs. Layers & Colors: a tree of layer groups (signal and
plane, component layer pairs, mechanical, other), an eye and a color swatch per layer, saved layer sets, the active
layer. View Options: 3D on or off, single-layer mode, and an Object Visibility table with a transparency slider per
kind of object (pads, vias, arcs, tracks, polygons, regions, fills, texts, rooms, dimensions, keepouts, 3D bodies),
with an Advanced matrix of layer by object kind. The user's working "transparent 2D" setting, the same on both
copper layers, is:

| Object | Transparency | KiCad opacity |
|---|---|---|
| Pads | 20 % | 0.80 |
| Vias | 0 % | 1.00 |
| Tracks, arcs | 15 % | 0.85 |
| Polygons (pours) | 70 % | 0.30 |
| Regions, fills | 40 % | 0.60 |
| Text, dimensions, keepouts | 0 % | 1.00 |

Along the bottom of the PCB window is a row of layer tabs, one per visible layer, each with its color; a click makes
a layer active, and a swatch at the left end (`LS`) shows the active layer's color and opens the layer sets.
`Shift+S` cycles normal view, other layers grayed, other layers hidden (the two modes the owner has enabled);
multi-layer pads stay visible. A heads-up
display at the top-left of the canvas shows cursor position, delta from the last click, the active layer, the snap
settings and the net and component under the cursor (`Shift+H` hides it, `Insert` zeroes the delta).

**3.6 3D.** `3` redraws the same document in 3D at the same position and zoom, `2` goes back. `0` looks straight
down, `9` rotates 90 degrees, `8` is isometric, `Ctrl+F` flips the board, `Shift`+right-drag orbits, right-drag
pans. Footprints carry 3D bodies: extruded shapes or embedded STEP models, with standoff height and rotation.

**3.7 Window furniture.** Document tabs across the top. Panels dock and auto-hide: Projects on the left,
Properties (and Components) on the right. With nothing selected, Properties shows document options: the selection
filter as a set of toggle buttons, units, grids, snap options. A small "Active Bar" of the most used placement
commands sits at the top center of the canvas. `Tab` and a double-click both land in the Properties panel (the
owner has "double click runs the inspector" on in both editors).

**3.8 Rules from Altium's documentation that the tables do not show.** Altium's online documentation is no longer
versioned, so these describe the current product; where the installed 21.7 resource files disagree, the files win.

- *Keys depend on state.* The same key has a meaning idle, another while routing and another while moving a
  component: `1`/`2`/`3` are view modes idle and look-ahead, via, width source while routing; `Shift+R` is the
  conflict mode while routing and component re-route while moving; `Shift+D` is the heads-up delta idle and loop
  removal while routing; `Shift+G` heads-up tracking and length gauge; `Shift+F` Find Similar and follow mode;
  `Shift+C` clear filter and subnet swap; `Ctrl+W` start routing and clearance boundaries; `Shift+V` violations
  popup and via size. Section 5.5's context aliases exist for this.
- *Placing a part.* `P` `P` opens the component chooser; a click places the part and another of the same part is on
  the cursor at once; right-click or `Esc` leaves. The part is held by its nearest pin end. A designator typed
  through `Tab` on the first part (`R1`) seeds the next ones (`R2`, `R3`). Dropping a part onto a wire cuts the
  wire onto its pins.
- *Wiring.* The segment on the cursor is a look-ahead; a click commits the one before it. `Shift+Space` cycles 90,
  45, any angle and an automatic point-to-point mode. `Insert` adds a vertex. Within the snap distance of a pin
  end or wire end the cursor leaves the grid, jumps to it and shows a marker. Junctions are made automatically.
- *Move and drag.* Move ignores connections, drag keeps them; a mouse drag is whichever "Always Drag" says and
  `Ctrl` gives the other. With "Drag Orthogonal" attached wires stay at 90 degrees and gain corners. During a drag
  `Space` rotates and `Ctrl+Space` flips the corner of the attached wires (this is what the installed key table
  says; one documentation page has them the other way round). `Ctrl`+arrows nudge the selection one grid step,
  `Shift+Ctrl`+arrows ten.
- *Text.* With in-place editing on: click a text, pause, click again, or select it and press `F2`; `Enter` or a
  click elsewhere commits. Two quick clicks are a double-click and open Properties.
- *Labels and ports.* A net label's hot spot is its lower-left corner and must touch a wire or pin end. A trailing
  number increments on every placement. A port takes two clicks: one end, then the length.
- *Selection.* A click replaces the selection, `Shift`+click toggles one object, a click on empty space clears.
  On the PCB `Tab` widens the selection: segment, connected tracks on the layer, all connected copper, the net.
  `Ctrl`+click on a net object highlights the net and dims the rest, `Shift+Ctrl`+click adds another net,
  `Ctrl`+click on empty space clears.
- *Routing.* Clicking the target pad ends that connection and the command stays active. Starting on an existing
  track picks up its width. `,` and `.` change the arc radius in the arc corner styles. Dragging a routed segment
  slides it and its neighbors keep their angles; `Ctrl`+drag moves it alone. Routing a new path that rejoins the
  old one removes the old loop.
- *Moving a component.* `Space` and `Shift+Space` rotate by the rotation step (90 degrees), `L` flips to the other
  side, `Tab` edits it, `Ctrl`+arrows nudge.
- *Polygons.* Right-click or `Esc` closes the outline and finishes; after a reshape the old copper stays drawn
  until repour.
- *3D.* Holding `Shift` shows an orbit sphere at the cursor, which sets the pivot; `Shift`+right-drag then rotates.

## 4. What KiCad gives us to build on

**4.1 Actions and hotkeys.** Every command is a `TOOL_ACTION` (`include/tool/tool_action.h`) with a dotted name, a
scope (`AS_CONTEXT`, `AS_ACTIVE`, `AS_GLOBAL`), a default hotkey and one alternate. Hotkeys resolve in
`ACTION_MANAGER::UpdateHotKeys` / `processHotKey` (`common/tool/action_manager.cpp`; the layering itself is
`ACTION_MANAGER::ResolveHotKey`): compiled default, then legacy
file, then the user's `user.hotkeys` (a tab-separated file: name, key, alternate key; read and written in
`common/hotkeys_basic.cpp`). There is one user set and no notion of a named scheme.

**4.2 Dispatch.** Canvas key events go to `TOOL_DISPATCHER::DispatchWxEvent`, then
`TOOL_MANAGER::processEvent` → `DispatchHotKey` → `ACTION_MANAGER::RunHotKey`. `RunHotKey` looks the key up in
`m_actionHotKeys` (one keycode to a list of actions) and prefers an `AS_CONTEXT` action belonging to the
highest-priority tool on the active stack; failing that a global action. Only keys no hotkey claims reach a tool's
event loop. Two consequences: the same key can mean different things inside different tools (the conflict checker
in `HOTKEY_STORE::CheckKeyConflicts` already allows it), and there is no multi-key sequence support anywhere.

**4.3 Menus.** Each frame builds its menu bar in code (`eeschema/menubar.cpp`, `pcbnew/menubar_pcb_editor.cpp`) from
`ACTION_MENU`s. `TOOL_ACTION::GetMenuItem()` escapes `&`, so action items have no mnemonics; a mnemonic needs the
override label of `ACTION_MENU::Add`. `WX_MENUBAR` installs an empty accelerator table, so on Windows the menu bar
does not steal keys. A popup can be shown from any handler with `PopupMenu` on an `ACTION_MENU`
(`ACTION_TOOLBAR::showContextMenu` does it); selection goes back through `ACTION_MENU::OnMenuEvent` to the tool
manager. Tool context menus go through `TOOL_MANAGER::DispatchContextMenu`. KiCad's own popups use mnemonics on
Windows and GTK and leave them out on macOS, where native menus have none.

**4.4 Toolbars and panels.** Toolbars are declared per frame in `*_TOOLBAR_SETTINGS::DefaultToolbarConfig`
(`TOOLBAR_CONFIGURATION`), customizable by the user. Panels are wxAUI panes; layout is saved per frame. There are
no named layouts.

**4.5 Mouse.** `COMMON_SETTINGS::m_Input` already has what Altium's mouse needs: right-drag pans by default and a
right-click without a drag still opens the context menu; the wheel's zoom and pan modifiers are settings
(`scroll_modifier_zoom`, `scroll_modifier_pan_h`, `scroll_modifier_pan_v`); middle-drag can zoom. Implemented in
`WX_VIEW_CONTROLS`.

**4.6 Settings and build options.** New global preferences go in `COMMON_SETTINGS` (`kicad_common.json`). The fork's
house options are declared in the top `CMakeLists.txt` (`_KICAD_HOUSE_DEFAULT`, ON when `KICAD_VERSION_EXTRA` is
`reckless`) and only choose defaults: `KICAD_HOUSE_THEME` picks the starting color theme by `#ifdef` in
`color_settings.cpp`. Follow that pattern.

**4.7 Schematic editor.** Already close: `X`/`Y` mirror, `Shift+Space` cycles the wire mode (`lineModeNext`),
`Backspace` removes the last segment, a plain mouse drag keeps wires attached (`input.drag_is_move` false), box
selection direction means the same, `E` opens properties of a part still on the cursor, right-drag pans. Different:
`Space` resets local coordinates, rotate is `R`, wire posture is `/`, `Tab` steps through net items, `PgUp`/`PgDn`
change sheet, zoom is `F1`/`F2`. Missing: in-place text editing, auto-increment of labels inside the label tool,
any way to end a command with a right-click.

**4.8 PCB editor and router.** The router (`pcbnew/router/router_tool.cpp`) has the functions but other keys:
posture `/`, corner mode cycle `Ctrl+/`, `CycleRouterMode` (Highlight collisions, Shove, Walk around; no default
key), widths `W`/`Shift+W`, via sizes `'`/`\`, custom size dialog `Q`, undo segment `Backspace`, attempt finish `F`,
route from other end `Ctrl+E`, via `V`, select layer and place via `<`. A layer-change key pressed mid-route
(`+`, `-`, `PgUp`, `PgDn`) arms a through via to that layer automatically (`ROUTER_TOOL::handleLayerSwitch`), which
is Altium's behavior. `fix_all_segments` (default true) is the inverse of Altium's look-ahead. Views: the Appearance
panel has opacity sliders for tracks, vias, pads, zones, filled shapes and images (stored per project in
`.kicad_prl` as `board.opacity.*`), layer presets, viewports, and an inactive-layer mode Normal, Dim, Hide cycled by
`H`, which is single-layer mode under another name. Layers composite with alpha, the active layer on top. Missing:
layer tabs, a heads-up display, a mask-level key, a grid popup, `Ctrl+Shift`+wheel layer change, any single-key
access to the Appearance panel.

**4.9 3D.** The viewer is a separate top-level frame (`EDA_3D_VIEWER_FRAME`, opened by `ACTIONS::show3DViewer`,
`Alt+3`), destroyed on close, so its camera is lost. Nothing raises the PCB editor from it. `EDA_3D_CANVAS` can be
embedded (footprint chooser, 3D model preview). Footprints carry `FP_3DMODEL`s (STEP, VRML and others, embeddable
with `kicad-embed://`), and upstream now has a first-class extruded body, `PCB_3D_BODY` (height, standoff, outline
source, material), edited in the 3D Models tab of the footprint properties. The Altium importer embeds STEP models
but skips Altium's extruded bodies.

## 5. Architecture

Six pieces. The first three carry most of the muscle memory and should land first.

### 5.1 The profile switch

One new global setting, `input.interaction_profile` in `COMMON_SETTINGS`, an enum with two values to start:
`kicad` (stock) and `altium` (this note), shown in Preferences as "KiCad" and "Altium-style". Everything below is conditional on it at run time; with `kicad` no code
path changes. A build option `KICAD_HOUSE_INTERACTION` (ON with the other house options) only makes `altium` the
value a new configuration starts with, exactly as `KICAD_HOUSE_THEME` does for the color theme. A selector goes at
the top of Preferences > Hotkeys ("Interaction profile"), and changing it rebuilds menus, toolbars and hotkeys in
every open frame (the path a language change takes: `ShowChangedLanguage` / `CommonSettingsChanged`).

Put the new code in its own files so a rebase onto upstream touches as little as possible:

```
include/interaction_profile.h                    the enum (`INTERACTION_PROFILE_ID`, `STOCK` and `ALTIUM`), the active profile, the layer
common/interaction/interaction_profile.cpp       its code, in the static `common` library (it needs the key-name parser)
common/interaction/altium.hotkeys                the checked-in profile table, embedded at build time (step 1: three bindings)
common/interaction/key_sequence.{h,cpp}          the sequence state machine (no wx, unit-testable)
common/interaction/menu_def.{h,cpp}              declarative menu trees and their builders
eeschema/interaction/sch_menus_altium.cpp        the schematic trees
pcbnew/interaction/pcb_menus_altium.cpp          the PCB trees
```

Each hook into an upstream file is a short guarded call into these. The active profile is read from the common
settings through `Pgm()`; the static `common` library is linked into every module, so a static there would be one copy
for each. Change is announced with `CommonSettingsChanged( HOTKEYS_CHANGED | INTERACTION_PROFILE_CHANGED )`, and
`TOOLS_HOLDER::InteractionProfileChanged()` is where step 2 rebuilds menus.

The owner chose the name "Altium-style" (section 9). This note says "the profile".

### 5.2 A profile layer in hotkey resolution

`processHotKey` gains a layer: compiled default → legacy file → profile table → user file. The profile table is a checked-in
tab-separated file in the `user.hotkeys` format, one per profile, embedded at build time the way
`reckless_color_theme.json` is, so the hotkey importer's parser reads it. An action the table does not name keeps
its compiled default unless that key is taken by the profile, in which case it is unbound. That rule matters:
the profile takes nearly every bare letter for sequences, so KiCad's single-letter defaults (`A`, `W`, `M`, `E`,
`X`, `V` and the rest) must drop out rather than fight.
A key is taken in the scope of the action that binds it (step 3, #136): a `common.` action takes it in every editor,
and an `eeschema.` or `pcbnew.` action in that editor only, so the two editors' tables can give one key different
meanings. An `AS_CONTEXT` action keeps its stock key where a root takes the letter, because inside its command it wins
(5.3).

The user's own changes are stored per profile: `user.hotkeys` for `kicad`, `user.<profile>.hotkeys` otherwise, so
switching profiles never destroys a customization. For a profile other than `kicad` the editor writes only the actions whose keys differ from
the profile's resolved defaults (compiled, legacy, table), an action the user unbound as an explicit empty binding and
an action set back to its default not at all, so a later change to the profile's table reaches everything the user never
changed; `kicad` keeps upstream's whole-set write. The legacy file is read for the schematic and PCB editors' actions only. In the hotkey editor "Reset to defaults" means the profile's
defaults (`HOTKEY_STORE::ResetAllHotkeysToDefault`), and conflict checks run on the resolved set.

### 5.3 The key-sequence engine

A sequence is a path through a menu tree. The engine is a small state machine owned by the tool manager:

- **Roots.** Each popup letter is a real `TOOL_ACTION` (`common.Sequence.place`, `common.Sequence.edit`, and so on,
  `AS_GLOBAL`) whose profile hotkey is the letter. That keeps the hotkey editor, the conflict checker, labels and
  user remapping working with no special cases, and it lets an `AS_CONTEXT` action of an active tool claim the same
  letter inside a command, which is exactly Altium's rule that in-command keys win.
- **Pending state.** Running a root action enters the pending state at that tree's node. The next key press is
  looked up among the node's children by mnemonic, before normal hotkey dispatch (hook: top of
  `TOOL_MANAGER::DispatchHotKey`). A leaf runs its action and clears the state. A submenu becomes the new node. `Esc`
  clears the state. Any other key clears the state and is then dispatched normally, so a mistyped sequence costs
  nothing. Modifier-only events are ignored.
- **The menu is a hint, not the mechanism.** On entering the pending state, start a short timer (default 300 ms,
  the owner's choice; a preference, 0 to show at once). If the sequence completes first, nothing is drawn: a fluent user never sees a
  menu flash. If the timer fires, show the node's menu at the cursor as a native popup built from the same tree.
  From then on the native menu handles the next letter through its mnemonics (Windows, GTK), and mouse and arrow
  keys work. Because the engine has already consumed the keys typed so far, type-ahead cannot be lost while the
  menu is being mapped, which is the failure a pure native-popup design has on GTK.
- **macOS.** Native popups have no mnemonics. Show the same menu with the letter in a right-hand hint column, and
  have the engine keep reading keys while it is open if wx allows; otherwise use a non-activating popup window
  (the way `HOTKEY_CYCLE_POPUP` is driven from the dispatcher). Settle this by experiment; Linux and Windows first.
- **Inside a command.** Sequences still work mid-command for view commands, as they do in Altium (`V` `F` while
  routing). The engine does not cancel the active tool; whether the leaf action interrupts it is the action's own
  business, as today.
- **Status.** While pending, show the keys so far in the status bar's tool field (`P ›`), so a half-typed sequence
  is visible.

`key_sequence.cpp` holds no wx types: it takes key codes and a tree, and returns "consumed", "run this action",
"show this node" or "not mine". It is tested in `qa_common` without a GUI.

### 5.4 Menus as data

The menu bar and the sequence engine must read the same tree, or they drift. Define a small declarative form:

```cpp
struct MENU_DEF
{
    const char*              label;     // with '&' mnemonic, translatable
    const TOOL_ACTION*       action;    // leaf, or nullptr
    std::vector<MENU_DEF>    children;  // submenu
    SELECTION_CONDITION      show;      // optional
    bool                     separator;
};
```

and two consumers: `BuildActionMenu( const MENU_DEF&, TOOL_INTERACTIVE* )` for the menu bar and popups, and the
sequence engine. Each frame's `doReCreateMenuBar()` starts with: if the profile is active, build from this frame's
tree and return. Label text is the profile's own (so `&` mnemonics are under our control and unique within a menu;
a QA test checks uniqueness and that every action exists), and the hotkey shown beside an item comes from the
action as usual. The sequence is shown too where there is room, the way Altium underlines letters.

The trees for both editors are in section 6. They follow Altium's grouping, keep only items KiCad can do, and add
KiCad-only commands where an Altium user would look for them (for example Net Inspector under Design, Symbol
Fields Table under Tools). KiCad's own Preferences menu becomes `T` `P` (Tools > Preferences) and `O`; Inspect's
items move to Tools and Reports.

**Right-click menus are rebuilt too** (the owner's choice, section 9), from `MENU_DEF` trees like the menu bar.
Today the selection tool owns the idle context menu (`SCH_SELECTION_TOOL::Init`, `PCB_SELECTION_TOOL::Init`) and
other tools append their items to it in their own `Init` (edit, move, align, inspection, wire, router), each with a
`SELECTION_CONDITION`. Leave all of that in place for the `kicad` profile. For this profile build a second
`CONDITIONAL_MENU` per editor from one tree, each node's `show` condition becoming the item's condition, and have
the selection tool show that one instead (choose at the point it calls `ShowContextMenu`, so switching profile
needs no tool re-initialization). The trees are in section 6.6. Three rules:

- Altium's order, which never moves: Find Similar and Clear Filter first; then the Place submenu (the same tree as
  `P`); then the "Actions" submenus, most of them shown whatever is under the cursor, with entries that do not apply
  disabled rather than removed; then Align, Cross Probe, the clipboard, and Properties last. The PCB has a shorter
  form over free space (6.6).
- Nothing is lost. Every action KiCad offers on right-click today must be reachable in the rebuilt menu, KiCad-only
  ones inside the Actions submenu of their object (for example Drag 45 and Break Track under Track Actions, Pin
  Tricks under Pin Actions). A test compares the two menus' action sets against a short list of deliberate drops.
- Menus shown inside a command are not these: a right-click in a command backs out (5.5), and `Shift+F1` shows the
  running tool's own menu, which stays as KiCad builds it.

### 5.5 In-command keys and command conventions

Most in-command keys are plain rebinding, because `RunHotKey` already prefers the active tool's context actions.
Three additions make the rest possible.

**Context aliases.** Some keys must mean one thing idle and another inside a command (`L` opens the Appearance
panel idle and flips a footprint to the other side during a move; `Space`, `Tab`, digits likewise), but the
existing action is global. Add thin `AS_CONTEXT` alias actions owned by the tool whose loop is running
(`pcbnew.InteractiveMove.flipWhileMoving`, `eeschema.InteractiveMove.rotateWhileMoving`, and so on) that forward to
the real handler. They have no default key in the `kicad` profile, so stock behavior is untouched.

**Right-click ends the command.** New setting `input.right_click_cancels` (on in the profile). When a tool other
than the selection tool is running, a right-click without a drag is delivered as `cancelInteractive` instead of
opening the context menu (hook: `TOOL_DISPATCHER::handleMouseButton`, where `TA_MOUSE_CLICK` for `BUT_RIGHT` is
made). Each drawing loop already treats cancel as "drop the current item, stay in the tool, second cancel leaves",
which is Altium's two-level exit. One difference to fix in the wire tool: Altium's right-click keeps the vertices
already clicked and discards only the segment on the cursor; make cancel-from-right-click finish the wire at the
last clicked vertex. The context menu stays reachable mid-command through the next item.

**The in-command key list.** `Shift+F1` and `` ` `` run a new global action that shows the active tool's context
menu at the cursor (`CMENU_NOW`). KiCad's tool context menus already list the tool's actions with their keys, so
this is Altium's in-command shortcut list for free. Note the caveat in `DispatchContextMenu`: the loop stops at the
first tool whose trigger is `CMENU_BUTTON`, so set the trigger on the tool that is actually active.

**Tab.** Bind `Tab` to the properties action (or its context alias) in every placement and move loop: part on the
cursor, label on the cursor, text. In the router bind it to `CustomTrackViaSize`. KiCad opens a modal dialog where
Altium pauses and uses a panel; that is acceptable. `nextNetItem` and `skip`, which hold `Tab` today, move.

### 5.6 Mouse

Selecting the profile for the first time applies a mouse preset and says so; everything stays editable in
Preferences > Mouse and Touchpad:

- wheel scrolls vertically, `Shift`+wheel horizontally, `Ctrl`+wheel zooms (KiCad's "trackpad defaults" mapping);
- right-drag pans (already the default), middle-drag zooms;
- `Ctrl+Shift`+wheel changes the active layer on the PCB (new: a wheel hook in the PCB frame that runs
  `layerNext`/`layerPrev`; mid-route those already insert the via);
- `Ctrl`+click on a PCB net object highlights the net: turn on the existing `editing.ctrl_click_highlight`, and
  check that `Shift+Ctrl`+click adds a net and `Ctrl`+click on empty space clears, adding them if not;
- `Ctrl`+click while routing runs `routerAttemptFinish` (new mouse binding in `ROUTER_TOOL::performRouting`).

The wheel mapping is the owner's own Altium setting (wheel scrolls, `Ctrl` is required to zoom), read from that
installation, not just Altium's default.

**As built (step 4).** The aliases are `eeschema.InteractiveMove.*WhileMoving` and `eeschema.InteractiveDrawing.*WhilePlacing`
(rotate counterclockwise and clockwise, mirror horizontally and vertically, properties) and `pcbnew.InteractiveEdit.*WhileMoving` and
`pcbnew.InteractiveDrawing.*WhilePlacing` (rotate both ways, flip, properties). An alias is a `TOOL_ACTION_ARGS::ForwardTo()`
action: `ACTION_MANAGER::RunHotKey` picks the context action of the tool highest on the stack, as it always did for
a key two actions share, and runs the action it forwards to. The sequence roots are global, so a running command's alias wins
their letter and the root runs when no such command does. `input.right_click_cancels` is switched with the profile
(`INTERACTION_PROFILE::SetActive()`); the cancel it makes carries `TOOL_EVENT::CANCEL_FROM_RIGHT_CLICK` so a loop can tell it from Esc,
and only the wire tool does (it restores the wires as they stood at the last click and drops those on the cursor).
The key list is `common.Control.showToolContextMenu`, which makes `TOOL_MANAGER::DispatchContextMenu()` open the first menu of
the tools on the stack whatever its trigger. `Tab` left `Next Net Item` (now Edit > Select > Next Net Item, `E S N`; `Shift+Tab` still
goes to the previous one) and `Skip` (its command's right-click menu). The mouse preset is applied once per
settings file (`input.altium_mouse_preset_applied`, and `editing.altium_mouse_preset_applied` for the PCB editor's Ctrl+click highlight),
when a frame starts or the profile is chosen in Preferences, and announced in the info bar.
`Shift+Ctrl`+click adds the net under the cursor to the highlighted ones when Ctrl+click highlights (`highlightNetAdd`).

### 5.7 PCB views

**Appearance by key.** `L` shows the Appearance panel and puts focus on its Layers tab; `Ctrl+D` on its Objects
tab. If the pane is hidden or collapsed, show it; a second `L` returns focus to the canvas.

**Transparent 2D.** Ship a named view configuration with the opacities in section 3.5 and make it what a new
project starts with under the profile. KiCad's object opacities are global, not per layer; the user's values are
the same on both copper layers, so nothing is lost. Tracks and arcs share one slider in KiCad, as do regions and
fills ("filled shapes"). Zone fills draw solid, at opacity 0.30. `LAYER_PRESET` (`include/project/board_project_settings.h`) saves layer and object visibility, the flip state and
the active layer but not the opacities, which live in the project's local settings; either add them to the preset
or make the profile write them when a project has none, so a new project opens in this look.

**Single-layer mode.** `Shift+S` cycles `HIGH_CONTRAST_MODE` Normal → Dimmed → Hidden → Normal
(`highContrastModeCycle`, today on `H`). Check that through-hole pads and vias stay fully visible in both modes, as
in Altium. `magneticSnapToggle`, which holds `Shift+S`, moves to `Shift+E`, Altium's key for the same idea.

**Mask level.** `[` and `]` step `appearance.hicontrast_dimming_factor` down and up and repaint (new actions). The
same factor should govern the dimming used by net highlight.

**Layer tab bar.** New widget, `PCB_LAYER_TAB_BAR`, an AUI pane docked under the canvas in the PCB and footprint
editors: at the left a swatch in the active layer's color that opens the layer presets (Altium's `LS`), then one tab
per visible layer in stack order (copper first, then the paired technical layers, then the rest), each a color chip
and the layer name, the active one bold. Click makes a layer active; right-click offers hide this layer, show all,
show only copper, and the presets; it scrolls when it overflows. It listens to the same events the Appearance panel
does (`layerChanged`, visibility and color changes) and holds no state of its own. A View menu item and a setting
show or hide it. This is the most visible "it looks right" piece and is self-contained.

**Heads-up display.** New overlay at the top-left of the canvas (a child window over the GAL canvas, or a view
overlay item; a child window is simpler and does not repaint the scene): cursor X and Y, delta from the last click,
the active layer in its color, grid and snap state, and the net and reference of what is under the cursor. While
routing it adds one line built from the router's state: corner mode, router mode, width and its source, via size,
layer pair, then net length. `Shift+H` toggles it, `Shift+G` toggles following the cursor, `Insert` zeroes the
delta (`resetLocalCoords`, which leaves `Space`). The status bar keeps showing what it shows today.

**Grid.** `G` opens the grid list as a popup at the cursor (the existing grid submenu from
`EDA_DRAW_FRAME`'s context menu); `Ctrl+G` opens grid properties; `Shift+Ctrl+G` grid overrides; `Q` toggles
units. In the schematic `G` and `Shift+G` cycle the grid (`gridNext`/`gridPrev`), as Altium does there.

**Colors.** The profile starts in Altium's classic colors (the owner's choice): on the PCB top red, bottom blue,
overlay yellow, multi-layer gray and the rest on Altium's dark canvas, so the transparent stack blends to the
colors an Altium eye reads; in the schematic a white sheet, blue wires and parts, dark-red labels and power ports.
Add it as a built-in theme beside "Reckless" for every editor, made the way the Reckless theme is (a generator
script and a checked-in JSON). A theme in these layer colors already exists outside this repository with the
converted libraries and is the reference. Any theme stays selectable in Preferences > Colors.

### 5.8 Routing

All by rebinding unless marked new. Target keys are in section 6.4.

- `Space` posture, `Shift+Space` corner mode cycle, `Shift+R` router mode cycle (give `CycleRouterMode` a key and
  put the mode in the HUD line), `Backspace` undo segment, `Enter` click, `Tab` size dialog.
- Layers: numpad `+` and `*` → `layerNext`, numpad `-` → `layerPrev`; mid-route they insert the via already. Idle,
  Altium's `+`/`-` walk every visible layer and `*` walks signal layers only; KiCad's `layerNext` walks copper only.
  Add `layerNextVisible`/`layerPrevVisible` for idle `+`/`-` (new, small). `Ctrl+L` → `SelLayerAndPlaceVia`.
- Widths: `3` → `trackWidthInc`, `4` → `viaSizeInc`. `Shift+W` and `Shift+V` open the track-width and via-size
  lists at the cursor (new actions that pop up the existing `TRACK_WIDTH_MENU`, split into its two halves).
  KiCad's width comes from the net class or from the list ("use net class values" is in that menu), which is a
  fair match for Altium's width source. The length tuner's `1`-`4` move.
- `1` toggles look-ahead: a new action that flips `fix_all_segments` for the session and shows it in the HUD.
  The owner routes with look-ahead off, so a click commits everything up to the cursor: leave `fix_all_segments`
  at its default, true.
- `Shift+D` toggles `remove_loops` (new action). `9` → `routerContinueFromEnd`. `2` → place a via; KiCad's via
  always continues on the paired layer, where Altium's `2` stays on the layer. A stay-on-layer via is a router
  change; leave it for later and document the difference.
- Conflict modes: Altium's Push and HugNPush both map to Shove; Walkaround to Walk around; Ignore to Highlight
  collisions with "allow DRC violations"; Stop at first obstacle to Highlight collisions without it. Jumping over
  obstacles and shoving vias are settings in the router dialog (`O` `T`, section 6).
- Finishing: `Ctrl`+click attempt finish (5.6); right-click or `Esc` drops the route and stays in the tool;
  "use starting track width" in the width menu is Altium's pick-up-width-from-existing-routes and should start on;
  starting a route with a click on a pad, track or empty space is unchanged.
- Dragging: a mouse drag on a track uses `editing.track_drag_action`, default 45-degree drag, which matches. `M`
  sequences are in section 6.
- Footprints: in the move loop `Space`/`Shift+Space` rotate and `L` flips to the other side (context aliases).

### 5.9 Schematic

- Keys by rebinding: `Space`/`Shift+Space` rotate, `X`/`Y` unchanged, `Tab` properties, `Space` posture while
  wiring, `Shift+Space` wire mode (unchanged), `Backspace` (unchanged), `PgUp`/`PgDn` zoom at the cursor, `Home`
  center on the cursor, `End` redraw, `Ctrl+PgDn` fit all, `Ctrl+R` duplicate, `Ctrl+M` measure, `Shift+C` clear
  highlight, `F11` Properties panel. Sheet navigation leaves `PgUp`/`PgDn` for `Alt+PgUp`/`Alt+PgDn`.
- **Dragging.** Keep KiCad's default, a mouse drag keeps wires attached. The owner runs Altium with "Always Drag"
  on, so this is already the habit. Add Altium's modifier as its inverse: `Ctrl`+drag does the other one (move without wires when
  rubber-banding is the default, and the reverse when `drag_is_move` is on). During a drag `Space` rotates and
  `Ctrl+Space` flips the posture of the attached wires. Today `Ctrl`+drag adds to a box
  selection; under the profile that meaning goes (Shift already adds). Hook: `SCH_SELECTION_TOOL::Main`, where the
  drag action is chosen.
- **Sticky placement.** Under the profile the part chooser's "place repeated copies" starts checked, and after a
  wire ends the wire tool stays active (it already does when started from the menu or a key; auto-started wires
  exit after one, which is right).
- **Net labels.** Altium puts a label on the cursor at once with the last name, its trailing number incremented,
  and `Tab` edits it before placing. KiCad opens the dialog first. Under the profile `createNewLabel` skips the
  dialog when there is a previous label of that kind, attaches a copy with the text incremented
  (`drawing.repeat_label_increment`), and `Tab` opens the dialog. Clicking on a wire that already has a name still
  takes that name. The first label of a session opens the dialog, as now.
- **Ports.** In a hierarchy Altium's port is KiCad's hierarchical label and its sheet entry is the sheet pin; in a
  flat multi-sheet design a port behaves like a global label. `P` `R` places a hierarchical label and `P` `C`
  (off-sheet connector) a global label (the owner's designs use sheet symbols with sheet entries); both are in the Place menu under their KiCad names with Altium's mnemonic.
  Power ports are power symbols (`P` `O`); add a submenu of the most used ones (GND, the supply rails found in the
  schematic) the way the Active Bar does.
- **Editing text in place.** New: `F2`, or a second slow click on a selected field or label, opens a borderless
  text control over the text on the canvas; `Enter` commits through the normal commit path, `Esc` abandons. Fields
  of symbols, labels, and plain text; multi-line text keeps the dialog. This is the largest schematic item and can
  land after everything else; until then double-click and `Tab` reach the dialog. The owner has in-place editing
  switched on in Altium and uses it, so it is wanted, not optional.
- **Selection.** Unchanged, since it already matches: click, `Shift`+click, drag left-to-right for inside,
  right-to-left for touching. `S` and `X` sequences map to the existing select actions.

### 5.10 3D

Step 1, small: `3` in the PCB editor shows the 3D viewer and `2` in the viewer raises the PCB editor (new action
`EDA_3D_ACTIONS::showPcbEditor`). Under the profile the viewer hides instead of being destroyed on close, so the
camera survives, and "live 3D refresh" defaults on. View keys in the viewer: `0` top, `9` rotate 90 degrees about Z
(a new action; today's rotate actions use the configured increment), `8` isometric (new preset), `Ctrl+F` flip
(`flipView`), `PgUp`/`PgDn` zoom. Mouse: `Shift`+right-drag orbits and right-drag pans (orbit is hard-coded to
left-drag in `HIDPI_GL_3D_CANVAS::OnMouseMoveCamera`; add the modifier path and keep left-drag).

Step 2, larger: show the 3D canvas inside the PCB frame in place of the 2D canvas, so `2` and `3` switch one
document's view as in Altium. `PANEL_PREVIEW_3D_MODEL` shows what an embedded canvas needs: its own
`BOARD_ADAPTER`, camera, tool manager and dispatcher. Swap the visible AUI center pane rather than showing both;
route keys to whichever canvas is shown; make a click in the docked canvas select in the board directly (today's
cross-probe path assumes the separate frame). The owner will decide after step 1 has been used for a while whether this is
worth its cost; don't start it before then.

Models: the footprint editor's Place menu gets "3D Body" (`P` `B`, opens the 3D Models tab with a new extruded
body) and "3D Model" (`P` `O`, the same tab's add-model path); Tools gets "Manage 3D Bodies". The importer should
turn Altium's extruded bodies into `PCB_3D_BODY`s; track that as importer work, with the gap that a footprint
currently uses only its first body.

### 5.11 Panels and toolbars

Low priority, after the keys. Under the profile: the Properties panel docks on the right, and with nothing selected
it could show document options (grid, units, snap, and the selection filter, which is a separate pane today); the
drawing toolbar's default order follows the Active Bar (filter, move/select/align groups, part, wire and net tools,
power ports, sheet tools, text, graphics; on the PCB: component, route group, via/pad, pour, keepout, dimensions,
text, graphics), by branching in `DefaultToolbarConfig`. `F11` toggles Properties, `F12` the selection filter,
`Shift+F12` the Search panel, `F4` hides and shows all panes, `Alt+F5` full screen.

## 6. Mapping tables

"=" means the KiCad action exists and only its binding or menu place changes. "new" needs code, described in
section 5. "-" is deliberately left out. A name with dots is the action's registered name, with the `eeschema.` or
`pcbnew.` prefix dropped where the table is for one editor; a bare identifier (`zoomIn`, `gridNext`) is the C++
member of `ACTIONS`, `SCH_ACTIONS` or `PCB_ACTIONS`, whose registered name is in its definition and sometimes
differs (`ACTIONS::gridProperties` is `common.Control.editGrids`). The profile's hotkey table uses registered names.

### 6.1 Schematic sequences

| Sequence | Altium command | KiCad action | |
|---|---|---|---|
| `P` `P` | Place Part | `InteractiveDrawing.placeSymbol` | = |
| `P` `W` (also `Ctrl+W`) | Place Wire | `InteractiveDrawingLineWireBus.drawWires` | = |
| `P` `B` | Place Bus | `InteractiveDrawingLineWireBus.drawBuses` | = |
| `P` `U` | Bus Entry | `InteractiveDrawing.placeBusWireEntry` | = |
| `P` `N` | Net Label | `InteractiveDrawing.placeLabel` | = (behavior: 5.9) |
| `P` `O` | Power Port | `InteractiveDrawing.placePowerSymbol` | = |
| `P` `R` | Port | `InteractiveDrawing.placeHierarchicalLabel` | = |
| `P` `C` | Off Sheet Connector | `InteractiveDrawing.placeGlobalLabel` | = |
| `P` `S` | Sheet Symbol | `InteractiveDrawing.drawSheet` | = |
| `P` `E` | Sheet Entry | `InteractiveDrawing.placeSheetPin` | = |
| `P` `J` | Junction (older Altium) | `InteractiveDrawing.placeJunction` | = |
| `P` `V` `N` | Directives > Generic No ERC | `InteractiveDrawing.placeNoConnect` | = |
| `P` `V` `M` | Directives > Parameter Set | `InteractiveDrawing.placeClassLabel` | = |
| `P` `V` `L` | Directives > Blanket | `InteractiveDrawing.drawRuleArea` | = |
| `P` `T` | Text String | `InteractiveDrawing.placeSchematicText` | = |
| `P` `F` | Text Frame | `InteractiveDrawing.drawTextBox` | = |
| `P` `D` `L` / `R` / `A` / `U` / `E` / `Y` / `B` / `G` | Drawing Tools: line, rectangle, arc, full circle, ellipse, polygon, Bezier, graphic | `drawLines`, `drawRectangle`, `drawArc`, `drawCircle`, `drawEllipse`, `drawPolygon`, `drawBezier`, `placeImage` | = |
| `E` `D` | Delete (pick objects) | `common.Interactive.deleteTool` | = |
| `E` `W` | Break Wire | `InteractiveEdit.breakWire` | = |
| `E` `N` (also `Shift+F`) | Find Similar Objects | - (no schematic equivalent; Find, `Ctrl+F`) | - |
| `M` `M` / `M` `S` | Move / Move Selection | `InteractiveMove.move` | = |
| `M` `D` / `M` `R` | Drag / Drag Selection | `InteractiveMove.drag` | = |
| `M` `E` / `M` `L` | Rotate Selection / clockwise | `InteractiveEdit.rotateCCW` / `rotateCW` | = |
| `S` `A` (also `Ctrl+A`) | Select All | `common.Interactive.selectAll` | = |
| `S` `C` | Select Connection | `InteractiveSelection.SelectConnection` | = |
| `S` `I` / `S` `E` | Inside Area / Lasso | `common.Interactive.selectSetRect` / `selectSetLasso` | = |
| `X` `A` | DeSelect All | `common.Interactive.unselectAll` | = |
| `A` `L` / `R` / `T` / `B` / `C` / `V` / `G` | Align left, right, top, bottom, horizontal centers, vertical centers, to grid | `Align.alignLeft` … `alignCenterX`, `alignCenterY`, `AlignToGrid` | = |
| `J` `C` | Jump to Component | `common.Interactive.find` | = |
| `V` `D` | Fit Document | `common.Control.zoomFitScreen` | = |
| `V` `F` (also `Ctrl+PgDn`) | Fit All Objects | `common.Control.zoomFitObjects` | = |
| `V` `A` | Zoom Area | `common.Control.zoomTool` | = |
| `V` `E` | Selected Objects | `common.Control.zoomFitSelection` | = |
| `V` `I` / `V` `O` | Zoom In / Out | `common.Control.zoomIn` / `zoomOut` | = |
| `V` `Z` | Zoom Last | `common.Control.undoZoom` | = |
| `V` `G` `G` / `R` / `V` / `S` | Grids: cycle, reverse, toggle visible, set | `gridNext`, `gridPrev`, `toggleGrid`, `gridProperties` | = |
| `V` `U` | Toggle Units | `common.Control.toggleUnits` | = |
| `Z` … | Zoom popup: `W` window, `I`, `O`, `E` selected, `L` last, `A` all, `S` sheet | as the `V` rows | = |
| `C` `C` | Project > Compile | `InspectionTool.runERC` | = |
| `C` `O` | Project Options | `EditorControl.schematicSetup` | = |
| `D` `U` | Update PCB Document | `common.Control.updatePcbFromSchematic` | = |
| `D` `B` | Browse Library | Symbol Library Browser | = |
| `D` `N` | Netlist | `EditorControl.exportNetlist` | = |
| `T` `A` `A` | Annotate Schematics | `EditorControl.annotate` | = |
| `T` `G` | Footprint Manager | `EditorControl.assignFootprints` | = |
| `T` `R` | Parameter Manager | `EditorControl.editSymbolFields` | = |
| `T` `L` | Update From Libraries | `InteractiveEdit.updateSymbols` | = |
| `T` `H` | Up/Down Hierarchy | `NavigateTool.enterSheet` on a sheet, else `leaveSheet` | new (one small action) |
| `T` `C` | Cross Probe | `EditorControl.selectOnPCB` | = |
| `T` `P` | Preferences | `common.SuiteControl.openPreferences` | = |
| `R` `I` | Bill of Materials | `EditorControl.generateBOM` | = |
| `R` `M` (also `Ctrl+M`) | Measure Distance | `common.Interactive.measureTool` | = |
| `O` `D` / `O` `S` | Document Options / Sheet | Page Settings | = |
| `O` `P` | Preferences | `openPreferences` | = |
| `O` `G` | Grids | `common.Control.editGrids` | = |
| `F` `S` / `F` `A` / `F` `P` / `F` `O` | Save, Save As, Print, Open | the common file actions | = |

### 6.2 Schematic direct and in-command keys

| Key | Where | Function | KiCad action | |
|---|---|---|---|---|
| `Space` / `Shift+Space` | idle with selection, placing, moving | rotate CCW / CW | `InteractiveEdit.rotateCCW` / `rotateCW` (context aliases in the loops) | = |
| `Space` | drawing a wire, bus or line | flip corner | `InteractiveDrawingLineWireBus.switchPosture` | = |
| `Shift+Space` | drawing a wire | cycle 90 / 45 / any angle | `EditorControl.lineModeNext` | = (same key) |
| `X` / `Y` | selection, placing, moving | flip | `InteractiveEdit.mirrorH` / `mirrorV` | = (same keys) |
| `Tab` | placing, moving | properties of the item on the cursor | `InteractiveEdit.properties` | = |
| `Backspace` | wiring | remove last vertex | `undoLastSegment` | = (same key) |
| `Enter` | any | left click | `common.Control.cursorClick` | = (same key) |
| right-click, `Esc` | any command | back out one level | `cancelInteractive` | new (5.5) |
| `Shift+F1`, `` ` `` | any command | list this command's keys | new action (5.5) | new |
| `PgUp` / `PgDn` | any | zoom in / out at the cursor | `zoomIn` / `zoomOut` | = |
| `Home` | any | center on the cursor | `common.Control.zoomCenter` | = |
| `End` | any | redraw | `common.Control.zoomRedraw` | = |
| `Ctrl+PgDn` | any | fit all objects | `zoomFitObjects` | = |
| `G` / `Shift+G` | any | next / previous snap grid | `gridNext` / `gridPrev` | = |
| `Ctrl+Shift+G` | idle | toggle visible grid | `toggleGrid` | = |
| arrows / `Shift`+arrows | any | cursor one step / ten steps | `cursorUp` … / `cursorUpFast` … | = |
| `Ctrl`+arrows | selection | nudge selection one grid step (`Shift` for ten) | new nudge actions | new |
| `Ctrl+R` | selection | rubber stamp | `common.Interactive.duplicate` | = |
| `Ctrl+M` | idle | measure | `measureTool` | = |
| `Shift+C` | idle | clear filter and highlight | `EditorControl.clearHighlight` | = |
| `Insert` | idle | repeat last item | `repeatDrawItem` | = (same key) |
| `F2` | field or label selected | edit in place | new (5.9) | new |
| `F11` | any | Properties panel | `common.Control.showProperties` | = |
| `Ctrl`+drag | on an object | the other of move and drag | new (5.9) | new |
| double-click | on an object | properties | unchanged | = |
| `Delete` | selection | delete | unchanged | = |

Keys KiCad uses that the profile frees: `A`, `P`, `W`, `B`, `L`, `H`, `S`, `T`, `J`, `Q`, `Z`, `C`, `I`, `M`, `G`,
`E`, `U`, `V`, `F`, `O`, `R`, `D`, `N`. `V`, `U` and `F` (edit value, reference, footprint) have no Altium key;
they stay reachable from the right-click "Edit Main Fields" submenu, by double-clicking the field, and by `F2`.

### 6.3 PCB sequences

| Sequence | Altium command | KiCad action | |
|---|---|---|---|
| `P` `T` (also `Ctrl+W`) | Place Track / Interactive Routing | `InteractiveRouter.SingleTrack` | = |
| `U` `T` | Route > Interactive Routing | `InteractiveRouter.SingleTrack` | = |
| `U` `I` | Interactive Differential Pair Routing | `InteractiveRouter.DiffPair` | = |
| `U` `R` / `U` `P` | Interactive Length Tuning / diff pair | `LengthTuner.TuneSingleTrack` / `TuneDiffPair` | = |
| `U` `L` | Gloss Selected | `InteractiveRouter.OptimizeSelected` | = |
| `U` `U` `A` | Un-Route All | `GlobalEdit.globalDeletions` | = (dialog) |
| `U` `U` `N` / `C` / `O` | Un-Route net, connection, component | `SelectNet` or `SelectConnection`, then `unrouteSelected` | new (three small composite actions) |
| `P` `V` | Via | `InteractiveDrawing.via` | = |
| `P` `C` | Component | `EditorControl.placeFootprint` | = |
| `P` `G` | Polygon Pour | `InteractiveDrawing.zone` | = |
| `P` `K` … | Keepout | `InteractiveDrawing.ruleArea` | = |
| `P` `L` | Line | `InteractiveDrawing.line` | = |
| `P` `S` | String | `InteractiveDrawing.text` | = |
| `P` `F` / `P` `E` | Fill / Rectangle | `InteractiveDrawing.rectangle` | = |
| `P` `R` | Solid Region | `InteractiveDrawing.graphicPolygon` | = |
| `P` `A` `A` / `E` / `U` | Arc by center, arc by edge, full circle | `arcCenter`, `arc`, `circle` | = |
| `P` `D` `L` / `R` / `E` / `C` | Dimension: linear, radial, leader, center | `alignedDimension`, `radialDimension`, `leader`, `centerDimension` | = |
| `P` `P` | Pad | `PadTool.placePad` (footprint editor) | = |
| `P` `B` / `P` `O` | Extruded 3D Body / 3D Body | footprint properties, 3D Models tab | new (5.10) |
| `E` `D` | Delete (pick objects) | `common.Interactive.deleteTool` | = |
| `E` `O` `S` / `R` | Origin set / reset | `gridSetOrigin` / `gridResetOrigin` | = |
| `E` `N` (also `Shift+F`) | Find Similar Objects | `EditorControl.findByProperties` | = |
| `M` `M` / `M` `S` | Move / Move Selection | `InteractiveMove.move` | = |
| `M` `D` | Drag | `InteractiveRouter.Drag45Degree` | = |
| `M` `C` | Move Component (pick by name) | `InteractiveEdit.FindMove` | = |
| `M` `B` | Break Track | `InteractiveRouter.BreakTrack` | = |
| `M` `X` | Move Selection by X, Y | `InteractiveEdit.moveExact` | = |
| `M` `I` | Flip Selection | `InteractiveEdit.flip` | = |
| `M` `O` | Rotate Selection | `InteractiveEdit.rotateCcw` | = |
| `S` `A` / `S` `B` | Select All / Board | `selectAll` | = |
| `S` `N` | Select Net | `InteractiveSelection.SelectNet` | = |
| `S` `P` (also `Ctrl+H`) / `S` `C` | Connected Copper / Physical Connection | `InteractiveSelection.SelectConnection` | = |
| `S` `I` / `S` `E` | Inside Area / Lasso | `selectSetRect` / `selectSetLasso` | = |
| `X` `A` | DeSelect All | `unselectAll` | = |
| `A` `L` / `R` / `T` / `B` / `C` / `V` / `D` / `S` | Align and distribute | `AlignAndDistribute.*` | = |
| `J` `C` / `J` `N` / `J` `P` | Jump to component, net, pad | `find` (Search panel) | = |
| `J` `A` / `J` `O` | Jump to absolute / current origin | new (move cursor to origin) | new, small |
| `V` `F` | Fit Board | `zoomFitObjects` | = |
| `V` `D` (also `Ctrl+PgDn`) | Fit Document | `zoomFitScreen` | = |
| `V` `A` / `V` `E` / `V` `Z` | Area / Selected / Zoom Last | `zoomTool` / `zoomFitSelection` / `zoomUndo` | = |
| `V` `B` (also `Ctrl+F`) | Flip Board | `Control.flipBoard` | = |
| `V` `U` (also `Q`) | Toggle Units | `toggleUnits` | = |
| `N` `S` `A` / `N` `H` `A` | Show / hide all connection lines | `Control.showRatsnest` | = |
| `N` `S` `N` / `N` `H` `N` | Show / hide a net's connection lines | `EditorControl.showNet` / `hideNet` | = |
| `G` | Snap grid popup | grid list at the cursor | new, small (5.7) |
| `D` `R` | Design Rules | `EditorControl.boardSetup` (Design Rules page) | = |
| `D` `K` | Layer Stack Manager | `boardSetup` (Board Stackup page) | = |
| `D` `C` | Classes | `boardSetup` (Net Classes page) | = |
| `D` `I` | Import Changes from schematic | `common.Control.updatePcbFromSchematic` | = |
| `D` `U` | Update Schematic | Update Schematic from PCB | = |
| `D` `T` | Manage Layer Sets | layer presets (Appearance panel) | = |
| `D` `N` | Netlist | `Control.showNetInspector` | = |
| `T` `D` | Design Rule Check | `DRCTool.runDRC` | = |
| `T` `G` `A` | Polygon Pours > Repour All | `ZoneFiller.zoneFillAll` | = |
| `T` `G` `R` | Repour Selected | `ZoneFiller.zoneFill` | = |
| `T` `G` `M` | Polygon Manager | `Control.zonesManager` | = |
| `T` `G` `H` | Shelve Polygons | `ZoneFiller.zoneUnfillAll` | = |
| `T` `E` | Teardrops | `GlobalEdit.editTeardrops` | = |
| `T` `N` | Re-Annotate | `ReannotateTool.ShowReannotateDialog` | = |
| `T` `H` | Via Stitching | `InteractiveDrawing.drawViaStitchArea` | = |
| `T` `C` | Cross Probe | `InteractiveSelection.SelectOnSchematic` | = |
| `T` `B` | 3D Body Placement | footprint 3D Models tab | new (5.10) |
| `T` `P` / `O` `P` | Preferences | `openPreferences` | = |
| `O` `K` | Layer Stack Manager | `boardSetup` | = |
| `O` `D` (also `Ctrl+D`) | View Options | Appearance panel, Objects tab | new, small |
| `O` `T` | Routing Options | `InteractiveRouter.SettingsDialog` | = |
| `O` `W` | Favorite Routing Widths | `boardSetup` (Pre-defined Sizes page) | = |
| `R` `B` | Board Information | `InspectionTool.ShowBoardStatistics` | = |
| `R` `M` (also `Ctrl+M`) | Measure Distance | `measureTool` | = |
| `R` `P` | Measure Primitives | `InspectionTool.InspectClearance` | = |
| `C` `C` / `C` `O` | Compile / Project Options | `runDRC` / `boardSetup` | = |

### 6.4 PCB direct and in-command keys

| Key | Where | Function | KiCad action | |
|---|---|---|---|---|
| `L` | idle | View Configuration | show and focus Appearance, Layers tab | new, small |
| `L` | moving a footprint | flip to the other side | `InteractiveEdit.flip` (context alias) | new alias |
| `Space` / `Shift+Space` | moving | rotate CCW / CW | `rotateCcw` / `rotateCw` (context aliases) | new alias |
| `Tab` | moving, placing | properties | `InteractiveEdit.properties` | = |
| `Tab` | idle with selection | extend selection: segment, connected tracks, net | `SelectConnection` | = |
| `Q` | any | mm / mil | `common.Control.toggleUnits` | = |
| `Ctrl+M` | idle | measure | `measureTool` | = |
| `Ctrl+G` / `Shift+Ctrl+G` | idle | grid properties / grid overrides | `gridProperties` / `toggleGridOverrides` | = |
| `Shift+E` | any | cycle snapping: active layer, all layers | `magneticSnapToggle` | = |
| `Shift+S` | any | cycle single-layer mode | `highContrastModeCycle` | = |
| `[` / `]` | any | mask level down / up | new actions | new |
| `Shift+C` | idle | clear highlight | `EditorControl.clearHighlight` | = |
| `Ctrl`+click | on a net object | highlight net | `editing.ctrl_click_highlight` on | = (setting) |
| numpad `+` / `-` | idle | next / previous visible layer | `layerNextVisible` / `layerPrevVisible` | new, small |
| numpad `*` / `Shift+*` | idle | next / previous signal layer | `Control.layerNext` / `layerPrev` | = |
| `Ctrl+Shift`+wheel | any | change layer | wheel hook | new, small |
| `PgUp` / `PgDn` | any | zoom at cursor | `zoomIn` / `zoomOut` | = |
| `Ctrl+PgDn` | any | fit | `zoomFitScreen` | = |
| `Home` / `End` | any | center on cursor / redraw | `zoomCenter` / `zoomRedraw` | = |
| `Ctrl+F` | any | flip board view | `Control.flipBoard` | = |
| `2` / `3` | idle | 2D / 3D | `show3DViewer` and its inverse | new (5.10) |
| `F5` | any | net color override | `Control.netColorMode` | = |
| `Ctrl+R` | selection | rubber stamp | `duplicate` | = |
| `Backspace` | idle with a segment selected | unroute segment and select the next | `unrouteSegment` | = (same key) |
| `Ctrl`+arrows | selection | nudge one grid step (`Shift` for ten) | new nudge actions | new |
| `Shift+H` / `Shift+G` / `Insert` | any | HUD on-off, HUD follows cursor, zero the delta | new HUD actions, `resetLocalCoords` | new (5.7) |
| `F11` / `F12` / `Shift+F12` | any | Properties, selection filter, Search | pane toggles | = |
| `Ctrl+Alt+G` | selection | gloss | `OptimizeSelected` | = |

Routing, from the list in section 3.4:

| Key | Function | KiCad action | |
|---|---|---|---|
| `Space` | toggle elbow side | `InteractiveRouter.SwitchPosture` | = |
| `Shift+Space` | cycle corner style | `InteractiveRouter.SwitchRoundingToNext` | = |
| `Shift+R` | cycle conflict mode | `InteractiveRouter.CycleRouterMode` | = |
| `Backspace` | undo commit | `InteractiveRouter.UndoLastSegment` | = (same key) |
| `Enter` | commit | `cursorClick` | = (same key) |
| `Ctrl`+click | autocomplete | `InteractiveRouter.AttemptFinish` | new mouse binding |
| `1` | look-ahead | toggle `fix_all_segments` | new action |
| `Shift+D` | loop removal | toggle `remove_loops` | new action |
| `Shift+W` / `Shift+V` | favorite widths / via sizes at the cursor | pop up `TRACK_WIDTH_MENU` halves | new, small |
| `3` / `4` | next width / via size | `trackWidthInc` / `viaSizeInc` | = |
| numpad `+`, `*` / `-` | next / previous layer with via | `layerNext` / `layerPrev` | = |
| `Ctrl+L` | select layer | `InteractiveRouter.SelLayerAndPlaceVia` | = |
| `2` | add via | `InteractiveRouter.PlaceVia` | = (continues on the paired layer; see 5.8) |
| `6` | change via type | cycle through, blind, micro | new, small |
| `9` | swap to opposite route point | `InteractiveRouter.ContinueFromEnd` | = |
| `Tab` | edit width and via | `InteractiveRouter.CustomTrackViaSize` | = |
| `Shift+F1`, `` ` `` | key list | router context menu at the cursor | new (5.5) |
| right-click, `Esc` | drop this route, stay in routing | `cancelInteractive` | new (5.5) |
| `L`, `/`, `7`, `8`, `Shift+F`, `Shift+A`, `Shift+G`, `Shift+T`, `Ctrl+W` | switch layer of trace, fanout via, next target, select transition, follow mode, accordions, length gauge, subnet swap, clearance boundaries | - | - |

### 6.5 3D viewer

| Key | Function | KiCad action | |
|---|---|---|---|
| `2` | back to the PCB editor | `showPcbEditor` | new |
| `0` | top view | `3DViewer.Control.viewTop` | = |
| `9` | rotate 90 degrees | new fixed-step rotate | new |
| `8` | isometric | new view preset | new |
| `Ctrl+F` | flip | `3DViewer.Control.flipView` | = |
| `PgUp` / `PgDn` | zoom | `zoomIn` / `zoomOut` | = |
| `Shift`+right-drag / right-drag | orbit / pan | canvas mouse handling | new / = |
| `L` | view configuration | `showLayersManager` | = |

### 6.6 Right-click menus

Read live on 2026-11-27 (section 2). Altium does not build a menu per kind of object. Each editor has one base menu in
a fixed order, and what is under the cursor only adds entries or changes what a submenu holds, so an entry's position
never moves; entries that do not apply are disabled, not removed. The profile keeps that rule. "(K)" marks KiCad
commands that have no Altium line and are filed where an Altium user would look; KiCad-only submenus are shown only
for their kind, after Altium's own, so they never move an Altium entry.

Schematic, one base menu:

| Item | Shown | Contents (KiCad actions) |
|---|---|---|
| Find Text… | always | `common.Interactive.find` (Altium's Find Similar Objects has no schematic equivalent) |
| Clear Filter | always | `EditorControl.clearHighlight` |
| Wire | over a wire or a pin | `drawWires`, starting there |
| Place ▸ | always | the `P` tree of 6.1 |
| Part Actions ▸ | always | Update From Libraries (`updateSymbol`, available with no symbol); with a symbol: Change Symbol, Edit with Symbol Editor, Unit ▸ and Body Style ▸ (K), Edit Main Fields ▸ reference, value, footprint (K), Autoplace Fields (K), Toggle Fitted/Not Fitted (`setDNP`) and the other attributes, Show Datasheet (K), Select PCB Components (`selectOnPCB`) |
| Sheet Symbol Actions ▸ | over a sheet symbol | Open SubSheet (`enterSheet`), Place Sheet Entry (`placeSheetPin`), Synchronize Sheet Entries and Ports (`syncSheetPins`), Cleanup Sheet Pins, Edit Page Number |
| Sheet Actions ▸ | always | Leave Sheet, Page Settings |
| Pin Actions ▸ (K) | pin | KiCad's pin tricks: no-connect, wire stub, label from pin name; Swap Pins |
| Wire Actions ▸ (K) | wire or bus | Add Junction, Add Net Label, Add Port, Break Wire, Slice, Unfold From Bus, Select Connection, Assign Netclass |
| Port Actions ▸ (K) | label of any kind | Change To ▸ net label, port, off-sheet connector, directive, text |
| Align ▸ (K) | two or more items | the `A` tree (Altium's schematic menu has none) |
| Cross Probe | always | `EditorControl.selectOnPCB` |
| Cut, Copy, Paste | always | unchanged, Cut and Copy disabled with no selection, plus Paste Special and Duplicate (K) |
| Project Options… | always | `EditorControl.schematicSetup` |
| Preferences… | always | `openPreferences` |
| Properties… | over an object or with a selection | `InteractiveEdit.properties`, last |

Altium's Pin Mapping, Supplier Links (over a part), the sheet and part conversions, variants and code symbols have no
KiCad counterpart.

PCB, two forms: a short one over free space, and one fixed long one over any object or with a selection.

| Item | Free space | Object | Contents (KiCad actions) |
|---|---|---|---|
| Find Similar Objects… | yes | yes | `EditorControl.findByProperties` |
| Clear Filter | yes | yes | `EditorControl.clearHighlight` |
| Interactive Routing, Interactive Length Tuning | yes | - | `InteractiveRouter.SingleTrack`, `LengthTuner.TuneSingleTrack` (Altium's second line is Quick Routing) |
| Place ▸ | yes | yes | the `P` tree of 6.3 |
| Toggle Lock | - | with a selection | `EditorControl.toggleLock` (Altium shows a lock toggle over a pour) |
| Unions ▸ | - | yes | KiCad's groups: enter, leave, create, remove |
| Component Actions ▸ | - | yes | over a footprint or its pad: Move Component (`move`), Unroute Component (new, as `U` `U` `O`), Show Nets / Hide Nets (local ratsnest), Select Component Connections, Manage 3D Bodies (footprint properties, 3D Models tab), Update From PCB Libraries (`updateFootprint`), Change Footprint (K), Open in Footprint Editor (K), Swap Components (`swap`), Flip, Rotate (K); otherwise the entries that need a footprint are disabled |
| Polygon Actions ▸ | - | yes | Repour Selected (`zoneFill`), Repour All (`zoneFillAll`), Shelve Selected (`zoneUnfill`), Bring to Front / Send to Back (`zonePriorityMoveToTop` / `MoveToBottom`, with Raise and Lower), Polygon Manager (`zonesManager`), Add Cutout, Add Similar Polygon, Duplicate onto Layer, Combine Selected Polygons (`zoneMerge`); those that need a zone are disabled without one |
| Net Actions ▸ | - | over a track, via or pad | Show Nets / Hide Nets (`showNet` / `hideNet`), Highlight Net, Select Net, Assign Net Class, Properties (Net Inspector) |
| Pad Actions ▸ (K) | - | pad | Copy, Apply and Push Pad Properties, Swap Pad Nets |
| Track Actions ▸ (K) | - | track, arc, via | Drag (45 degree), Drag (free angle), Break Track, Change Track Width, Fillet Tracks, Select Connection, Unroute Selected, Switch Track to Next / Previous Layer, Route From Other End |
| Convert ▸ (K) | - | shapes | the convert-shape commands |
| Align ▸ | - | yes | the `A` tree with a selection; Altium shows only Position Component Text without one |
| Cross Probe | yes | yes | `InteractiveSelection.SelectOnSchematic` |
| Cut, Copy, Paste | yes | yes | unchanged, Cut and Copy disabled with no selection, plus Paste Special and Duplicate (K) |
| Applicable Unary Rules…, Applicable Binary Rules… | - | yes | `InspectionTool.InspectConstraints`, `InspectionTool.InspectClearance` |
| Violations ▸ (K) | - | DRC marker | marker items: exclude, next, previous |
| Preferences… | yes | yes | `openPreferences` |
| Properties… | - | yes | `InteractiveEdit.properties`, last |

Altium's Component Actions has a short form when no component is under the cursor (unroute, select connections,
swap) and a long one over a component, its text or its pad; with components selected it adds Move Component Origin
To Grid. Its xSignals and Room Actions submenus, and the polygon slice, explode, move and resize items, have no KiCad
counterpart. Zoom and Grid, which KiCad appends to every context menu, are dropped from both editors' menus: they
are `Z`, `V` and `G`.

### 6.7 In-command keys by command

Read live on 2026-11-27 (section 2) from `Shift+F1` in each command. The core of 3.3 (`Tab`, `Space`,
`Shift+Space`, `Enter`, `Esc`, the zoom keys and the cursor keys) is in every list below and is not repeated. The last
column says what the profile does with each key: = it maps to an existing action or a context alias of 5.5; new, a
small action is wanted; - no KiCad counterpart, left out.

**PCB, moving a component (`M` `C`, `M` `M` on a footprint).**

| Key | Function | |
|---|---|---|
| `L` | flip to the other side | = (6.4) |
| `X` / `Y` | flip in X / Y | - (footprints flip sides, not in place) |
| `R` | cycle the pushing mode (ignore, push) | - |
| `Shift+Tab` | change which component is on the cursor | - |
| `N` | cycle the net-line connect mode (pad to pad and others) | - |
| hold `Ctrl` / hold `Shift` | align to component edges / to pads | - |
| `Shift+R` | toggle re-routing the component's tracks as it moves | - |
| `Shift+Ctrl+G` | gloss effort | - |
| numpad `+` `-` `*` | next / previous / next signal layer | = (6.4) |

Its display reads the live DRC count and the rule being checked, then the move mode, pushing, re-route, object
count, net-line mode and gloss, each mode with the key that changes it.

**PCB, placing a line (`P` `L`).** On a copper layer this places track without the router, and its list is close to
the router's (3.4): `Backspace` removes the last segment, `Shift+R` the routing mode, `Shift+W` / `Shift+V` the
width / via pickers, `,` / `.` decrease / increase the arc setback, `/` connects to an internal plane, `1`
look-ahead, `2` adds a via with no layer change, `3` / `4` cycle the width / via-size source, numpad `+` `-` `*`
change layer. KiCad's `P` `L` is a graphic line (6.3), whose tool has none of these; only `Backspace` maps
(`undoLastSegment`). Track goes through the router.

**PCB, placing a dimension (`P` `D` …).** The core only, with the layer keys. The submenu holds linear, angular,
radial, ordinate, leader, standard, center, baseline, linear diameter and radial diameter; 6.3 maps the four KiCad
has.

**Schematic, wiring (`P` `W`).**

| Key | Function | |
|---|---|---|
| `Space` | toggle wire start / end mode (the corner flip) | = (6.2) |
| `Shift+Space` | cycle 90 / 45 / any angle | = (6.2) |
| `Backspace` | remove the last vertex | = (6.2) |
| `Ins` | insert a vertex | - |

The status bar shows the corner mode and the end it applies to. Dragging (`M` `D`) has the placing list below
without `Ins`, plus `Ctrl+Space` for the start / end mode.

**Schematic, placing a net label, port, power port or part.**

| Key | Function | |
|---|---|---|
| `X` / `Y` | flip | = (6.2) |
| `Ins` | copy the attributes of the object under the cursor | new |

The status bar names `Tab` and `F1`.

**Schematic, the move menu (`M`).** As 6.1, with Move Selection by X, Y, the four ordering commands and six
sheet-symbol and sheet-entry commands (flip along X / Y, swap side, reverse order, toggle the IO type of the selected
or all entries), disabled without a selection. The ordering and sheet-entry commands have no KiCad counterpart.

**PCB, placing a polygon pour (`P` `G`).** Outline placement starts at once, with no dialog first. `Backspace`
removes the last vertex, `Space` / `Shift+Space` toggle the elbow side / cycle the corner mode (shown as "90/45" in
the display), `,` / `.` change the arc setback by one step and `Shift+,` / `Shift+.` by ten, `1` toggles look-ahead,
`Enter` places a vertex, numpad `+` `-` `*` change layer. The view pans when the cursor passes the window edge.
KiCad's zone tool has the vertex, posture and layer keys; arc setback and look-ahead are left out.

**Schematic, the other placements.**

- Bus (`P` `B`): the wiring list exactly, `Ins` included.
- Bus entry (`P` `U`) and port (`P` `R`): the placing list (`X`, `Y`, `Ins` copy attributes). A port takes two
  points, the second setting its length.
- Sheet symbol (`P` `S`): first corner, then size, with the display showing the box's delta. Rotation and `Ins`, but
  no `X` / `Y` flip.

**Moving a selection.** Dragging a schematic selection (`M`, Drag Selection) draws the dragged objects faded and has
the dragging list. Moving a PCB selection (`M`, Move Selection) has Move Component's list and display, the DRC count
updating live; if the selection holds locked items, Altium asks whether to continue once the reference point is
picked. KiCad asks nothing and skips locked items; the profile keeps that.

**Length tuning (`U` popup; single nets and differential pairs).** Until a track is picked, the display asks for one
and the key list holds only Help; picking a track that is not part of a pair, in pair tuning, is refused with a
message. Then: `Shift+G` toggles the length gauge, `,` / `.` decrease / increase the amplitude, `1` / `2` the miter,
`3` / `4` the spacing, `Space` cycles the pattern style, `Ctrl`+drag rotates the pattern and `R` toggles 45-degree
snapping while rotating, `Tab` pauses, numpad `+` `-` `*` change layer. The display shows the net length and the
target. The digits mean something else here than when routing, as the PCB line list above shows: the in-command
keys belong to the command (5.5).

**Differential-pair routing (`U` popup).** Started on a pad of one net of a defined pair; if the other net has nothing
near the pick, a message says so and the command keeps waiting. Its list is the fullest routing set: everything in
3.4, plus `5` cycles the via pattern, `Shift+6` the gap source and `Shift+B` the width and gap sources together, with
checkmarks on the toggles that are on (loop removal, subnet swapping). The display adds the gap source, and the
length of each net of the pair with their difference against the allowed delta. Inside routing `Ctrl+W` shows
clearance boundaries, where idle it starts routing. Defining a pair asks only for the positive net, the negative net
and a name.

**The Route menu (`U`).** Active routing, interactive routing, interactive differential-pair routing, the two quick
routing modes and multi-routing; gloss and retrace selected; single and pair length tuning; then fanout, autoroute,
subnet jumpers and unroute submenus. 6.3 maps the KiCad ones.

**The pickers.**

- `G` on the PCB, the grid menu at the cursor: seven mil steps (1 to 100) and six mm steps (0.025 to 2.5), then grid
  properties (`Ctrl+G`), the global snap grid (`Shift+Ctrl+G`) and separate X and Y snap submenus. KiCad's grid list
  at the cursor (5.7) has the steps; separate X and Y are left out.
- `Shift+W`, a modal "Choose Width" dialog: the favorite widths in imperial and metric columns with the current one
  highlighted, and "Apply To All Layers". `Shift+V`, a modal "Choose Via Sizes" dialog: the via definitions in use,
  each with its source (local or a rule), and the diameter and hole of the one selected. The favorites are a
  preference. The KiCad form stays a menu at the cursor (6.4): the board's track widths and via sizes are already a
  list.

**The heads-up display while routing.** The cursor position and delta, the active layer, the snap grid and the
hotspot snap distance; one line with the corner style, conflict mode, the width and via-size sources, the via span
and the gloss effort; and one with the net name, the routed length and the size of the current segment. The status
bar repeats the mode line, layer tabs other than signal layers grey out, and the uncommitted look-ahead segment is
drawn red and hatched. 5.7's HUD and 5.8's look-ahead cover these.

**What this changes in 6.4.** `3` and `4` cycle where the width and via size come from (user choice, rule minimum,
preferred, maximum), not the next size; 6.4's `trackWidthInc` / `viaSizeInc` is a different behavior. Step 4 (#137)
decides between a source-cycling action and keeping the next-size mapping as the nearest KiCad meaning.

## 7. Order of work

Each step is one issue and one or more `fix/<issue>-<slug>` branches, mergeable alone, with the profile off by
default until step 3 is in. An umbrella issue links them.

| Step | Content | Size | Done when |
|---|---|---|---|
| 1 | Profile switch (5.1), profile hotkey layer and per-profile user file (5.2), tests | M | Selecting the profile changes a handful of test bindings and back; `kicad` profile byte-for-byte unchanged; hotkey editor resets to profile defaults |
| 2 | `MENU_DEF` and builders (5.4); schematic and PCB trees; menu bars built from them | M | Menu bars follow section 6 with working mnemonics on Windows and GTK; tree tests pass |
| 3 | Sequence engine (5.3) with the root actions; the full profile hotkey tables for direct keys (6.2, 6.4) | L | Every sequence in 6.1 and 6.3 marked "=" runs when typed fast with no menu shown, and from the menu when typed slowly, on Linux and Windows |
| 4 | Command conventions (5.5): context aliases, right-click cancels, key list, `Tab`; mouse preset (5.6) | M | A part can be placed, rotated with `Space`, edited with `Tab`, and the command left with two right-clicks; wires end on right-click keeping clicked vertices |
| 5 | Routing keys (5.8): bindings, width and via popups, look-ahead and loop-removal toggles, `Ctrl`+click finish, visible-layer stepping, `Ctrl+Shift`+wheel | M | A two-layer route with a layer change, a width change and a mode change can be made using only the keys in 6.4 |
| 6 | Views (5.7): `L`, transparent 2D preset, Altium classic color theme, `Shift+S`, mask level, grid popup | M | The preset reproduces the opacities in 3.5 in the classic colors; the three single-layer states cycle |
| 7 | Layer tab bar (5.7) | M | Tabs track layer visibility, color and the active layer both ways |
| 8 | Heads-up display (5.7), with the routing line | M | Position, delta, layer, net under cursor, and router state update live without measurable frame cost |
| 9 | Schematic behaviors (5.9): `Ctrl`+drag inversion, label auto-increment, sticky defaults, nudge | M | As described in 5.9 |
| 10 | 3D step 1 (5.10): `2`/`3`, camera kept, view keys, orbit modifier | S-M | `3` then `2` returns to the same 2D view; `3` again shows the same camera |
| 11 | In-place text editing (5.9) | L | `F2` edits a value on the canvas and commits through undo |
| 12 | Panels and toolbars (5.11) | S-M | - |
| 13 | 3D step 2: docked 3D canvas; importer support for extruded bodies | L | decided after step 10 has been used |
| 14 | Right-click menus rebuilt from `MENU_DEF` trees (5.4, 6.6), both editors | M | The idle right-click menu follows 6.6 for free space, a part, a wire, a label, a sheet, a footprint, a track, a pad and a zone; the action-set test passes |

Step 14 is numbered last only so the earlier numbers keep matching their issues; it needs step 2 alone and belongs
right after step 3. Steps 1 to 4 give most of the benefit and have no dependency on the rest. 5 to 8 can go in parallel after 3, as can
9 and 10.

## 8. Testing

- **Unit, no GUI** (`qa_common`): the sequence state machine (complete, partial, mistyped, `Esc`, timer, nested
  submenus, a key that is both a child mnemonic and a hotkey); profile table parsing and layering; every action
  name in a profile table and in a menu tree resolves to a registered action (run in `qa_eeschema` and `qa_pcbnew`,
  where the actions are linked); mnemonics unique within each menu; no two actions in one scope and one tool share
  a key after resolution; the `kicad` profile resolves to exactly the compiled defaults.
- **Settings**: the new keys load with defaults and need no migration; switching profile and back restores every
  binding and the user file of each profile is untouched by the other.
- **By hand, per platform** (Linux first, then Windows, then macOS), a short script per step: the "done when"
  column, typed at speed. Fast typing is the thing unit tests cannot see: check for dropped second keys on GTK.
- Follow the fork's rules: warnings are errors, upstream code style, 120 columns, American spelling, no pragmas.

## 9. Decisions

Taken by the owner on 2026-11-26.

1. **Name.** "Altium-style" in Preferences; setting value `altium`.
2. **Colors.** The profile starts in Altium's classic colors, with the transparent 2D view of section 3.5 as the
   default for a new project.
3. **Menu delay.** The popup appears after the short pause of section 5.3 (300 ms), adjustable, 0 for at once.
4. **Ports.** `P` `R` places a hierarchical label; `P` `C` places a global label.
5. **Docked 3D.** Not decided: build step 10 (two windows, camera kept) first and decide on step 13 after using it.
6. **Right-click menus.** Rebuilt in Altium's structure, not just reordered (5.4, 6.6, step 14).
7. **Default.** Altium-style is the profile a new configuration starts with in the Reckless build
   (`KICAD_HOUSE_INTERACTION` ON with the other house options), from step 3 on (2026-11-27).

Settled earlier from the owner's own Altium preferences: the wheel scrolls and `Ctrl`+wheel zooms; a plain drag in
the schematic keeps wires attached and `Ctrl`+drag does not; in-place text editing is wanted; routing starts with
look-ahead off.

## 10. Risks

- **Upstream churn.** The menu bar files, `tool_manager.cpp`, `action_manager.cpp` and `router_tool.cpp` change
  often upstream. Keeping the profile's code in its own files with one-line guarded hooks keeps rebase conflicts
  small; the name-resolution tests catch actions that upstream renames or removes.
- **Sequences on macOS.** Native menus have no mnemonics; the engine-owned key handling in 5.3 is the mitigation,
  and macOS is the last platform to verify.
- **Keys the profile frees.** KiCad users who try the profile lose single-letter keys. That is the point, but the
  hotkey editor must make the per-profile sets and the reset clear.
- **Text controls.** Sequences must never fire while a text control has focus; the dispatcher already ignores keys
  then, and the in-place editor (step 11) must keep that true.
- **Same key, different meaning.** The context aliases rely on `RunHotKey`'s priority for the active tool. Each
  alias needs a test or a manual check that the idle meaning does not fire mid-command, and the reverse.

# KiCad, Reckless build

This is a modified version of [KiCad](https://www.kicad.org), maintained by Reckless Technology
(github.com/reckless-technology/kicad). It is not an official KiCad release and is not endorsed by the KiCad project.

- **Based on:** KiCad's development line (upstream `master`, gitlab.com/kicad/code/kicad), which becomes KiCad 11.
  Files it saves use the KiCad 11 development format, which KiCad 10 cannot open.
- **Modified:** from 2026-11-22. Branch `reckless/main` is upstream `master` plus our changes, rebased onto upstream
  only when we choose to; every change is a separate commit, listed by `git log upstream/master..reckless/main`.
- **License:** unchanged, GNU GPL version 3 or later (see `LICENSE` and `LICENSE.README`), as for KiCad itself.
- **Builds** identify themselves with a `-reckless` suffix in Help > About.

## What we change and why

Fixes first: the Altium importer faults we met converting our own designs, each also reported upstream
(gitlab.com/kicad/code/kicad issues #26126-#26133). Each fix starts on its own `fix/*` branch from `reckless/main`
and lands as one commit. We don't offer the changes back to KiCad unless KiCad asks for them.

House fonts (#47): our installed build uses Noto Serif for menus, dialogs and panels, and Monaspace Xenon for
fixed-width text (the custom rules editor, text-variable fields, the scripting console). Two mechanisms, both behind
the `KICAD_HOUSE_FONTS` option, on when `KICAD_VERSION_EXTRA` is `reckless` and off otherwise, so a build without it is
stock KiCad:
- Fixed-width: on Linux the desktop entries start each program through `env` with `FONTCONFIG_FILE` pointing at
  `share/kicad/fonts/fonts.conf`, which includes the system configuration and maps `monospace` to Monaspace Xenon, so
  only KiCad's processes see it. Starting a program by hand from a shell skips the entries.
- Proportional: with wxGTK, `PGM_BASE::InitPgm()` calls `KIPLATFORM::UI::SetInterfaceFontFamily( "Noto Serif" )`
  before any window exists, whatever started the program. It swaps only the family in the desktop's `gtk-font-name`,
  keeping its size, at application priority, which holds over the desktop's XSETTINGS font. If Noto Serif is not
  installed the desktop font is left alone.

Schematic text defaults to Noto Serif (#48): `appearance.default_font` ships as Noto Serif when fontconfig lists that
family, and as the KiCad stroke font otherwise. New text carries no font of its own, so it follows the setting when
drawn. The choice is made when the settings object is built, because fontconfig answers a missing family with some other
font, never the stroke font. No migration touches the key: a file that names a font keeps it. Text saved with no face of
its own, which includes every pin's name and number and most fields, follows the setting too, so existing schematics
draw and plot in Noto Serif; the preference set back to "KiCad Font" restores the stroke font.

PCB text has a default font face too (#49), set per layer class in Board Setup > Text & Graphics Defaults beside size,
thickness and italic, stored in `BOARD_DESIGN_SETTINGS` (`defaults.silk_text_font`, `copper`, `fab` and `other`, in the
`.kicad_pro`). It has three states: **not set**, the empty string, which is the default and what an absent key reads as
(shown as "(not set)" in the grid); an explicit "KiCad Font", the stroke font; or a face. Not set leaves everything as it
was: `StyleFromSettings()` keeps the face an existing item has, so restyling a footprint's fields, the netlist update and
the global edit dialog never strip a face, and the drawing tools give new items no font. An explicit "KiCad Font" or a
face is applied by `StyleFromSettings()` and by the text, text box and dimension tools. The footprint editor's own
defaults have no font setting yet. A board with "Embed fonts" on embeds the fonts its items use, and
`BOARD::GetFonts()` now includes footprint fields and text as well as board drawings.

## Keeping up with upstream

`reckless/main` is rebased onto upstream `master` only when Jonny asks for it, never on a schedule, so a rebase never
lands in the middle of fixes in progress (#24). When it is:

1. `git fetch upstream master`. If `reckless/main` already sits on it, there's nothing to do.
2. Tag the current head `pre-rebase/YYYY-MM-DD` and push the tag. A rebase rewrites every commit of ours, and the
   tag keeps the old SHAs reachable for anything that cites them.
3. Rebase on a branch `rebase/YYYY-MM-DD`. If a conflict can't be resolved mechanically, abort, open a `bug` issue
   naming the files and commits, and stop. Nothing is pushed.
4. Build with clang-20 and run the Altium import suites (`qa_eeschema` and `qa_pcbnew`, `Altium*`). Compare the
   warning count with the previous build.
5. `git push --force-with-lease=reckless/main:<old head> origin rebase/YYYY-MM-DD:reckless/main`, then rebase any
   open `fix/*` branches onto the new `reckless/main`.
6. Install. Comment on the rebase's issue with the old and new upstream base, the number of our commits and the test results.

## Building

As for KiCad (dev-docs.kicad.org, "Building on Linux"), with clang-20. We configure with
`-DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/opt/kicad-reckless -DKICAD_VERSION_EXTRA=reckless
-DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20` and set `CMAKE_INSTALL_RPATH` to the prefix's `lib` and
`lib/kicad/plugins/3d`, so the programs run without wrapper scripts.

The experimental Vulkan render engine (`common/rhi/`, "RHI Vulkan (Experimental)" in Preferences > Common >
Graphics) is only compiled when the Vulkan headers and `glslc` are present (Ubuntu: `libvulkan-dev`, `glslc`). We
build it in (#40); OpenGL stays the default canvas, and the engine loads the system Vulkan library only when selected.

### Warnings are errors

`KICAD_WERROR` (cmake/Warnings.cmake) makes every compiler warning in KiCad's own code an error, through CMake's
`COMPILE_WARNING_AS_ERROR` target property (CMake 3.24 or newer), so configure-time feature checks are not affected.
It is ON by default with Clang and MSVC and OFF with other compilers; `-DKICAD_WERROR=OFF` turns it off. Code under
`thirdparty/` keeps its own flags. The warnings are upstream's set (`-Wall` and the extras in `cmake/Warnings.cmake`)
plus `-Wextra` and `-Wpedantic` (#6), applied once at the top level to all of KiCad's own C, C++ and Objective-C++
(#46), so `qa/`, `3d-viewer/`, `pcb_calculator/` and the libraries are checked too. A warning is fixed at its cause,
not silenced with a pragma or a `-Wno-` flag. Upstream changes can introduce a warning when `reckless/main` is
rebased: fix it in that rebase.

MSVC builds KiCad's own C++ at `/W4 /permissive-` (#43). Upstream's `/Wall` and its list of 37 `/wd` exceptions are
gone, and there is one exception left, `/wd4251`. C4251 ("needs to have dll-interface") warns when an exported class
has a member or base of a type that is not exported, such as a standard-library container. It matters only when
modules are built with different toolsets or C runtimes. Every KiCad module is built in one configure, with one
toolset and the dynamic CRT, which is where Microsoft documents C4251 as safe to ignore. The one pragma is a `#pragma
warning( disable : 4324 )` between `push` and `pop` around `qa/tests/common/test_ki_any.cpp`'s `X3`, whose
over-alignment is what that test tests.
With `KICAD_WERROR` on, turning on an opt-in tool that the default build leaves out, such as the PNS debug tool
(`KICAD_BUILD_PNS_DEBUG_TOOL`, about 120 C4100 sites), fails the MSVC build, because those tools have been outside
every warning check so far (#78). Build them with `-DKICAD_WERROR=OFF` until then.

## Commits

Every commit in `reckless/main` is authored and committed by one identity, Jonny Reckless
<jonny@recklesstechnology.com>. Set it in each clone (`git config user.name` and `user.email`) and turn the hooks on
once per clone with `git config core.hooksPath .githooks`. Two drop-ins check the identity:
`.githooks/pre-commit.d/10-reckless-identity` refuses a commit whose author or committer is anyone else, and
`.githooks/pre-push.d/10-reckless-identity` refuses a push carrying such a commit, which catches a rebase or
cherry-pick, since those run no pre-commit hook. Commits already on a remote-tracking branch, upstream KiCad's among
them, are not checked.

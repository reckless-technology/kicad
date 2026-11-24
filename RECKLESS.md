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

House fonts (#47, part): fixed-width text (the custom rules editor, text-variable fields, the scripting console) uses
Monaspace Xenon in our installed build. On Linux the desktop entries start each program through `env` with
`FONTCONFIG_FILE` pointing at `share/kicad/fonts/fonts.conf`, which includes the system configuration and maps
`monospace` to Monaspace Xenon, so only KiCad's processes see it. The `KICAD_HOUSE_FONTS` option controls it, on when
`KICAD_VERSION_EXTRA` is `reckless` and off otherwise, so a build without it is stock KiCad. Starting a program by hand
from a shell skips the entries. The proportional font (Noto Serif) is not set this way: on X11 GTK takes it from the
desktop's XSETTINGS, which a `settings.ini` or theme in the environment does not override, so it needs a small code
change (#47 comment).

Schematic text defaults to Noto Serif (#48): `appearance.default_font` ships as Noto Serif when fontconfig lists that
family, and as the KiCad stroke font otherwise. New text carries no font of its own, so it follows the setting when
drawn. The choice is made when the settings object is built, because fontconfig answers a missing family with some other
font, never the stroke font. No migration touches the key: a file that names a font keeps it. Text saved with no face of
its own, which includes every pin's name and number and most fields, follows the setting too, so existing schematics
draw and plot in Noto Serif; the preference set back to "KiCad Font" restores the stroke font.

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
It is ON by default with Clang and OFF with other compilers; `-DKICAD_WERROR=OFF` turns it off. Code under
`thirdparty/` keeps its own flags. The set of warnings is upstream's own (`-Wall` and the extras in
`cmake/Warnings.cmake`); `-Wextra` and `-Wpedantic` are not enabled, pending #6. A warning is fixed at its cause,
not silenced with a pragma or a `-Wno-` flag. Upstream changes can introduce a warning when `reckless/main` is
rebased: fix it in that rebase.

## Commits

Every commit in `reckless/main` is authored and committed by one identity, Jonny Reckless
<jonny@recklesstechnology.com>. Set it in each clone (`git config user.name` and `user.email`) and turn the hooks on
once per clone with `git config core.hooksPath .githooks`. Two drop-ins check the identity:
`.githooks/pre-commit.d/10-reckless-identity` refuses a commit whose author or committer is anyone else, and
`.githooks/pre-push.d/10-reckless-identity` refuses a push carrying such a commit, which catches a rebase or
cherry-pick, since those run no pre-commit hook. Commits already on a remote-tracking branch, upstream KiCad's among
them, are not checked.

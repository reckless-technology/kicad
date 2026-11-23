# KiCad, Reckless build

This is a modified version of [KiCad](https://www.kicad.org), maintained by Reckless Technology
(github.com/reckless-technology/kicad). It is not an official KiCad release and is not endorsed by the KiCad project.

- **Based on:** KiCad's development line (upstream `master`, gitlab.com/kicad/code/kicad), which becomes KiCad 11.
  Files it saves use the KiCad 11 development format, which KiCad 10 cannot open.
- **Modified:** from 2026-11-22. Branch `reckless/main` is upstream `master` plus our changes, rebased onto upstream
  every week; every change is a separate commit, listed by `git log upstream/master..reckless/main`.
- **License:** unchanged, GNU GPL version 3 or later (see `LICENSE` and `LICENSE.README`), as for KiCad itself.
- **Builds** identify themselves with a `-reckless` suffix in Help > About.

## What we change and why

Fixes first: the Altium importer faults we met converting our own designs, each also reported upstream
(gitlab.com/kicad/code/kicad issues #26126-#26133). Each fix starts on its own `fix/*` branch from `reckless/main`
and lands as one commit. We don't offer the changes back to KiCad unless KiCad asks for them.

## Keeping up with upstream

Once a week, `reckless/main` is rebased onto upstream `master` (#24):

1. `git fetch upstream master`. If `reckless/main` already sits on it, there's nothing to do.
2. Tag the current head `pre-rebase/YYYY-MM-DD` and push the tag. A rebase rewrites every commit of ours, and the
   tag keeps the old SHAs reachable for anything that cites them.
3. Rebase on a branch `rebase/YYYY-MM-DD`. If a conflict can't be resolved mechanically, abort, open a `bug` issue
   naming the files and commits, and stop. Nothing is pushed.
4. Build with clang-20 and run the Altium import suites (`qa_eeschema` and `qa_pcbnew`, `Altium*`). Compare the
   warning count with the previous build.
5. `git push --force-with-lease=reckless/main:<old head> origin rebase/YYYY-MM-DD:reckless/main`, then rebase any
   open `fix/*` branches onto the new `reckless/main`.
6. Install. Comment on #24 with the old and new upstream base, the number of our commits and the test results.

## Building

As for KiCad (dev-docs.kicad.org, "Building on Linux"), with clang-20. We configure with
`-DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/opt/kicad-reckless -DKICAD_VERSION_EXTRA=reckless
-DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20` and set `CMAKE_INSTALL_RPATH` to the prefix's `lib` and
`lib/kicad/plugins/3d`, so the programs run without wrapper scripts.

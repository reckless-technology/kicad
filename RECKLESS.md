# KiCad, Reckless build

This is a modified version of [KiCad](https://www.kicad.org), maintained by Reckless Technology
(github.com/reckless-technology/kicad). It is not an official KiCad release and is not endorsed by the KiCad project.

- **Based on:** KiCad's development line (upstream `master`, gitlab.com/kicad/code/kicad), which becomes KiCad 11.
  Files it saves use the KiCad 11 development format, which KiCad 10 cannot open.
- **Modified:** from 2026-11-22. Branch `reckless/main` is upstream `master` plus our changes, rebased onto upstream
  from time to time; every change is a separate commit, listed by `git log upstream/master..reckless/main`.
- **Licence:** unchanged, GNU GPL version 3 or later (see `LICENSE` and `LICENSE.README`), as for KiCad itself.
- **Builds** identify themselves with a `-reckless` suffix in Help > About.

## What we change and why

Fixes first: the Altium importer faults we met converting our own designs, each also reported upstream
(gitlab.com/kicad/code/kicad issues #26126-#26133). Each fix is kept on its own branch from upstream `master`, so it
can be offered back to KiCad unchanged.

## Building

As for KiCad (dev-docs.kicad.org, "Building on Linux"). We configure with
`-DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/opt/kicad-reckless -DKICAD_VERSION_EXTRA=reckless`
and install beside the packaged KiCad, never over it.

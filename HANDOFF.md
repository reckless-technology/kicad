# Handoff: the Reckless KiCad fork and the Altium conversion

Written 2026-11-24, when work paused for the usage limit. Read `RECKLESS.md` first: it describes the fork, the build and
the rules.

## Where things stand

- **Fork:** `reckless/main` is at `9d16cbd97f`. It is upstream master plus our commits.
  - Merges are fast-forward pushes after a rebase (`git push origin <branch>:reckless/main`).
  - Rebase onto upstream only when Jonny asks. Nothing is posted upstream unless he asks, and anything posted there
    is in his own words.
  - The upstream GitLab issues are #26126–#26133 and #26009. Read them with `glab`, read-only.
- **Installed on the Linux Legion:** `/opt/kicad-reckless`, version `10.99.0-6958-g9d16cbd97f-reckless`, built with
  clang-20 and `KICAD_WERROR=ON` (`-Wall -Wextra -Wpedantic`). The Vulkan engine is compiled in; OpenGL is the
  default canvas.
- **Conversion:** finished and verified on this build.
  - Re-runs: hardware-kicad#30 (PR #31) and reckless-jester#2 (PR #4), both merged.
  - The pipeline is in `hardware-kicad/tools/altium-conversion/`. The working copy is in
    `~/work/kicad-import-experiment/tools/`.

## Half done: warnings as errors on all three platforms (#46, #43)

- **PR #60** (`fix/46-central-warning-flags`, head `fa8873e389`, on `9d16cbd97f`) applies the warning flags to all of
  KiCad's own code.
  - Clean on Linux (clang-20) and macOS (Apple clang 21, `KICAD_WERROR=ON`). Both results are on #46.
  - **Waiting on the MSVC check.**
- **Windows (#43), on jonny-precision**, in the session "KiCad Windows":
  - Five MSVC-only build fixes are on `fix/43-msvc-build-errors` (`73e326fbd7`, rebased on `9d16cbd97f`):
    - `near` is a macro
    - `TOKEN_TYPE` clashes with `winnt.h`
    - `wxS` literal `+` in two QA tests
    - a `vector<wxString>` LNK2005
  - QA on MSVC:
    - **common:** 1792 of 1792 passed.
    - **eeschema:** 1745 of 1745 passed.
    - **pcbnew:** 3019 of 3039, with one failure: a symlink test that needs Windows Developer Mode.
  - The checkout is `C:\Users\jonny\work\kicad`; the build is `build\msvc`. The brief is
    `C:\Users\jonny\work\kicad-win-brief.md`.
  - The session was told to push that branch and open a PR (Refs #43), then check PR #60 with MSVC, post on #46, and
    stop. Check #43, #46 and the branch list for how far it got.

## Next steps, in order

1. If `fix/43-msvc-build-errors` is pushed and its PR is open, review it, rebase, fast-forward `reckless/main`, and
   delete the branch.
2. If the MSVC check of PR #60 is clean (or once its fixes are in), do the following:
   - Rebase PR #60 and fast-forward it.
   - Delete the branch.
   - Close #46. A fast-forward does not fire `Closes #n`, so check the state.
3. Rebuild and install on Linux:
   - Build: `systemd-run --user --scope -q -p MemoryMax=20G -p MemorySwapMax=0 -p CPUQuota=1200% -- ninja -C build/clang20 -j12`
   - Install: `sudo -n ninja -C build/clang20 install && sudo -n chown -R jonny:jonny /opt/kicad-reckless build/clang20`
   - Check: `kicad-cli version`.
4. Then #43's remaining work: `/W4 /WX /permissive-` on MSVC, in `fix/43-*` branches. Don't investigate the symlink
   test unless asked.

## Backlog (GitHub issues, none started)

- **kicad:**
  - #8, #9 (Jonny to decide whether to close it), #10, #13, #36 (Altium import)
  - #39, #47–#50 (fonts)
  - #62, #63 (macOS)
  - #65 (a Reckless default look)
- **hardware-kicad:** #11 (port the test-board helpers off SWIG).

## Open questions for Jonny

- Close #9, or keep it?

## Machines

- **KiCad macOS** (jonny-macbook): offline since tonight's macOS update; Jonny chose not to relaunch it. Its work is
  done: the macOS share of #46 (PRs #54–#59) and the clean check of PR #60. Its brief is `~/work/kicad-mac-brief.md`
  on the Mac.
- **KiCad Windows** (jonny-precision): as described above.

## Rules that bite

- **Identity:** commits as `Jonny Reckless <jonny@recklesstechnology.com>`, author and committer.
- **Writing:** American spelling. Stage explicit paths.
- **Finish with cleanup:** after a merge, delete the branch and the worktree and close the issue.
- **Never:**
  - write to `~/work/hardware` or `~/work/cad-libs/altium`
  - run GUIs or inject keys on Jonny's desktop (use Xvfb)
  - kill processes by name
- **Built boards:** never change the electrics of a board that has already been built.
- **This repo is public:** no private details in it.
- **Scratch:** `~/wt/kicad-pcb-tools/` holds scratch helper scripts from the conversion work. It's not a repo; the
  versioned copies are in hardware-kicad.

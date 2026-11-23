#!/usr/bin/env python3
# This file is part of KiCad, a free EDA CAD application.
#
# Copyright (C) 2026 Reckless Technology.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""Mark unused function parameters with [[maybe_unused]] from clang's diagnostics.

Reads a build log (ninja output from a clang-20 build with -Wunused-parameter enabled)
and, for every unique warning in KiCad's own source, inserts " [[maybe_unused]]" directly
after the parameter name:

    int aLayer            ->  int aLayer [[maybe_unused]]
    int a = 0             ->  int a [[maybe_unused]] = 0

Diagnostics are accepted in both forms: "warning: unused parameter 'x' [-Wunused-parameter]"
and, when warnings are errors, "error: unused parameter 'x' [-Werror,-Wunused-parameter]".

Safety rules, each reported in the summary when they skip a warning:
  * the text at the reported line and column must be exactly the parameter name,
    on a word boundary; otherwise the warning is skipped;
  * warnings whose location is inside a macro expansion are skipped;
  * array parameters (name followed by '[') are skipped;
  * files under the build directory (generated code) are never edited; a generated
    file's warning must be fixed at its generator input;
  * files outside the source root, and anything under thirdparty/, are never edited.

Edits within a line are applied right to left, so the columns of earlier edits stay valid.
Duplicates (a header included by many translation units) are counted and edited once.

Usage, from the build directory of a configured tree (ninja's output carries paths relative to it):

    ninja -j12 -k 0 > build.log 2>&1
    python3 <source-root>/tools/reckless/mark_unused_parameters.py build.log \
        --build-dir . --source-root <source-root> --dry-run
    python3 <source-root>/tools/reckless/mark_unused_parameters.py build.log \
        --build-dir . --source-root <source-root>

Run it with --dry-run first, read the skipped list, then run it without the flag. Run it again
after any upstream rebase: a new build log names only the warnings still present.
"""

import argparse
import collections
import os
import re
import sys

MARKER = " [[maybe_unused]]"

DIAGNOSTIC = re.compile(
    r"^(?P<file>.+?):(?P<line>\d+):(?P<col>\d+): (?:warning|error): "
    r"unused parameter '(?P<name>[A-Za-z_][A-Za-z0-9_]*)' "
    r"\[-(?:Werror,-)?Wunused-parameter\]\s*$"
)

NEW_DIAGNOSTIC = re.compile(r"^\S.*?:\d+:\d+: (?:warning|error): ")


def read_warnings(log_path):
    """Yield (file, line, col, name, in_macro) for each unique unused-parameter warning block."""
    with open(log_path, encoding="utf-8", errors="replace") as handle:
        lines = handle.read().splitlines()

    seen = set()
    index = 0
    while index < len(lines):
        match = DIAGNOSTIC.match(lines[index])
        index += 1
        if not match:
            continue
        # The block runs until the next diagnostic header. A macro note belongs to the block.
        in_macro = False
        scan = index
        while scan < len(lines) and not NEW_DIAGNOSTIC.match(lines[scan]):
            if "expanded from macro" in lines[scan] or "in expansion of macro" in lines[scan]:
                in_macro = True
            scan += 1
        key = (os.path.normpath(match.group("file")), int(match.group("line")), int(match.group("col")), match.group("name"))
        if key in seen:
            continue
        seen.add(key)
        yield (key[0], key[1], key[2], key[3], in_macro)


def classify(path, build_dir, source_root):
    """Return (absolute path, None) for an editable file, or (None, reason) otherwise."""
    absolute = os.path.normpath(os.path.join(build_dir, path))
    build_root = os.path.normpath(build_dir)
    if absolute == build_root or absolute.startswith(build_root + os.sep):
        return None, "generated (under the build directory)"
    if not (absolute == source_root or absolute.startswith(source_root + os.sep)):
        return None, "outside the source root"
    relative = os.path.relpath(absolute, source_root)
    if relative.split(os.sep)[0] == "thirdparty":
        return None, "thirdparty (keeps its own flags)"
    return absolute, None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("log", help="build log containing clang's diagnostics")
    parser.add_argument("--build-dir", default=".", help="directory ninja ran in (default: current)")
    parser.add_argument("--source-root", required=True, help="root of the KiCad source tree")
    parser.add_argument("--dry-run", action="store_true", help="report what would change; edit nothing")
    arguments = parser.parse_args()

    build_dir = os.path.abspath(arguments.build_dir)
    source_root = os.path.abspath(arguments.source_root)

    unique = 0
    skipped = collections.Counter()
    skipped_detail = []
    edits_by_file = collections.defaultdict(list)

    for path, line, col, name, in_macro in read_warnings(arguments.log):
        unique += 1
        target, reason = classify(path, build_dir, source_root)
        if target is None:
            skipped[reason] += 1
            skipped_detail.append(f"{path}:{line}:{col}: {name}: {reason}")
            continue
        if in_macro:
            skipped["inside a macro expansion"] += 1
            skipped_detail.append(f"{path}:{line}:{col}: {name}: inside a macro expansion")
            continue
        edits_by_file[target].append((line, col, name))

    applied = 0
    for target, edits in sorted(edits_by_file.items()):
        with open(target, encoding="utf-8", newline="") as handle:
            text = handle.read()
        lines = text.split("\n")
        # Right to left within each line, so earlier columns stay valid.
        for line, col, name in sorted(edits, key=lambda edit: (edit[0], -edit[1])):
            if line < 1 or line > len(lines):
                skipped["line out of range"] += 1
                skipped_detail.append(f"{target}:{line}:{col}: {name}: line out of range")
                continue
            content = lines[line - 1]
            start = col - 1
            end = start + len(name)
            if content[start:end] != name:
                skipped["text at column is not the name"] += 1
                skipped_detail.append(f"{target}:{line}:{col}: {name}: text at column is not the name")
                continue
            after = content[end:end + 1]
            before = content[start - 1:start] if start > 0 else ""
            if (after and (after.isalnum() or after == "_")) or (before and (before.isalnum() or before == "_")):
                skipped["name is part of a longer word"] += 1
                skipped_detail.append(f"{target}:{line}:{col}: {name}: name is part of a longer word")
                continue
            if content[end:].startswith(MARKER):
                skipped["already marked"] += 1
                continue
            rest = content[end:].lstrip()
            if rest.startswith("["):
                skipped["array parameter"] += 1
                skipped_detail.append(f"{target}:{line}:{col}: {name}: array parameter")
                continue
            if content[end:].startswith(MARKER) or content[end:].startswith(" [[maybe_unused]]"):
                skipped["already marked"] += 1
                continue
            lines[line - 1] = content[:end] + MARKER + content[end:]
            applied += 1
        if not arguments.dry_run and text != "\n".join(lines):
            with open(target, "w", encoding="utf-8", newline="") as handle:
                handle.write("\n".join(lines))

    print(f"unique warnings: {unique}")
    print(f"files to edit:   {len(edits_by_file)}")
    print(f"{'would mark' if arguments.dry_run else 'marked'}:        {applied}")
    print("skipped:")
    if not skipped:
        print("  none")
    for reason, count in skipped.most_common():
        print(f"  {count:6d}  {reason}")
    if skipped_detail:
        print("skipped, by location:")
        for detail in skipped_detail:
            print(f"  {detail}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

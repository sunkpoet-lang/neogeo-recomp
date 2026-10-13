#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#
# Keeps a project's copy of recomp-common (the in-game menu, the settings rules, the launcher) and checks that
# nobody changed it. Put this script in a tools/ folder of your project; the copy goes beside that folder, in
# recomp-common/:
#
#   <somewhere>/tools/sync-common.sh
#   <somewhere>/recomp-common/          menu.h, menu.cpp, settings.h, ... (recomp-common's src/), COMMIT
#   <somewhere>/recomp-common/launcher/ launcher.cmake, src/, example/ (recomp-common's launcher/)
#
#   tools/sync-common.sh           copy src/ and launcher/ as committed there (HEAD) and record the commit
#   tools/sync-common.sh --check   fail if the copy is not what was recorded (run this in your checks)
#
# A clone of recomp-common is looked for beside your repository (../recomp-common), or where RECOMP_COMMON
# says. The copy is committed in your repository, which so builds from a fresh clone on its own. Never
# change the copy by hand: change recomp-common, commit there, and run this.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
dest="$here/../recomp-common"
root=$(git -C "$here" rev-parse --show-toplevel)
src=${RECOMP_COMMON:-$root/../recomp-common}
record="$dest/COMMIT"

if command -v shasum > /dev/null; then sum="shasum -a 256"; else sum="sha256sum"; fi
# the copy's files and their hashes, one a line
hashes() {
    (cd "$dest" && find . -type f ! -name COMMIT | sed 's|^\./||' | LC_ALL=C sort | while read -r f; do
        printf '%s  %s\n' "$($sum < "$f" | cut -d' ' -f1)" "$f"
    done)
}

if [ "${1:-}" = "--check" ]; then
    [ -f "$record" ] || { echo "sync-common: no $record; run tools/sync-common.sh" >&2; exit 1; }
    commit=$(sed -n 's/^commit //p' "$record")
    if [ "$(hashes)" != "$(grep -v '^#' "$record" | grep -v '^commit ')" ]; then
        echo "sync-common: recomp-common/ is not what recomp-common $commit gave (changed by hand?)." >&2
        echo "  Change recomp-common instead and run tools/sync-common.sh." >&2
        exit 1
    fi
    # With recomp-common at hand and knowing the commit: file by file against it too
    if git -C "$src" cat-file -e "$commit^{commit}" 2> /dev/null; then
        for f in $(cd "$dest" && find . -type f ! -name COMMIT | sed 's|^\./||'); do
            case "$f" in launcher/*) theirs="$f" ;; *) theirs="src/$f" ;; esac
            if ! git -C "$src" show "$commit:$theirs" 2> /dev/null | cmp -s - "$dest/$f"; then
                echo "sync-common: recomp-common/$f differs from recomp-common $commit" >&2
                exit 1
            fi
        done
        if [ "$(git -C "$src" ls-tree -r --name-only "$commit" src launcher | wc -l)" -ne "$(hashes | wc -l)" ]; then
            echo "sync-common: recomp-common/ has not the files of recomp-common $commit" >&2
            exit 1
        fi
    fi
    echo "recomp-common/: commit $commit"
    exit 0
fi

git -C "$src" rev-parse HEAD > /dev/null 2>&1 || { echo "sync-common: no recomp-common at $src (set RECOMP_COMMON)" >&2; exit 1; }
commit=$(git -C "$src" rev-parse HEAD)
if [ -n "$(git -C "$src" status --porcelain -- src launcher)" ]; then
    echo "sync-common: note: $src has changes in src/ or launcher/ not committed; copying them as committed ($commit)" >&2
fi
# (only ever a copy this script made is replaced)
if [ -e "$dest" ] && [ ! -f "$record" ]; then
    echo "sync-common: $dest is there and is not a copy made by this script; leaving it alone" >&2
    exit 1
fi
if [ -d "$dest" ]; then
    find "$dest" -type f -delete
fi
mkdir -p "$dest"
git -C "$src" archive "$commit" src | tar -x -C "$dest" --strip-components=1
if git -C "$src" cat-file -e "$commit:launcher" 2> /dev/null; then
    git -C "$src" archive "$commit" launcher | tar -x -C "$dest"
fi
{
    echo "# A copy of recomp-common's src/ and launcher/, made by tools/sync-common.sh. Do not change it here."
    echo "commit $commit"
    hashes
} > "$record"
echo "recomp-common/: commit $commit"

#!/usr/bin/env bash
#
# sync-x265.sh — mechanical upstream sync of the embedded x265 kernel package.
#
# Usage:  src/tools/sync-x265.sh [upstream-ref] [--dry-run]
#   upstream-ref   branch or tag to sync (default: 4.2, the latest stable tag;
#                  any ref accepted by `git fetch origin <ref>` works, e.g.
#                  master or Release_4.0)
#   --dry-run      clone/update /tmp/upstream-x265 and print the copy plan
#                  without writing anything into the repo
#
# Upstream:  https://bitbucket.org/multicoreware/x265_git
#            fallback mirror: https://github.com/videolan/x265
# Clone:     /tmp/upstream-x265 (reference only, never inside this repo)
#
# What this script does:
#   1. clones (or updates) /tmp/upstream-x265 at the chosen ref;
#   2. copies the DSP-relevant C++ sources AND the asm dirs together into
#      src/x265/ — C and asm must always move together, the asm-offsets
#      compile-time check is the safety net if they ever drift apart;
#   3. copies upstream COPYING to src/x265/COPYING;
#   4. NEVER overwrites local-only files (PROTECTED below: bench_x265.cpp,
#      CMakeLists.txt, x265shim.h, x265_shim.cpp, and the local minimal shim
#      headers cpu.h / threading.h which replace the upstream ones). Anything
#      not in the copy lists is left untouched, and nothing is ever deleted.
#
# Copied source files (COPY_FILES and every file inside COPY_DIRS, including
# the asm dirs) get the same mechanical transform the port was created
# with (see PORTING.md): x265 needs no symbol rename (it is C++, namespace
# x265 — the asm symbols are native x265_ via X265_NS), only the include
# rewrite
#     #include "common.h" -> #include "x265shim.h"
# plus dropping the two upstream includes the shim replaces ("x265.h",
# "slicetype.h" — LOWRES_COST_MASK is provided by x265shim.h).
# Never edit synced files by hand — edit x265shim.h instead.
#
# Acceptance after a sync (from the repo root):
#     cmake --build build -j8 && ./build/vbench --bench=x265:
#   the x265 rows must appear with the C column (and ISA columns once the
#   asm is wired) and all checks must pass.
#
set -euo pipefail

CODEC=x265
DEFAULT_REF=4.2
URLS=(
    "https://bitbucket.org/multicoreware/x265_git"
    "https://github.com/videolan/x265"
)
UPSTREAM_DIR=/tmp/upstream-x265

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
DEST_DIR=$REPO_ROOT/src/$CODEC
BUILD_DIR=${BUILD_DIR:-build}

# Upstream path | dest path relative to src/x265/ | mode (sed = transformed, raw = verbatim)
# mode defaults to sed when omitted.
COPY_FILES=(
    "source/common/pixel.cpp|pixel.cpp"
    "source/common/dct.cpp|dct.cpp"
    "source/common/intrapred.cpp|intrapred.cpp"
    "source/common/ipfilter.cpp|ipfilter.cpp"
    "source/common/loopfilter.cpp|loopfilter.cpp"
    "source/common/constants.cpp|constants.cpp"
    "source/common/primitives.h|primitives.h"
    "source/common/contexts.h|contexts.h"
    "source/common/constants.h|constants.h"
    "COPYING|COPYING|raw"
)

# Upstream dir | dest dir relative to src/x265/ — copied with the same
# include-shim transform as COPY_FILES (asm moves together with the C++
# above; upstream also has ppc/riscv64/vec dirs and 32-bit arm
# (source/common/arm), which this port does not use — add them here if that
# changes).
COPY_DIRS=(
    "source/common/x86|x86"
    "source/common/aarch64|aarch64"
)

# Local-only files this script must never overwrite (or create). Anything under
# src/x265/ that is not in COPY_FILES/COPY_DIRS is likewise never touched.
# cpu.h and threading.h are deliberately local: they are minimal shims that
# replace the upstream headers (upstream cpu.h / threading.h are NOT copied).
# sao.h is a local minimal shim for upstream encoder/sao.h (only SAO_BO_BITS
# is needed by aarch64/sao-prim.cpp).
PROTECTED=(
    "bench_x265.cpp"
    "CMakeLists.txt"
    "x265shim.h"
    "x265_shim.cpp"
    "cpu.h"
    "threading.h"
    "sao.h"
)

usage() {
    sed -n '2,31p' "$0" | sed 's/^# \{0,1\}//'
}

is_protected() {
    local p rel=$1
    for p in "${PROTECTED[@]}"; do
        [ "$rel" = "$p" ] && return 0
    done
    return 1
}

# Mechanical transform applied to copied source files (stdout). No symbol
# rename for x265 (namespace x265); only the shim include rewrite.
transform_file() {
    sed -e 's/#include "common.h"/#include "x265shim.h"/' \
        -e '/^#include "x265\.h"$/d' \
        -e '/^#include "slicetype\.h"/d' \
        "$1"
}

check_dst() {
    case $1 in
        *..*|/*) echo "error: refusing unsafe dest path '$1'" >&2; exit 1 ;;
    esac
    if is_protected "$1"; then
        echo "error: '$1' is a local-only file (PROTECTED) — refusing to overwrite it" >&2
        exit 1
    fi
}

copy_file() { # <upstream-rel> <dest-rel> <mode>
    local up_rel=$1 dst_rel=$2 mode=${3:-sed}
    local src=$UPSTREAM_DIR/$up_rel dst=$DEST_DIR/$dst_rel
    check_dst "$dst_rel"
    if [ ! -f "$src" ]; then
        echo "error: upstream path '$up_rel' not found in $UPSTREAM_DIR (ref '$REF'?)" >&2
        exit 1
    fi
    if [ "$DRY_RUN" = 1 ]; then
        printf 'dry-run:  %-34s -> src/%s/%s  (%s)\n' "$up_rel" "$CODEC" "$dst_rel" "$mode"
        return 0
    fi
    mkdir -p "$(dirname "$dst")"
    if [ "$mode" = raw ]; then
        cp -a "$src" "$dst"
    else
        transform_file "$src" > "$dst.vbsync.$$"
        mv "$dst.vbsync.$$" "$dst"
    fi
    printf 'copied:   %-34s -> src/%s/%s\n' "$up_rel" "$CODEC" "$dst_rel"
}

copy_dir() { # <upstream-dir-rel> <dest-dir-rel>  (transformed per file; never deletes)
    local up_rel=$1 dst_rel=$2
    local src=$UPSTREAM_DIR/$up_rel dst=$DEST_DIR/$dst_rel
    check_dst "$dst_rel"
    if [ ! -d "$src" ]; then
        echo "error: upstream dir '$up_rel' not found in $UPSTREAM_DIR (ref '$REF'?)" >&2
        exit 1
    fi
    if [ "$DRY_RUN" = 1 ]; then
        printf 'dry-run:  %-34s -> src/%s/%s/  (dir)\n' "$up_rel" "$CODEC" "$dst_rel"
        return 0
    fi
    local f rel
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        rel=${f#"$src"/}
        check_dst "$dst_rel/$rel"
        mkdir -p "$dst/$(dirname "$rel")"
        transform_file "$f" > "$dst/$rel.vbsync.$$"
        mv "$dst/$rel.vbsync.$$" "$dst/$rel"
    done < <(find "$src" -type f | sort)
    printf 'copied:   %-34s -> src/%s/%s/  (dir)\n' "$up_rel" "$CODEC" "$dst_rel"
}

update_upstream() {
    if [ -d "$UPSTREAM_DIR/.git" ]; then
        echo ">>> updating $UPSTREAM_DIR to ref '$REF'"
        if ! git -C "$UPSTREAM_DIR" fetch --depth 1 origin "$REF"; then
            echo "error: cannot fetch ref '$REF' (pass a branch or tag, e.g. '$DEFAULT_REF' or master)" >&2
            exit 1
        fi
        git -C "$UPSTREAM_DIR" checkout -q --detach FETCH_HEAD
    else
        if [ -e "$UPSTREAM_DIR" ]; then
            echo "error: $UPSTREAM_DIR exists but is not a git clone — remove it manually" >&2
            exit 1
        fi
        local url
        for url in "${URLS[@]}"; do
            echo ">>> cloning $url (ref '$REF') into $UPSTREAM_DIR"
            if git clone --depth 1 --single-branch --branch "$REF" "$url" "$UPSTREAM_DIR"; then
                return 0
            fi
            echo ">>> clone from $url failed, trying next URL" >&2
        done
        echo "error: could not clone upstream $CODEC from any known URL" >&2
        exit 1
    fi
}

main() {
    REF=$DEFAULT_REF
    DRY_RUN=0
    local arg ref_set=0
    for arg in "$@"; do
        case $arg in
            --dry-run) DRY_RUN=1 ;;
            -h|--help) usage; exit 0 ;;
            -*) echo "unknown option: $arg" >&2; usage >&2; exit 2 ;;
            *)  if [ "$ref_set" = 1 ]; then
                    echo "error: only one upstream ref may be given" >&2; exit 2
                fi
                REF=$arg; ref_set=1 ;;
        esac
    done

    update_upstream
    echo ">>> syncing $CODEC @ $(git -C "$UPSTREAM_DIR" rev-parse --short HEAD) into $DEST_DIR"
    [ "$DRY_RUN" = 1 ] && echo ">>> DRY RUN — nothing will be written to the repo"

    local entry up_rel dst_rel mode
    for entry in "${COPY_FILES[@]}"; do
        IFS='|' read -r up_rel dst_rel mode <<< "$entry"
        copy_file "$up_rel" "$dst_rel" "${mode:-sed}"
    done
    for entry in "${COPY_DIRS[@]}"; do
        IFS='|' read -r up_rel dst_rel <<< "$entry"
        copy_dir "$up_rel" "$dst_rel"
    done

    if [ "$DRY_RUN" = 1 ]; then
        echo ">>> dry run complete — no files in $DEST_DIR were modified"
    else
        echo ">>> sync complete"
    fi
    echo
    echo "Acceptance (from the repo root, $REPO_ROOT):"
    echo "  cmake --build $BUILD_DIR -j8 && ./$BUILD_DIR/vbench --bench=$CODEC:"
    echo "Review the sync with: git -C $REPO_ROOT status --short -- src/$CODEC"
}

main "$@"

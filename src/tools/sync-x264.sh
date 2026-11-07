#!/usr/bin/env bash
#
# sync-x264.sh — mechanical upstream sync of the embedded x264 kernel package.
#
# Usage:  src/tools/sync-x264.sh [upstream-ref] [--dry-run]
#   upstream-ref   branch or tag to sync (default: stable — x264 publishes no
#                  tags, its `stable` branch is the latest stable line; any
#                  ref accepted by `git fetch origin <ref>` works, e.g. master)
#   --dry-run      clone/update /tmp/upstream-x264 and print the copy plan
#                  without writing anything into the repo
#
# Upstream:  https://code.videolan.org/videolan/x264.git
#            fallback mirror: https://github.com/videolan/x264.git
# Clone:     /tmp/upstream-x264 (reference only, never inside this repo)
#
# What this script does:
#   1. clones (or updates) /tmp/upstream-x264 at the chosen ref;
#   2. copies the DSP-relevant C sources AND the asm dirs together into
#      src/x264/ — C and asm must always move together, the asm-offsets
#      compile-time check is the safety net if they ever drift apart;
#   3. copies upstream COPYING to src/x264/LICENSE;
#   4. NEVER overwrites local-only files (PROTECTED below: bench_x264.c,
#      CMakeLists.txt, vb_shim.h). Anything not in the copy lists is left
#      untouched, and nothing is ever deleted.
#
# The copied files are a PLAIN copy of upstream: since src/x264 is now the
# single x264 in the tree (the 2016 src/c_kernels + src/asm pair is deleted),
# no symbol renaming is needed anymore — the package keeps the native
# upstream x264_* symbol names, so diffs against upstream stay clean. The
# only mechanical transform left is the include shim:
#     #include "common.h" / "base.h" / "common/common.h" / "common/base.h"
#         -> #include "vb_shim.h"
# applied to the copied .c/.h files (also inside the asm dirs, whose C
# helpers mc-c.c/predict-c.c/asm-offsets.c include upstream's common.h);
# .asm/.S files are copied verbatim (they include the generated config.h).
# Never edit synced files by hand — edit vb_shim.h instead.
#
# Acceptance after a sync (from the repo root):
#     cmake --build build -j8 && ./build/vbench --bench=x264:
#   the x264 rows must appear with the C, C-plain and ISA columns populated
#   and all checks must pass.
#
set -euo pipefail

CODEC=x264
DEFAULT_REF=stable
URLS=(
    "https://code.videolan.org/videolan/x264.git"
    "https://github.com/videolan/x264.git"
)
UPSTREAM_DIR=/tmp/upstream-x264

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
DEST_DIR=$REPO_ROOT/src/$CODEC
BUILD_DIR=${BUILD_DIR:-build}

# Upstream path | dest path relative to src/x264/
COPY_FILES=(
    "common/bitstream.c|bitstream.c"
    "common/cabac.c|cabac.c"
    "common/dct.c|dct.c"
    "common/deblock.c|deblock.c"
    "common/mc.c|mc.c"
    "common/pixel.c|pixel.c"
    "common/predict.c|predict.c"
    "common/quant.c|quant.c"
    "common/tables.c|vb_tables.c"
    "common/bitstream.h|bitstream.h"
    "common/cabac.h|cabac.h"
    "common/dct.h|dct.h"
    "common/mc.h|mc.h"
    "common/pixel.h|pixel.h"
    "common/predict.h|predict.h"
    "common/quant.h|quant.h"
    "COPYING|LICENSE"
)

# Upstream dirs copied into src/x264/ — the asm moves together with the C
# above. .c/.h files inside get the include-shim rewrite; .asm/.S are copied
# verbatim. (upstream also has arm/ppc/mips/loongarch/opencl asm dirs which
# this port does not use — add them here if that changes.)
COPY_DIRS=(
    "common/x86|x86"
    "common/aarch64|aarch64"
)

# Local-only files this script must never overwrite (or create). Anything under
# src/x264/ that is not in COPY_FILES/COPY_DIRS is likewise never touched.
PROTECTED=(
    "bench_x264.c"
    "CMakeLists.txt"
    "vb_shim.h"
)

usage() {
    sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
}

is_protected() {
    local p rel=$1
    for p in "${PROTECTED[@]}"; do
        [ "$rel" = "$p" ] && return 0
    done
    return 1
}

# Mechanical transform applied to copied C sources/headers (stdout): plain
# copy of upstream except the include shim (vb_shim.h replaces upstream's
# common.h/base.h). No symbol renaming — the package uses native x264_* names.
transform_file() {
    sed -e 's|#include "common/common.h"|#include "vb_shim.h"|' \
        -e 's|#include "common/base.h"|#include "vb_shim.h"|' \
        -e 's|#include "common.h"|#include "vb_shim.h"|' \
        -e 's|#include "base.h"|#include "vb_shim.h"|' \
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

copy_file() { # <upstream-rel> <dest-rel> <mode: shim|raw>
    local up_rel=$1 dst_rel=$2 mode=$3
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

copy_dir() { # <upstream-dir-rel> <dest-dir-rel>  (per file; never deletes)
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
    mkdir -p "$dst"
    local f rel mode
    while IFS= read -r -d '' f; do
        rel=${f#"$src"/}
        check_dst "$dst_rel/$rel"
        case "$rel" in
            *.c|*.h) mode=shim ;;
            *)        mode=raw ;;
        esac
        if [ "$mode" = raw ]; then
            mkdir -p "$(dirname "$dst/$rel")"
            cp -a "$f" "$dst/$rel"
        else
            mkdir -p "$(dirname "$dst/$rel")"
            transform_file "$f" > "$dst/$rel.vbsync.$$"
            mv "$dst/$rel.vbsync.$$" "$dst/$rel"
        fi
        printf 'copied:   %-34s -> src/%s/%s\n' "$up_rel/$rel" "$CODEC" "$dst_rel/$rel"
    done < <(find "$src" -type f -print0)
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

    local entry up_rel dst_rel
    for entry in "${COPY_FILES[@]}"; do
        IFS='|' read -r up_rel dst_rel <<< "$entry"
        if [ "$dst_rel" = "LICENSE" ]; then
            copy_file "$up_rel" "$dst_rel" raw
        else
            copy_file "$up_rel" "$dst_rel" shim
        fi
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

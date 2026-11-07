#!/usr/bin/env bash
#
# sync-dav1d.sh — mechanical upstream sync of the embedded dav1d kernel package.
#
# Usage:  src/tools/sync-dav1d.sh [upstream-ref] [--dry-run]
#   upstream-ref   branch or tag to sync (default: 1.5.4, the latest stable
#                  tag; any ref accepted by `git fetch origin <ref>` works,
#                  e.g. master)
#   --dry-run      clone/update /tmp/upstream-dav1d and print the copy plan
#                  without writing anything into the repo
#
# Upstream:  https://code.videolan.org/videolan/dav1d.git
#            fallback mirror: https://github.com/videolan/dav1d
# Clone:     /tmp/upstream-dav1d (reference only, never inside this repo)
#
# What this script does:
#   1. clones (or updates) /tmp/upstream-dav1d at the chosen ref;
#   2. copies the DSP-relevant C sources (8bpc tmpl + plain C) AND the asm
#      dirs together into src/dav1d/ — C and asm must always move together,
#      the asm-offsets compile-time check is the safety net if they ever
#      drift apart;
#   3. copies the upstream headers (src/*.h, include/common/*.h,
#      include/dav1d/*.h) and upstream COPYING to src/dav1d/COPYING;
#   4. NEVER overwrites local-only files (PROTECTED below: bench_dav1d.c,
#      CMakeLists.txt, the hand-written config.h, and the local *_8bpc.c
#      bitdepth wrappers). Anything not in the copy lists is left untouched,
#      and nothing is ever deleted.
#
# Upstream path mapping (dav1d keeps its src/ tree shape under src/dav1d/):
#     upstream src/<name>_tmpl.c   -> src/dav1d/<name>_tmpl.inc   (renamed .c -> .inc;
#                                 the local <name>_8bpc.c wrappers set BITDEPTH and
#                                 #include the .inc, like dav1d's meson build)
#     upstream src/{itx_1d,scan,tables}.c -> src/dav1d/*.c
#     upstream src/*.h             -> src/dav1d/src/*.h
#     upstream include/common/*.h  -> src/dav1d/common/*.h
#     upstream include/dav1d/*.h   -> src/dav1d/dav1d/*.h
#     upstream src/{x86,arm}/      -> src/dav1d/src/{x86,arm}/      (asm, verbatim)
#
# Copied source files get the same mechanical transform the port was created
# with (see PORTING.md):
#     dav1d_ -> vbdav1d_
# Never edit synced files by hand — edit the local shim headers (config.h and
# the *_8bpc.c wrappers) instead.
#
# Acceptance after a sync (from the repo root):
#     cmake --build build -j8 && ./build/vbench --bench=dav1d:
#   the dav1d rows must appear with the C column (and ISA columns once the
#   asm is wired) and all checks must pass.
#
set -euo pipefail

CODEC=dav1d
DEFAULT_REF=1.5.4
URLS=(
    "https://code.videolan.org/videolan/dav1d.git"
    "https://github.com/videolan/dav1d"
)
UPSTREAM_DIR=/tmp/upstream-dav1d

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
DEST_DIR=$REPO_ROOT/src/$CODEC
BUILD_DIR=${BUILD_DIR:-build}

# Upstream path | dest path relative to src/dav1d/ | mode (sed = transformed, raw = verbatim)
# mode defaults to sed when omitted.
COPY_FILES=(
    "src/cdef_tmpl.c|cdef_tmpl.inc"
    "src/ipred_tmpl.c|ipred_tmpl.inc"
    "src/itx_tmpl.c|itx_tmpl.inc"
    "src/loopfilter_tmpl.c|loopfilter_tmpl.inc"
    "src/mc_tmpl.c|mc_tmpl.inc"
    "src/itx_1d.c|itx_1d.c"
    "src/scan.c|scan.c"
    "src/tables.c|tables.c"
    "COPYING|COPYING|raw"
)

# Upstream headers, copied per-file with the transform:
#   upstream <dir> matching <pattern> -> dest dir relative to src/dav1d/
#     upstream src/*.h            -> src/
#     upstream include/common/*.h -> common/
#     upstream include/dav1d/*.h  -> dav1d/
COPY_HEADER_GLOBS=(
    "src|*.h|src"
    "include/common|*.h|common"
    "include/dav1d|*.h|dav1d"
)

# Upstream dir | dest dir relative to src/dav1d/ — copied verbatim (asm moves
# together with the C above; upstream also has ppc/riscv/loongarch asm dirs
# which this port does not use — add them here if that changes).
COPY_DIRS=(
    "src/x86|src/x86"
    "src/arm|src/arm"
)

# Local-only files this script must never overwrite (or create). Anything under
# src/dav1d/ that is not in the copy lists is likewise never touched. The
# *_8bpc.c files are local bitdepth wrappers (upstream has no such files);
# config.h is the local hand-written replacement for the meson-generated one.
PROTECTED=(
    "bench_dav1d.c"
    "CMakeLists.txt"
    "config.h"
    "cdef_8bpc.c"
    "ipred_8bpc.c"
    "itx_8bpc.c"
    "loopfilter_8bpc.c"
    "mc_8bpc.c"
)

usage() {
    sed -n '2,38p' "$0" | sed 's/^# \{0,1\}//'
}

is_protected() {
    local p rel=$1
    for p in "${PROTECTED[@]}"; do
        [ "$rel" = "$p" ] && return 0
    done
    return 1
}

# Mechanical transform applied to copied source files (stdout).
transform_file() {
    sed -e 's/dav1d_/vbdav1d_/g' "$1"
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

copy_dir() { # <upstream-dir-rel> <dest-dir-rel>  (verbatim; never deletes)
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
    cp -a "$src/." "$dst/"
    printf 'copied:   %-34s -> src/%s/%s/\n' "$up_rel" "$CODEC" "$dst_rel"
}

copy_glob() { # <upstream-dir-rel> <name-pattern> <dest-dir-rel>  (per-file, transformed)
    local dir_rel=$1 pattern=$2 dst_dir=$3 f rel
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        rel=${f#"$UPSTREAM_DIR"/}
        copy_file "$rel" "$dst_dir/$(basename "$f")" sed
    done < <(find "$UPSTREAM_DIR/$dir_rel" -maxdepth 1 -type f -name "$pattern" | sort)
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
    local dir_rel pattern
    for entry in "${COPY_HEADER_GLOBS[@]}"; do
        IFS='|' read -r dir_rel pattern dst_rel <<< "$entry"
        copy_glob "$dir_rel" "$pattern" "$dst_rel"
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

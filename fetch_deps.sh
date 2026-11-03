#!/bin/sh
# Re-clone the upstream codec sources that the ported kernels in kernels/
# were taken from. third_party/ is reference-only and not tracked by git.
set -e
cd "$(dirname "$0")"
mkdir -p third_party
[ -d third_party/x264/.git ]  || git clone --depth 1 https://code.videolan.org/videolan/x264.git  third_party/x264
[ -d third_party/x265/.git ]  || git clone --depth 1 https://bitbucket.org/multicoreware/x265_git.git third_party/x265
[ -d third_party/dav1d/.git ] || git clone --depth 1 https://code.videolan.org/videolan/dav1d.git third_party/dav1d
echo "third_party clones up to date."

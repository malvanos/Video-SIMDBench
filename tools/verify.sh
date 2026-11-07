#!/bin/sh
# tools/verify.sh — regression script for Video-SIMDBench 2.0.
#
# Builds the project in a fresh temporary build dir, then runs vbench with a
# fixed seed and asserts on its output:
#   * stderr contains "VideoBench: All tests passed"
#   * the stdout timing table header carries both the "C" and "C-plain" columns
#   * every table row is codec-prefixed (x264:/x265:/dav1d:) — the per-category
#     harness bench modules are gone, so total rows == codec rows
#   * at least 100 codec-prefixed rows
#   * a "--bench=x264:" filtered run still passes and emits only x264: rows
#
# Prints PASS/FAIL per check and exits nonzero if any check fails.
# Portable POSIX sh (macOS + Linux). The temp dir is removed on exit.

set -eu

REPO=$(CDPATH= cd -P "$(dirname -- "$0")/.." && pwd)
SEED=12345
MIN_CODEC_ROWS=100
ROW_RE='^[[:space:]]+[^ ]+.* :'                         # table rows: "<name> : <values>"
HEADER_C_RE='^[[:space:]]+:[[:space:]]*C([[:space:]]|$)' # header line carrying the C column
HEADER_PLAIN_RE='^[[:space:]]+:[[:space:]]*C[[:space:]]+C-plain([[:space:]]|$)' # ... and C-plain right after C

# ------------------------------------------------------------------ temp dir --
TMP=$(mktemp -d)
cleanup() {
    if [ -n "${TMP:-}" ] && [ "$TMP" != "/" ] && [ -d "$TMP" ]; then
        rm -rf "$TMP"
    fi
    return 0
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

# -------------------------------------------------------------- parallelism --
if [ "$(uname -s)" = "Darwin" ] && command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu)
elif command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc)
elif command -v sysctl >/dev/null 2>&1; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=4
fi

# ------------------------------------------------------------------- helpers --
FAILS=0
pass() { printf 'PASS: %s\n' "$1"; }
fail() { printf 'FAIL: %s\n' "$1"; FAILS=$((FAILS + 1)); }

echo "verify.sh: repo   = $REPO"
echo "verify.sh: tmpdir = $TMP (removed on exit)"
echo "verify.sh: jobs   = $JOBS"

# --------------------------------------------------------------------- build --
echo "==> cmake configure + build in $TMP/build ..."
if cmake -S "$REPO" -B "$TMP/build" >"$TMP/configure.log" 2>&1 &&
   cmake --build "$TMP/build" -j"$JOBS" >"$TMP/build.log" 2>&1; then
    pass "build: cmake -S <repo> -B <tmp>/build && cmake --build <tmp>/build -j$JOBS"
else
    fail "build: cmake -S <repo> -B <tmp>/build && cmake --build <tmp>/build -j$JOBS"
    echo "--- last 40 lines of configure/build logs ---" >&2
    tail -n 40 "$TMP/configure.log" "$TMP/build.log" >&2 || true
fi

VBENCH="$TMP/build/vbench"
if [ ! -x "$VBENCH" ]; then
    echo "verify.sh: $VBENCH missing, aborting remaining checks" >&2
    echo "RESULT: FAIL - $FAILS check(s) failed"
    exit 1
fi
pass "vbench binary built: $VBENCH"

# -------------------------------------------------- full run: --bench 12345 --
echo "==> running: vbench --bench $SEED (full table) ..."
RC=0
"$VBENCH" --bench "$SEED" >"$TMP/full.out" 2>"$TMP/full.err" || RC=$?
grep '^VideoBench:' "$TMP/full.err" | sed 's/^/    /' || true

if [ "$RC" -eq 0 ] && grep -q 'VideoBench: All tests passed' "$TMP/full.err"; then
    pass "full run (vbench --bench $SEED): exit 0 and stderr contains 'VideoBench: All tests passed'"
else
    fail "full run (vbench --bench $SEED): exit $RC, stderr lacks 'VideoBench: All tests passed'"
    tail -n 5 "$TMP/full.err" >&2 || true
fi

if grep -qE "$HEADER_C_RE" "$TMP/full.out"; then
    pass "stdout table header line contains the C column"
else
    fail "stdout table header line with C column not found"
    grep -nE '^[[:space:]]*:' "$TMP/full.out" >&2 || true
fi

if grep -qE "$HEADER_PLAIN_RE" "$TMP/full.out"; then
    pass "stdout table header line contains the C-plain column right after C"
else
    fail "stdout table header line with C-plain column not found"
    grep -nE '^[[:space:]]*:' "$TMP/full.out" >&2 || true
fi

rows=$(grep -cE "$ROW_RE" "$TMP/full.out" || true); rows=${rows:-0}

codec_rows=$(grep -E "$ROW_RE" "$TMP/full.out" | grep -cE '^[[:space:]]*(x264:|x265:|dav1d:)' || true)
codec_rows=${codec_rows:-0}
if [ "$codec_rows" -ge "$MIN_CODEC_ROWS" ]; then
    pass "codec-prefixed rows (x264:/x265:/dav1d:) $codec_rows >= $MIN_CODEC_ROWS"
else
    fail "codec-prefixed rows (x264:/x265:/dav1d:) $codec_rows < $MIN_CODEC_ROWS"
fi

# the per-category harness bench modules are deleted: every table row must be
# a codec row, i.e. total rows == codec-prefixed rows
if [ "$rows" -gt 0 ] && [ "$rows" -eq "$codec_rows" ]; then
    pass "all $rows table rows are codec-prefixed (no harness rows left)"
else
    fail "table rows $rows != codec-prefixed rows $codec_rows (harness rows still present?)"
    grep -E "$ROW_RE" "$TMP/full.out" | sed 's/^[[:space:]]*//' | grep -vE '^(x264:|x265:|dav1d:)' >&2 || true
fi

# ---------------------------------------------- filtered run: --bench=x264: --
echo "==> running: vbench --bench=x264: $SEED (filtered) ..."
RC=0
"$VBENCH" "--bench=x264:" "$SEED" >"$TMP/x264.out" 2>"$TMP/x264.err" || RC=$?
grep '^VideoBench:' "$TMP/x264.err" | sed 's/^/    /' || true

if [ "$RC" -eq 0 ] && grep -q 'VideoBench: All tests passed' "$TMP/x264.err"; then
    pass "filtered run (vbench --bench=x264: $SEED): exit 0 and stderr contains 'VideoBench: All tests passed'"
else
    fail "filtered run (vbench --bench=x264: $SEED): exit $RC, stderr lacks 'VideoBench: All tests passed'"
    tail -n 5 "$TMP/x264.err" >&2 || true
fi

xrows=$(grep -cE "$ROW_RE" "$TMP/x264.out" || true); xrows=${xrows:-0}
if [ "$xrows" -ge 1 ]; then
    pass "filtered run produced $xrows table row(s)"
else
    fail "filtered run produced no table rows"
fi

non_x264=$(grep -E "$ROW_RE" "$TMP/x264.out" | sed 's/^[[:space:]]*//' | grep -vcE '^x264:' || true)
non_x264=${non_x264:-0}
if [ "$non_x264" -eq 0 ]; then
    pass "filtered run rows are all x264:-prefixed (0 non-x264 rows of $xrows)"
else
    fail "filtered run has $non_x264 non-x264: row(s) of $xrows"
    grep -E "$ROW_RE" "$TMP/x264.out" | sed 's/^[[:space:]]*//' | grep -vE '^x264:' >&2 || true
fi

# ------------------------------------------------------------------- summary --
echo
if [ "$FAILS" -gt 0 ]; then
    echo "RESULT: FAIL - $FAILS check(s) failed"
    exit 1
fi
echo "RESULT: OK - all checks passed"
exit 0

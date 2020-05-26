# DO NOT CALL THIS SCRIPT DIRECTLY.
# Run `meson test` on the build dir.

unset EMILUA_COLORS

AWKSCRIPT="$1"
TEST="$2"

"$EMILUA_BIN" "$TEST.lua" 2>&1 | "$AWK_BIN" -v TEST="$TEST" -f "$AWKSCRIPT"

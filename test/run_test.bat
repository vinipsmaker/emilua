:: DO NOT CALL THIS SCRIPT DIRECTLY.
:: Run `meson test` on the build dir.
@ECHO OFF

MKDIR "%TEMP%\emilua-test" 2> NUL
ECHO > "%TEMP%\emilua-test\%~n3"

( "%EMILUA_BIN%" "%3.lua" 2>&1 && DEL "%TEMP%\emilua-test\%~n3" ) | "%AWK_BIN%" -v "TEST=%3" -f "%APPVEYOR_FIXUP%" -f "%2"

IF ERRORLEVEL 1 (
    EXIT /B %ERRORLEVEL%
) ELSE IF EXIST "%TEMP%\emilua-test\%~n3" (
    DEL "%TEMP%\emilua-test\%~n3"
    EXIT /B 2
) ELSE (
    EXIT /B 0
)

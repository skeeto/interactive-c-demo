@echo off

rem build dll
if exist libgame.dll move libgame.dll libgame.temp%random%.dll
cl game.c /D_USRDLL /D_WINDLL /Iwin32 /link /DLL /OUT:libgame.dll

rem build & run viewer
if exist main.exe move main.exe main.temp%random%.exe
cl main.c /I win32 win32\*.c Psapi.lib kernel32.lib
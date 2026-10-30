@echo off

if not exist .\Build mkdir .\Build

set CCOpts=/DINTERNAL_BUILD /I..\raylib\include /Od /Zi /FeAircraftMap.exe
set CLOpts=/INCREMENTAL:NO /OPT:REF /NODEFAULTLIB:LIBCMT /LIBPATH:..\raylib\lib

pushd .\Build
cl ..\Code\AircraftMapMain.cpp %CCOpts% shell32.lib gdi32.lib msvcrt.lib raylib.lib winmm.lib user32.lib -link %CLOpts%
popd

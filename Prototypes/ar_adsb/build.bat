@echo off

if not exist build mkdir build

set cc_opts=/Od /Zi /DINTERNAL_BUILD
set cl_opts=/INCREMENTAL:NO /OPT:REF

pushd build
cl ..\ADSB.cpp %cc_opts% user32.lib wininet.lib -link %cl_opts%
popd
@echo off

if not exist build mkdir build

set cc_opts=/Od /Zi /DINTERNAL_BUILD
set cl_opts=/INCREMENTAL:NO /OPT:REF

pushd build
cl ..\UDP.cpp %cc_opts% -link %cl_opts%
popd


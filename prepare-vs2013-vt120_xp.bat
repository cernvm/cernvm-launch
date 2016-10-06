@echo off

set CONFIG=Release
if %1%a==a goto begin_config
set CONFIG=%1
if %CONFIG%==Release goto begin_config
if %CONFIG%==Debug goto begin_config
echo Configuration can either be 'Debug' or 'Release'!
goto end

:begin_config
echo Acquiring required submodules [requires git]...
git submodule init
git submodule update

set BUILDDIR=build
if not exist "%BUILDDIR%" mkdir "%BUILDDIR%"
cd "%BUILDDIR%"
cmake -Wno-dev -DCMAKE_BUILD_TYPE=%CONFIG% -DTARGET_ARCH="i386" -G"Visual Studio 12 2013" -T"v120_xp" ..

:end

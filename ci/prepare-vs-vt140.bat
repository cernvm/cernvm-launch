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

REM Run C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\Tools\Vs...bat to setup environment

set BUILDDIR=build
if not exist "%BUILDDIR%" mkdir "%BUILDDIR%"
cd "%BUILDDIR%"
cmake -DCMAKE_BUILD_TYPE=%CONFIG% -DSYSTEM_BOOST=ON -DBOOST_ROOT="C:/Boost" -DBOOST_LIBRARYDIR="C:/Boost/lib"  -DBOOST_INCLUDEDIR="C:/Boost/include" -DTARGET_ARCH="i386" -T"v140" ..

:end

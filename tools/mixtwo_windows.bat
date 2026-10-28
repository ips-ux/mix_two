@ECHO OFF
REM Mix Two local Windows build helper (MSVC Build Tools + Ninja + Mixxx buildenv).
REM   tools\mixtwo_windows.bat configure      Configure build\x64__portable
REM   tools\mixtwo_windows.bat build [target] Build everything or one target (e.g. mixxx-test)
REM   tools\mixtwo_windows.bat test [gtest args]  Run mixxx-test, e.g. --gtest_filter=*Midi*
REM   tools\mixtwo_windows.bat run [--nobuild] [mixxx args]
REM                                               Build everything, then run Mix Two with a
REM                                               separate dev profile (MIXTWO_PROFILE, default
REM                                               build\mixtwo-profile), e.g. run --controller-debug.
REM                                               --nobuild starts the current build without building.
SETLOCAL
SET MIXXX_ROOT=%~dp0..
FOR %%I IN ("%MIXXX_ROOT%") DO SET MIXXX_ROOT=%%~fI
SET BUILD_DIR=%MIXXX_ROOT%\build\x64__portable
SET BUILDENV_NAME=mixxx-deps-2.7-x64-windows-1c20f84a
SET MIXXX_VCPKG_ROOT=%MIXXX_ROOT%\buildenv\%BUILDENV_NAME%
SET DEPS=%MIXXX_VCPKG_ROOT%\installed\x64-windows
IF NOT DEFINED MIXTWO_BUILDTOOLS SET MIXTWO_BUILDTOOLS=Z:\Program Files\BuildTools
SET NINJA=%MIXTWO_BUILDTOOLS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe

IF NOT DEFINED MIXTWO_PROFILE SET MIXTWO_PROFILE=%MIXXX_ROOT%\build\mixtwo-profile

IF /I "%1"=="test" GOTO :TEST
IF /I "%1"=="run" GOTO :RUN
IF /I "%1"=="configure" GOTO :CONFIGURE
IF /I "%1"=="build" GOTO :BUILD
ECHO Usage: %~nx0 configure ^| build [target] ^| test [gtest args] ^| run [--nobuild] [mixxx args]
EXIT /B 1

REM Exit codes are returned with a top-level EXIT /B only: inside a
REM parenthesized block %ERRORLEVEL% is read too early, and "cmd /c" loses an
REM EXIT /B there.

:CONFIGURE
REM vcvars64 reports a missing vswhere.exe on stderr; it is harmless.
CALL "%MIXTWO_BUILDTOOLS%\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1 || EXIT /B 1
cmake -S "%MIXXX_ROOT%" -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_TOOLCHAIN_FILE="%MIXXX_VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ^
    -DMIXXX_VCPKG_ROOT="%MIXXX_VCPKG_ROOT%" ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DBUILDENV_BASEPATH="%MIXXX_ROOT%\buildenv" ^
    -DBUILDENV_URL=https://downloads.mixxx.org/dependencies/2.7/Windows/%BUILDENV_NAME%.zip ^
    -DBUILDENV_SHA256=77b75ab17f06e07b1c140e90f86aafb8658d66d8d581d488ab9b9545996fdc6e ^
    -DGIT_DESCRIBE=2.7-mixtwo ^
    -DOPTIMIZE=portable ^
    -DQT6=ON -DSTATIC_DEPS=OFF -DKEYFINDER=OFF ^
    -DBATTERY=ON -DBROADCAST=ON -DBULK=ON -DHID=ON -DHSS1394=ON ^
    -DFFMPEG=ON -DLILV=ON -DMAD=ON -DMEDIAFOUNDATION=ON -DMODPLUG=ON ^
    -DOPUS=ON -DQTKEYCHAIN=ON -DVINYLCONTROL=ON -DWAVPACK=ON ^
    -DLOCALECOMPARE=ON -DDEBUG_ASSERTIONS_FATAL=ON ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
EXIT /B %ERRORLEVEL%

:BUILD
CALL "%MIXTWO_BUILDTOOLS%\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1 || EXIT /B 1
"%NINJA%" -C "%BUILD_DIR%" %2
EXIT /B %ERRORLEVEL%

:RUN
REM Forward the raw arguments, as for :TEST.
SET "RUN_ARGS=%*"
CALL SET "RUN_ARGS=%%RUN_ARGS:*run=%%"
IF NOT DEFINED RUN_ARGS SET "RUN_ARGS= "
REM --nobuild skips the build and starts the current one.
SET NOBUILD=
IF NOT "%RUN_ARGS:--nobuild=%"=="%RUN_ARGS%" (
    SET NOBUILD=1
    CALL SET "RUN_ARGS=%%RUN_ARGS:--nobuild=%%"
)
IF DEFINED NOBUILD GOTO :RUN_START
REM Build everything first, so that the app always matches the source.
ECHO Building Mix Two. Use "run --nobuild" to start the current build instead.
CALL "%MIXTWO_BUILDTOOLS%\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1 || EXIT /B 1
"%NINJA%" -C "%BUILD_DIR%"
IF ERRORLEVEL 1 GOTO :RUN_BUILD_FAILED
:RUN_START
IF NOT EXIST "%BUILD_DIR%\mixxx.exe" GOTO :RUN_NOT_BUILT
REM Keep Mix Two away from an installed Mixxx's profile in %%LOCALAPPDATA%%\Mixxx.
IF NOT EXIST "%MIXTWO_PROFILE%" MKDIR "%MIXTWO_PROFILE%"
SET PATH=%DEPS%\bin;%DEPS%\Qt6\bin;%PATH%
SET QT_PLUGIN_PATH=%DEPS%\Qt6\plugins
SET QML_IMPORT_PATH=%DEPS%\Qt6\qml
SET QML2_IMPORT_PATH=%DEPS%\Qt6\qml
ECHO Profile: %MIXTWO_PROFILE%
"%BUILD_DIR%\mixxx.exe" --settings-path "%MIXTWO_PROFILE%" %RUN_ARGS%
EXIT /B %ERRORLEVEL%

:RUN_BUILD_FAILED
ECHO Build failed; not starting Mix Two.
EXIT /B 1

:RUN_NOT_BUILT
ECHO mixxx.exe not found. Build it first: %~nx0 build mixxx
EXIT /B 1

:TEST
REM Forward the raw arguments: %1..%9 would split --gtest_filter=... at "=".
SET "TEST_ARGS=%*"
CALL SET "TEST_ARGS=%%TEST_ARGS:*test=%%"
SET PATH=%DEPS%\bin;%DEPS%\Qt6\bin;%PATH%
SET QT_PLUGIN_PATH=%DEPS%\Qt6\plugins
SET QML_IMPORT_PATH=%DEPS%\Qt6\qml
SET QML2_IMPORT_PATH=%DEPS%\Qt6\qml
SET QT_QPA_PLATFORM=offscreen
cd /d "%BUILD_DIR%"
.\mixxx-test.exe %TEST_ARGS%
EXIT /B %ERRORLEVEL%

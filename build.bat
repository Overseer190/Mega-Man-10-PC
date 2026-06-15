@echo off
REM ===========================================================================
REM build.bat - Build MegaMan 10 Remake and copy assets to out/
REM ===========================================================================

setlocal

set SDL_DLL=SDL3-3.4.10\x86_64-w64-mingw32\bin\SDL3.dll
set BASS_DLL=bass24\x64\bass.dll
set BASSMIDI_DLL=bassmidi24\x64\bassmidi.dll
set SF2_SRC=src\Music\MIDI\MM10.sf2
set MID_SRC=src\Music\MIDI\14_StrikeMan.mid
set WAV_SRC=src\SFX\SoundEffect.wav

REM ----------------------------------------------------------------------
REM 1. Build via Makefile
REM ----------------------------------------------------------------------
echo [BUILD] Compiling...
make all
if %ERRORLEVEL% neq 0 (
    echo [BUILD] FAILED  --  see errors above.
    exit /b %ERRORLEVEL%
)

REM ----------------------------------------------------------------------
REM 2. Copy DLLs and assets alongside the exe
REM ----------------------------------------------------------------------
if exist "%SDL_DLL%" (
    copy /Y "%SDL_DLL%" out\SDL3.dll >nul
    echo [BUILD] SDL3.dll copied
) else (
    echo [BUILD] WARNING: %SDL_DLL% not found -- copy manually.
)

if exist "%BASS_DLL%" (
    copy /Y "%BASS_DLL%" out\bass.dll >nul
    echo [BUILD] bass.dll copied
) else (
    echo [BUILD] WARNING: %BASS_DLL% not found
)

if exist "%BASSMIDI_DLL%" (
    copy /Y "%BASSMIDI_DLL%" out\bassmidi.dll >nul
    echo [BUILD] bassmidi.dll copied
) else (
    echo [BUILD] WARNING: %BASSMIDI_DLL% not found
)

if exist "%SF2_SRC%" (
    copy /Y "%SF2_SRC%" out\MM10.sf2 >nul
    echo [BUILD] MM10.sf2 copied
) else (
    echo [BUILD] WARNING: %SF2_SRC% not found
)

if exist "%MID_SRC%" (
    copy /Y "%MID_SRC%" out\14_StrikeMan.mid >nul
    echo [BUILD] 14_StrikeMan.mid copied
) else (
    echo [BUILD] WARNING: %MID_SRC% not found
)

if exist "%WAV_SRC%" (
    copy /Y "%WAV_SRC%" out\SoundEffect.wav >nul
    echo [BUILD] SoundEffect.wav copied
) else (
    echo [BUILD] WARNING: %WAV_SRC% not found
)

echo [BUILD] Done.  Run with:  out\megaman10.exe
exit /b 0

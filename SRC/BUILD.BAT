@echo off

REM This file assumes fixed locations for the installations of Borland C++ 3.0,
REM Borland C++ 3.1, and Turbo C++ 3.0. Adjust as needed.
REM Borland C++ 3.1 is optional, a precompiled OBJ file will be used if it's
REM not found at the expected location.

set BCCROOT_30=C:\BORLANDC
set BCCROOT_31=C:\BCC_31
set TCROOT=C:\TC

if "%1" == "-s" goto shareware

set SHAREWARE_FLAG=
goto set_cflags

:shareware
set SHAREWARE_FLAG=-DSHAREWARE

:set_cflags
set CFLAGS=-I%BCCROOT_30%\INCLUDE -L%BCCROOT_30%\LIB %SHAREWARE_FLAG%


%BCCROOT_30%\BIN\tasm /d__MEDIUM__ /m2 /i%BCCROOT_30%\LIB\STARTUP C0.ASM
if errorlevel 1 goto err


if NOT EXIST %BCCROOT_31% goto skip_digisnd

%BCCROOT_31%\BIN\bcc -1- -O2 -Z -K- -b -c -oDIGISND\DIGISND.OBJ -I%BCCROOT_31%\INCLUDE DIGISND\SRC\DIGISND.C
if errorlevel 1 goto err

:skip_digisnd


%TCROOT%\BIN\tcc %CFLAGS% -rd -c BASICSND.C
if errorlevel 1 goto err

%TCROOT%\BIN\tcc %CFLAGS% -c UNIT1.C
if errorlevel 1 goto err

%TCROOT%\BIN\tcc %CFLAGS% -c UNIT2.C
if errorlevel 1 goto err

%BCCROOT_30%\BIN\tlink /C /d /s /m C0.OBJ BASICSND.OBJ DIGISND\DIGISND.OBJ UNIT1.OBJ UNIT2.OBJ, NUKEM2RE.EXE, , %BCCROOT_30%\LIB\CM.LIB
if errorlevel 1 goto err

LZEXE\LZEXE NUKEM2RE.EXE

:err

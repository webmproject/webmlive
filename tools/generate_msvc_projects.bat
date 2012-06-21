@echo off
setlocal

REM Copyright (c) 2012 The WebM project authors. All Rights Reserved.
REM
REM Use of this source code is governed by a BSD-style license
REM that can be found in the LICENSE file in the root of the source
REM tree. An additional intellectual property rights grant can be found
REM in the file PATENTS.  All contributing project authors may
REM be found in the AUTHORS file in the root of the source tree.

REM Generates webmlive MSVC projects. Note that GYP uses settings in the
REM environment to control project file generation, and that they're going
REM to be overridden here to force output of projects and solutions for
REM multiple versions of MSVC.

IF NOT %~0==tools\generate_msvc_projects.bat (
  echo This script must be run from your webmlive root.
  exit /B
)

set original_dir=%cd%
set dshow_name="direct_show_base_classes"
set encoder_name="client_encoder"

REM Make sure only MSVC projects are generated-- not supporting ninja
REM in webmlive, for example.
set GYP_GENERATORS=msvs

REM Make sure GYP is defined, and that it points at a file that exists.
IF NOT DEFINED GYP (
  CALL :die "GYP environment variable not defined"
)
IF NOT EXIST %GYP% (
  CALL :die "%GYP% does not exist"
)

REM Remove existing files
del client_encoder\*.vcproj
del client_encoder\*.vcxproj
del client_encoder\*.vcxproj.filters
del client_encoder\*.sln
del client_encoder\win\*.vcproj
del client_encoder\win\*.vcxproj
del client_encoder\win\*.vcxproj.filters
del client_encoder\win\*.sln

REM TODO(tomfinegan): Move duped code into a function(s).

REM
REM Generate the MSVC 2008 projects and solutions.
REM
set GYP_MSVS_VERSION=2008
chdir client_encoder

REM Generate the projects and solutions.
CALL %GYP%

REM Rename the client_encoder files.
CALL :RenameMSVCFiles %encoder_name% 2008 vcproj
CALL :RenameMSVCFiles %encoder_name% 2008 sln

REM Correct project file names in the client_encoder solution file.
perl -pi.orig -e "s/.vcproj/_2008.vcproj/g" %renamed_file%
REM A backup file is stored because in place edits seem to trigger
REM permissions issues on Win 7. Delete it.
del %renamed_file%.orig

REM Do the same for the direct_show_base_classes standalone solution.
chdir win
CALL :RenameMSVCFiles %dshow_name% 2008 vcproj
CALL :RenameMSVCFiles %dshow_name% 2008 sln
perl -pi.orig -e "s/.vcproj/_2008.vcproj/g" %renamed_file%
del %renamed_file%.orig

REM
REM Generate the MSVC 2010 project and solutions.
REM
set GYP_MSVS_VERSION=2010
chdir %original_dir%\client_encoder

REM Generate the projects and solutions.
CALL %GYP%

REM Rename the client_encoder files, and patch references to the vcxproj.
CALL :RenameMSVCFiles %encoder_name% 2010 vcxproj
perl -pi.orig -e "s/.vcxproj/_2010.vcxproj/g" %renamed_file%
del %renamed_file%.orig
CALL :RenameMSVCFiles %encoder_name% 2010 sln
perl -pi.orig -e "s/.vcxproj/_2010.vcxproj/g" %renamed_file%
del %renamed_file%.orig

REM Do the same for the direct_show_base_classes standalone solution.
chdir win
CALL :RenameMSVCFiles %dshow_name% 2010 vcxproj
CALL :RenameMSVCFiles %dshow_name% 2010 sln
perl -pi.orig -e "s/.vcxproj/_2010.vcxproj/g" %renamed_file%
del %renamed_file%.orig

chdir %original_dir%
echo Done.
goto :EOF

REM Utility function used to rename project and solution files so that
REM their names contain the year of the MSVC release they are intended
REM for use within.
:RenameMSVCFiles
  set name=%~1
  set year=%~2
  set ext=%~3
  set original=%name%.%ext%
  set renamed=%name%_%year%.%ext%
  rename %original% %renamed%
  endlocal & set renamed_file=%renamed%
  IF NOT EXIST %renamed_file% (
    CALL :die "%renamed_file% does not exist"
  )
goto :EOF

:die
  set message=%~1
  IF NOT ""=="%message%" (
    echo ERROR: %message%, giving up.
  ) ELSE (
    echo ERROR: unspecified error, giving up.
  )
  endlocal
  echo Ignore the syntax error: it's how this script kills itself.
  () creates syntax error, and terminates script.
goto :EOF

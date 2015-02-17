#!/bin/bash
##
##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
##
. $(dirname $0)/../common/common.sh

cleanup() {
  local readonly res=$?
  cd "${ORIG_PWD}"

  if [[ $res -ne 0 ]]; then
    elog "cleanup() trapped error"
  fi
  if [[ "${KEEP_WORK_DIR}" != "yes" ]]; then
    if [[ -n "${WORK_DIR}" ]] && [[ -d "${WORK_DIR}" ]]; then
      rm -rf "${WORK_DIR}"
    fi
  fi
}

update_libwebm_usage() {
cat << EOF
  Usage: ${0##*/} [arguments]
    --help: Display this message and exit.
    --git-hash <git hash/ref/tag>: Use this revision instead of HEAD.
    --keep-work-dir: Keep the work directory.
    --show-program-output: Show output from each step.
    --verbose: Show more output.
EOF
}

# Runs cmake to generate projects for MSVC.
cmake_generate_projects() {
  local readonly cmake_lists_dir="${THIRD_PARTY}/${WORK_DIR}/${LIBWEBM}"
  local readonly orig_pwd="$(pwd)"

  vlog "Generating MSVC projects..."

  for (( i = 0; i < ${#CMAKE_MSVC_GENERATORS[@]}; ++i )); do
    mkdir -p "${CMAKE_PROJECT_DIRS[$i]}"
    cd "${CMAKE_PROJECT_DIRS[$i]}"
    eval cmake.exe "${cmake_lists_dir}" -G "\"${CMAKE_MSVC_GENERATORS[$i]}\"" \
      ${devnull}
    cd "${orig_pwd}"
  done

  vlog "Done."
}

# Runs msbuild.exe on projects generated by cmake_generate_projects().
build_libs() {
  local readonly vcxproj="libwebm.vcxproj"
  local readonly orig_pwd="$(pwd)"

  vlog "Building libwebm..."

  for project_dir in ${CMAKE_PROJECT_DIRS[@]}; do
    cd "${project_dir}"

    for config in ${CMAKE_BUILD_CONFIGS[@]}; do
      eval msbuild.exe "${vcxproj}" -p:Configuration="${config}" ${devnull}
    done

    cd "${orig_pwd}"
  done

  vlog "Done."
}

# Installs libwebm includes, libraries, and PDB files. Includes come from the
# git repo in $WORK_DIR/$LIBWEBM. Libraries and PDB files are picked up from the
# output locations of the projects built by build_libs().
install_libwebm() {
  local readonly target_dir="${THIRD_PARTY}/libwebm"

  vlog "Installing includes..."
  mkdir -p "${target_dir}"
  cp -rp "${THIRD_PARTY}/${WORK_DIR}/${LIBWEBM}/"*.h "${target_dir}"
  cp -rp "${THIRD_PARTY}/${WORK_DIR}/${LIBWEBM}/"*.hpp "${target_dir}"
  vlog "Done."

  # CMake generated vcxproj files place PDB files in config-named subdirs of
  # projname.dir. Projname.dir (libwebm.dir here) is a sibling of the actual
  # library build directory: Build an array of paths that can be used alongside
  # $CMAKE_BUILD_DIRS to copy pdb files where they belong.
  local readonly pdb_name="vc120.pdb"
  local readonly cmake_pdb_files=(
    ${CMAKE_PROJECT_DIRS[0]}/libwebm.dir/${CMAKE_BUILD_CONFIGS[0]}/${pdb_name}
    ${CMAKE_PROJECT_DIRS[0]}/libwebm.dir/${CMAKE_BUILD_CONFIGS[1]}/${pdb_name}
    ${CMAKE_PROJECT_DIRS[1]}/libwebm.dir/${CMAKE_BUILD_CONFIGS[0]}/${pdb_name}
    ${CMAKE_PROJECT_DIRS[1]}/libwebm.dir/${CMAKE_BUILD_CONFIGS[1]}/${pdb_name})

  vlog "Installing libraries..."
  local readonly lib_name="libwebm.lib"
  for (( i = 0; i < ${#CMAKE_BUILD_DIRS[@]}; ++i  )); do
    mkdir -p "${target_dir}/${LIBWEBM_TARGET_LIB_DIRS[$i]}"
    cp -p "${CMAKE_BUILD_DIRS[$i]}/${lib_name}" \
      "${target_dir}/${LIBWEBM_TARGET_LIB_DIRS[$i]}"
    cp -p "${cmake_pdb_files[$i]}" "${target_dir}/${LIBWEBM_TARGET_LIB_DIRS[$i]}"
  done
  vlog "Done."
}

# Clones libwebm and then runs cmake_generate_projects(), build_libs(), and
# install_libwebm() to update webmlive's version of the libwebm includes and
# libraries.
update_libwebm() {
  # Clone libwebm and checkout the desired ref.
  vlog "Cloning ${LIBWEBM_GIT_URL}..."
  eval git clone "${LIBWEBM_GIT_URL}" "${LIBWEBM}" ${devnull}
  git_checkout "${GIT_HASH}" "${LIBWEBM}"
  vlog "Done."

  # Generate projects using CMake.
  cmake_generate_projects

  # Build generated projects.
  build_libs

  # Install includes and libraries.
  install_libwebm
}

trap cleanup EXIT

# Defaults for command line options.
GIT_HASH="HEAD"
KEEP_WORK_DIR=""
MAKE_JOBS="1"

# Parse the command line.
while [[ -n "$1" ]]; do
  case "$1" in
    --help)
      update_libwebm_usage
      exit
      ;;
    --git-hash)
      GIT_HASH="$2"
      shift
      ;;
    --keep-work-dir)
      KEEP_WORK_DIR="yes"
      ;;
    --show-program-output)
      devnull=
      ;;
    --verbose)
      VERBOSE="yes"
      ;;
    *)
      update_libwebm_usage
      exit 1
      ;;
  esac
  shift
done

readonly CMAKE_BUILD_CONFIGS=(Debug RelWithDebInfo)
readonly CMAKE_MSVC_GENERATORS=("Visual Studio 12 Win64"
                                "Visual Studio 12")
readonly CMAKE_PROJECT_DIRS=(cmake_projects/x64 cmake_projects/x86)
readonly CMAKE_BUILD_DIRS=(${CMAKE_PROJECT_DIRS[0]}/${CMAKE_BUILD_CONFIGS[0]}
                           ${CMAKE_PROJECT_DIRS[0]}/${CMAKE_BUILD_CONFIGS[1]}
                           ${CMAKE_PROJECT_DIRS[1]}/${CMAKE_BUILD_CONFIGS[0]}
                           ${CMAKE_PROJECT_DIRS[1]}/${CMAKE_BUILD_CONFIGS[1]})
readonly LIBWEBM="libwebm"
readonly LIBWEBM_GIT_URL="https://chromium.googlesource.com/webm/libwebm"
readonly LIBWEBM_TARGET_LIB_DIRS=(win/x64/debug
                                  win/x64/release
                                  win/x86/debug
                                  win/x86/release)
readonly THIRD_PARTY="$(pwd)"
readonly WORK_DIR="tmp_$(date +%Y%m%d_%H%M%S)"

if [[ "${VERBOSE}" = "yes" ]]; then
cat << EOF
  KEEP_WORK_DIR=${KEEP_WORK_DIR}
  GIT_HASH=${GIT_HASH}
  MAKE_JOBS=${MAKE_JOBS}
  VERBOSE=${VERBOSE}
  CMAKE_BUILD_CONFIGS=${CMAKE_BUILD_CONFIGS[@]}
  CMAKE_BUILD_DIRS=${CMAKE_BUILD_DIRS[@]}
  CMAKE_MSVC_GENERATORS=${CMAKE_MSVC_GENERATORS[@]}
  CMAKE_PROJECT_DIRS=${CMAKE_PROJECT_DIRS[@]}
  LIBWEBM=${LIBWEBM}
  LIBWEBM_GIT_URL=${LIBWEBM_GIT_URL}
  LIBWEBM_TARGET_LIB_DIRS=${LIBWEBM_TARGET_LIB_DIRS[@]}
  THIRD_PARTY=${THIRD_PARTY}
  WORK_DIR=${WORK_DIR}
EOF
fi

# Bail out if not running from the third_party directory.
check_dir "third_party"

# cmake.exe, git.exe, and msbuild.exe are ultimately required. Die when
# unavailable.
check_cmake
check_git
check_msbuild

# Make a work directory, and cd into to avoid making a mess.
mkdir "${WORK_DIR}"
cd "${WORK_DIR}"

# Update stuff.
update_libwebm

# Print the new text for README.webmlive.
cat << EOF

Libwebm includes and libraries updated. README.webmlive information follows:

libwebm
Version: $(git_revision ${LIBWEBM})
URL: ${LIBWEBM_GIT_URL}

EOF


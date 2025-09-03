#!/usr/bin/env bash

#
# prepare.sh - Preparation script to build FORTRAN program
# This script will work on Linux and Linux distros.
#
# Copyright (C) 2025 Nguyen Duy Thanh (@Nekkochan0x0007). All right reserved
#
# With reference from Intel Corporation. All the references is
# Copyright (C) 2019 - 2023 Intel Corporation. All right reserved.
# Please refer to the Intel Corporation for more information.
#
# This file is a part of NeRF project
#
# USAGE:
#       sh prepare.sh
# or:
#       chmod +x prepare.sh
#       ./prepare.sh
#
# PARAMETERS:
#       This script doesn't require any parameters.
#
# REQUIREMENTS:
#       +) You must use Linux or *NIX operating system. Microsoft Windows completely unsupported. 
#          and macOS partially compatibility
#
#          NOTE: If you're Mac user, please use this script with your warranty. This script not
#                tested on Mac systems. Apple Sillicon is completely unsupported.
#
#       +) You must have a Intel(R) oneAPI Fortran Compiler installed
#       +) Your Intel(R) oneAPI Fortran Compiler must be configured properly and installed in
#          /opt/intel. If you have installed in the other location, please edit the line OPT_INTEL=
#          in this script.
#
#          NOTE: You must provide absolute path, not the relative path
#
# AUTHORS:
#       Nguyen Duy Thanh (@Nekkochan0x0007)
# 
# DATE AND TIME:
#       09/03/2025 at 10:31 AM
#

###
# VARIABLES DEFINITIONS
###

#
# Please update this if you have installed Intel(R) oneAPI Fortran Compiler
# in another location not /opt/intel
#
# I highly recommended not changing the install location
#
OPT_INTEL=/opt/intel

# Resolve script directory robustly
if [ -n "$BASH_SOURCE" ]; then
    SCRIPT_PATH="$BASH_SOURCE"
else
    SCRIPT_PATH="$0"
fi
SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_PATH")" && pwd)"

###
# REQUIREMENTS CHECKER:
## Checking the Intel(R) oneAPI Fortran Compiler installation
###

if [ ! -d "$OPT_INTEL" ]; then
    echo "[!] Intel(R) oneAPI Fortran Compiler not found in $OPT_INTEL"
    echo "[!] Please install Intel(R) oneAPI Fortran Compiler and re-run this script"
    exit 1
fi

if [ ! -f "$OPT_INTEL/oneapi/setvars.sh" ]; then
    echo "[!] Intel(R) oneAPI Fortran Compiler configuration (setvars.sh) not found"
    echo "[!] Expected at: $OPT_INTEL/oneapi/setvars.sh"
    exit 1
fi

echo "[i] Intel(R) oneAPI Fortran Compiler found in $OPT_INTEL"
echo "[i] Intel(R) oneAPI Fortran Compiler configuration check done"
echo

### INFO
cat <<'EOF'
Preparation script

Copyright (C) 2025 Nguyen Duy Thanh (@Nekkochan).
With reference from Intel Corporation. All the references is
Copyright (C) 2019 - 2023 Intel Corporation. All right reserved.
Please refer to the Intel Corporation for more information.

This script is a part of NeRF project.

---------------------------------
Before continuing, please read this IMPORTANT note:

To build this project, you will need:
   + Linux
     (Windows is unsupported, even with MSYS2, Cygwin, or WSL.
      macOS is partially supported, Apple Silicon unsupported.)

   + Intel(R) oneAPI Fortran Compiler installed in default location.
     If you installed it elsewhere, edit the line OPT_INTEL= in this script.

If you encounter errors, check all requirements and re-run the script.
---------------------------------
EOF

echo

### INITIALIZE ONEAPI ENVIRONMENT
echo "[i] Initializing oneAPI environment ..."
. "$OPT_INTEL/oneapi/setvars.sh" >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "[!] Failed to initialize oneAPI environment"
    exit 1
fi

export STAGE1_DONE="OK"

### RUN STAGE 2
if ! . "$SCRIPT_DIR/prepare_stage2.sh"; then
    echo "[!] Stage 2 failed"
    exit 1
fi

echo "[i] All preparation stages finished successfully"
#!/usr/bin/env bash

#
# prepare_stage2.sh - Preparation script to build FORTRAN program
# This script will work on Linux and Linux distros (Stage 2)
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
#       09/03/2025 at 10:59 AM
#

set -euo pipefail

if [[ "${STAGE1_DONE:-}" != "OK" ]]; then
    echo "[!] Stage 1 is not done yet"
    exit 1
fi

# Check compiler
if ! command -v ifx >/dev/null 2>&1; then
    echo "[!] Intel(R) oneAPI Fortran Compiler not found in PATH"
    exit 1
fi

COMPILER_VERSION=$(ifx -v 2>&1 | head -n1 || true)
echo "[i] Using Intel(R) Fortran Compiler: $COMPILER_VERSION"

# Enter compiler_tests dir (relative to this script)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/compiler_tests" || exit 1

# Build
echo "[i] Building test program..."
if ! make all; then
    echo "[!] Compilation failed"
    exit 1
fi

if [[ ! -x ./compiler_tests ]]; then
    echo "[!] Build succeeded but binary not found"
    exit 1
fi

# Run the binary directly
echo "[i] Running test binary..."
if ! ./compiler_tests; then
    echo "[!] Execution failed"
    exit 1
fi

echo
echo "[i] Preparation script finished successfully"
echo

# Cleanup
echo "[i] Cleaning up..."
make clean || { echo "[!] Cleanup failed"; exit 1; }

# Reset variable
unset STAGE1_DONE
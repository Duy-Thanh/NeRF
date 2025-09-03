#
# Makefile - Main build configuration for NeRF Big Data Processing
# NeRF (Neural Radiance Fields for Big Data Processing with MapReduce)
#
# Copyright (C) 2025 Nguyen Duy Thanh (@Nekkochan0x0007). All right reserved
# Copyright (C) 2025 NeRF Team. All right reserved
#
# With reference from Intel Corporation. All the references are
# Copyright (C) 2019 - 2023 Intel Corporation. All right reserved.
# Please refer to the Intel Corporation for more information.
#
# This file is a part of NeRF project
#

###
# COMPILER CONFIGURATION
###

# Intel oneAPI Fortran Compiler
# We need to source the Intel oneAPI environment for each compilation
INTEL_SETUP = . /opt/intel/oneapi/setvars.sh >/dev/null 2>&1 &&
FC_WITH_ENV = $(INTEL_SETUP) ifx
FC = ifx
F90 = ifx

# Check if Intel oneAPI is available
check-compiler:
	@echo "[i] Checking Intel oneAPI Fortran Compiler availability..."
	@if [ ! -f /opt/intel/oneapi/setvars.sh ]; then \
		echo "[!] Intel oneAPI not found at /opt/intel/oneapi/setvars.sh"; \
		echo "[!] Please install Intel oneAPI Fortran Compiler"; \
		exit 1; \
	fi
	@bash -c '$(INTEL_SETUP) ifx -v >/dev/null 2>&1' || \
		{ echo "[!] Intel Fortran Compiler not working"; \
		  echo "[!] Please run: ./prepare.sh"; \
		  exit 1; }
	@echo "[✓] Intel oneAPI Fortran Compiler verified"

# Source and build directories
SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Main executable name
MAIN_PROGRAM = nerf_bigdata
MAIN_SOURCE = $(SRC_DIR)/main.f90

# Module sources (add your modules here)
MODULE_SOURCES = \
	$(SRC_DIR)/nerf_types.f90 \
	$(SRC_DIR)/nerf_utils.f90 \
	$(SRC_DIR)/nerf_mapreduce.f90 \
	$(SRC_DIR)/nerf_face_processor.f90 \
	$(SRC_DIR)/nerf_volume_renderer.f90 \
	$(SRC_DIR)/nerf_hadoop_interface.f90

# All source files
ALL_SOURCES = $(MODULE_SOURCES) $(MAIN_SOURCE)

# Object files (derived from sources)
MODULE_OBJECTS = $(MODULE_SOURCES:$(SRC_DIR)/%.f90=$(OBJ_DIR)/%.o)
MAIN_OBJECT = $(MAIN_SOURCE:$(SRC_DIR)/%.f90=$(OBJ_DIR)/%.o)
ALL_OBJECTS = $(MODULE_OBJECTS) $(MAIN_OBJECT)

###
# COMPILER FLAGS
###

# Basic compiler flags
FFLAGS = -O2 -g -warn all -traceback -check bounds
FFLAGS += -module $(OBJ_DIR) -I$(OBJ_DIR)

# Linker flags
LDFLAGS = 

# Libraries (add mathematical libraries for NeRF processing)
LIBS = -lm

# Debug flags (use: make DEBUG=1)
ifdef DEBUG
	FFLAGS += -O0 -g -debug full -check all -ftrapuv
	FFLAGS += -warn unused -warn declarations -warn interfaces
else
	FFLAGS += -O3
endif

# Parallel processing flags (for MapReduce support)
ifdef PARALLEL
	FFLAGS += -qopenmp
	LDFLAGS += -qopenmp
endif

###
# BUILD TARGETS
###

.PHONY: all clean distclean setup help test install prepare check-compiler source-intel

# Default target
all: prepare setup $(BIN_DIR)/$(MAIN_PROGRAM)

# Prepare environment (Intel oneAPI setup and compiler verification)
prepare:
	@echo "[i] Preparing Intel oneAPI environment and verifying compiler..."
	@if [ -x ./prepare.sh ]; then \
		./prepare.sh; \
	else \
		echo "[!] prepare.sh not found or not executable"; \
		echo "[!] Please run: chmod +x prepare.sh"; \
		exit 1; \
	fi
	@echo "[✓] Environment preparation completed"

# Source Intel oneAPI environment for current shell (informational)
source-intel:
	@echo "[i] To source Intel oneAPI environment in your current shell, run:"
	@echo "    source /opt/intel/oneapi/setvars.sh"
	@echo "[i] Or run the preparation script:"
	@echo "    ./prepare.sh"

# Create necessary directories
setup:
	@echo "[i] Setting up build directories..."
	@mkdir -p $(BUILD_DIR) $(OBJ_DIR) $(BIN_DIR)
	@echo "[✓] Build directories created"

# Main executable
$(BIN_DIR)/$(MAIN_PROGRAM): $(ALL_OBJECTS)
	@echo "[i] Linking main program: $(MAIN_PROGRAM)"
	bash -c '$(FC_WITH_ENV) $(LDFLAGS) -o $@ $(ALL_OBJECTS) $(LIBS)'
	@echo "[✓] Build successful: $@"

# Module compilation (modules must be compiled in dependency order)
$(OBJ_DIR)/nerf_types.o: $(SRC_DIR)/nerf_types.f90 | setup check-compiler
	@echo "[i] Compiling module: nerf_types"
	bash -c '$(FC_WITH_ENV) $(FFLAGS) -c $< -o $@'

$(OBJ_DIR)/nerf_utils.o: $(SRC_DIR)/nerf_utils.f90 $(OBJ_DIR)/nerf_types.o | setup check-compiler
	@echo "[i] Compiling module: nerf_utils"
	bash -c '$(FC_WITH_ENV) $(FFLAGS) -c $< -o $@'

$(OBJ_DIR)/nerf_mapreduce.o: $(SRC_DIR)/nerf_mapreduce.f90 $(OBJ_DIR)/nerf_types.o $(OBJ_DIR)/nerf_utils.o | setup check-compiler
	@echo "[i] Compiling module: nerf_mapreduce"
	bash -c '$(FC_WITH_ENV) $(FFLAGS) -c $< -o $@'

$(OBJ_DIR)/nerf_face_processor.o: $(SRC_DIR)/nerf_face_processor.f90 $(OBJ_DIR)/nerf_types.o $(OBJ_DIR)/nerf_utils.o | setup check-compiler
	@echo "[i] Compiling module: nerf_face_processor"
	bash -c '$(FC_WITH_ENV) $(FFLAGS) -c $< -o $@'

$(OBJ_DIR)/nerf_volume_renderer.o: $(SRC_DIR)/nerf_volume_renderer.f90 $(OBJ_DIR)/nerf_types.o $(OBJ_DIR)/nerf_utils.o | setup check-compiler
	@echo "[i] Compiling module: nerf_volume_renderer"
	bash -c '$(FC_WITH_ENV) $(FFLAGS) -c $< -o $@'

$(OBJ_DIR)/nerf_hadoop_interface.o: $(SRC_DIR)/nerf_hadoop_interface.f90 $(OBJ_DIR)/nerf_types.o $(OBJ_DIR)/nerf_mapreduce.o | setup check-compiler
	@echo "[i] Compiling module: nerf_hadoop_interface"
	bash -c '$(FC_WITH_ENV) $(FFLAGS) -c $< -o $@'

# Main program compilation
$(OBJ_DIR)/main.o: $(MAIN_SOURCE) $(MODULE_OBJECTS) | setup check-compiler
	@echo "[i] Compiling main program"
	bash -c '$(FC_WITH_ENV) $(FFLAGS) -c $< -o $@'

###
# UTILITY TARGETS
###

# Clean build artifacts
clean:
	@echo "[i] Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f *.mod *.o *.exe *.so
	@echo "[✓] Clean completed"

# Complete clean including distribution files
distclean: clean
	@echo "[i] Performing complete clean..."
	@rm -f config.log config.status
	@echo "[✓] Distribution clean completed"

# Run compiler tests
test: prepare
	@echo "[i] Running compiler tests..."
	@cd compiler_tests && bash -c '$(INTEL_SETUP) $(MAKE) all'
	@cd compiler_tests && bash -c '$(INTEL_SETUP) ./compiler_tests'
	@cd compiler_tests && $(MAKE) clean
	@echo "[✓] Compiler tests passed"

# Install to system (optional)
install: all
	@echo "[i] Installing NeRF Big Data Processor..."
	@mkdir -p /usr/local/bin
	@cp $(BIN_DIR)/$(MAIN_PROGRAM) /usr/local/bin/
	@echo "[✓] Installation completed"

# Help target
help:
	@echo "NeRF Big Data Processing - Makefile Help"
	@echo "========================================"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build the main program (default, includes prepare)"
	@echo "  prepare   - Setup Intel oneAPI environment and verify compiler"
	@echo "  source-intel - Show how to source Intel oneAPI in current shell"
	@echo "  clean     - Remove build artifacts"
	@echo "  distclean - Complete clean including config files"
	@echo "  setup     - Create build directories"
	@echo "  test      - Run compiler tests (includes prepare)"
	@echo "  install   - Install to system (/usr/local/bin)"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Build options:"
	@echo "  DEBUG=1   - Build with debug flags"
	@echo "  PARALLEL=1 - Enable OpenMP parallel processing"
	@echo ""
	@echo "Example usage:"
	@echo "  make                    # Normal build (includes prepare)"
	@echo "  make prepare           # Setup Intel oneAPI environment only"
	@echo "  make DEBUG=1           # Debug build"
	@echo "  make PARALLEL=1        # Build with OpenMP"
	@echo "  make clean all         # Clean and rebuild"
	@echo "  make test              # Run compiler verification tests"
	@echo ""
	@echo "Requirements:"
	@echo "  - Intel oneAPI Fortran Compiler (ifx)"
	@echo "  - Linux operating system"
	@echo "  - Source files in src/ directory"

###
# BUILD INFORMATION
###

# Display build configuration
info:
	@echo "Build Configuration:"
	@echo "==================="
	@echo "Compiler: $(FC)"
	@echo "Source Directory: $(SRC_DIR)"
	@echo "Build Directory: $(BUILD_DIR)"
	@echo "Object Directory: $(OBJ_DIR)"
	@echo "Binary Directory: $(BIN_DIR)"
	@echo "Main Program: $(MAIN_PROGRAM)"
	@echo "Compiler Flags: $(FFLAGS)"
	@echo "Linker Flags: $(LDFLAGS)"
	@echo "Libraries: $(LIBS)"
	@echo ""
	@echo "Source Files:"
	@for src in $(ALL_SOURCES); do echo "  $$src"; done
# NeRF Project - Windows Setup Guide

## 🪟 Windows-Specific Instructions

This project has been adapted to work on Windows with Intel oneAPI. This guide will help you get started quickly.

### 📋 Prerequisites

#### Required Software
1. **Windows 10/11** (64-bit)
2. **Intel oneAPI HPC Toolkit** (installed at `C:\Program Files (x86)\Intel\oneAPI`)
3. **PowerShell 5.1** or later (included with Windows 10/11)

#### Hardware Requirements
- **CPU**: Intel x64 processor (recommended)
- **Memory**: 8GB RAM minimum, 16GB+ recommended for large datasets
- **Storage**: 5GB free space for build and datasets

### 🚀 Quick Start

#### Option 1: Automated Setup (Recommended)
```powershell
# Run the complete setup script
.\quick_start_real_project.ps1
```

For interactive setup:
```powershell
.\quick_start_real_project.ps1 -Interactive
```

#### Option 2: Manual Setup

1. **Prepare the environment:**
   ```powershell
   .\prepare.ps1
   ```

2. **Build the project:**
   ```powershell
   # Using PowerShell (recommended)
   .\build.ps1
   
   # Or using batch file
   .\build.bat
   ```

3. **Run demonstrations:**
   ```powershell
   .\demo\run_real_demo.ps1
   ```

4. **Run benchmarks:**
   ```powershell
   .\benchmarks\run_performance_benchmark.ps1
   ```

### 🔧 Build Options

#### PowerShell Build Script (`build.ps1`)
```powershell
# Release build (default)
.\build.ps1

# Debug build
.\build.ps1 -Configuration Debug

# Clean build
.\build.ps1 -Clean

# Verbose output
.\build.ps1 -Verbose

# Include tests
.\build.ps1 -Test

# Build specific target
.\build.ps1 -Target modules  # Only compile modules
.\build.ps1 -Target all      # Build everything (default)
```

#### Batch Build Script (`build.bat`)
```cmd
# Release build (default)
build.bat

# Debug build
build.bat debug

# Clean build
build.bat clean
```

### 📊 Performance Benchmarking

Run comprehensive performance tests:
```powershell
# Default benchmark
.\benchmarks\run_performance_benchmark.ps1

# Custom batch sizes
.\benchmarks\run_performance_benchmark.ps1 -BatchSizes @(25, 100, 250)

# Custom dataset path
.\benchmarks\run_performance_benchmark.ps1 -DatasetPath "C:\MyDatasets\faces"

# Custom timeout
.\benchmarks\run_performance_benchmark.ps1 -TimeoutSeconds 120
```

### 🎬 Demonstrations

#### Interactive Demo
```powershell
.\demo\run_real_demo.ps1 -Interactive
```

#### Specific Demo Types
```powershell
# Quick face processing
.\demo\run_real_demo.ps1

# Custom batch size
.\demo\run_real_demo.ps1 -BatchSize 100

# Verbose output
.\demo\run_real_demo.ps1 -Verbose

# Custom dataset
.\demo\run_real_demo.ps1 -DatasetPath "C:\MyDatasets\custom"
```

### 📁 Project Structure

```
NeRF/
├── src/                          # Fortran source files
│   ├── main.f90                 # Main application
│   ├── nerf_types.f90           # Type definitions
│   ├── nerf_utils.f90           # Utility functions
│   ├── nerf_face_processor.f90  # Face processing
│   ├── nerf_volume_renderer.f90 # Volume rendering
│   ├── nerf_mapreduce.f90       # MapReduce implementation
│   └── nerf_hadoop_interface.f90 # Hadoop interface
├── build/                       # Build output (created by scripts)
│   ├── bin/                    # Executables
│   ├── obj/                    # Object files
│   └── lib/                    # Libraries
├── datasets/                    # Training datasets
│   ├── real_faces/             # Real face datasets
│   └── synthetic/              # Synthetic datasets
├── benchmarks/                  # Performance benchmarks
│   ├── performance/            # Benchmark results
│   └── run_performance_benchmark.ps1
├── demo/                        # Demonstration scripts
│   ├── output/                 # Demo results
│   └── run_real_demo.ps1
├── results/                     # Processing results
│   ├── 3d_models/              # Generated 3D models
│   ├── performance/            # Performance data
│   └── quality_reports/        # Quality assessments
├── prepare.ps1                 # Environment setup
├── build.ps1                   # PowerShell build script
├── build.bat                   # Batch build script
└── quick_start_real_project.ps1 # Complete setup script
```

### 🛠️ Troubleshooting

#### Intel oneAPI Not Found
```
[ERROR] Intel oneAPI not found at: C:\Program Files (x86)\Intel\oneAPI
```
**Solution:**
1. Install Intel oneAPI HPC Toolkit from [Intel's website](https://www.intel.com/content/www/us/en/developer/tools/oneapi/hpc-toolkit.html)
2. If installed elsewhere, edit the `$INTEL_ONEAPI_ROOT` variable in the scripts

#### Build Failures
```
[ERROR] Build failed: Compilation failed for nerf_types.f90
```
**Solution:**
1. Check that Intel oneAPI is properly installed
2. Verify that `ifx` compiler is available:
   ```powershell
   ifx --version
   ```
3. Try a clean build:
   ```powershell
   .\build.ps1 -Clean
   ```

#### Execution Policy Errors
```
execution of scripts is disabled on this system
```
**Solution:**
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

#### Memory Issues
```
[WARN] 8GB or more RAM is recommended for large datasets
```
**Solution:**
1. Use smaller batch sizes
2. Process datasets in chunks
3. Close other applications to free memory

### 🔍 Development Workflow

1. **Edit source files** in the `src/` directory
2. **Rebuild** using `.\build.ps1`
3. **Test changes** with `.\demo\run_real_demo.ps1`
4. **Benchmark performance** with `.\benchmarks\run_performance_benchmark.ps1`
5. **Check results** in the `results/` directory

### 📖 Additional Resources

- **Implementation Guide**: `IMPLEMENTATION_GUIDE.md`
- **Technical Documentation**: `TECHNICAL_IMPLEMENTATION.md`
- **Business Case**: `BUSINESS_CASE.md`
- **Real-World Implementation**: `docs\REAL_WORLD_IMPLEMENTATION.md`

### 🆘 Getting Help

If you encounter issues:

1. Check the build logs in the terminal output
2. Verify Intel oneAPI installation
3. Ensure all prerequisites are met
4. Try running with `-Verbose` flag for detailed output
5. Check the `benchmarks/performance/` directory for detailed logs

### 🎯 Next Steps

After successful setup:

1. **Explore the codebase** in the `src/` directory
2. **Run benchmarks** to understand performance characteristics
3. **Experiment with different datasets** in the `datasets/` directory
4. **Generate 3D models** using the demo scripts
5. **Optimize and customize** for your specific use case

---

**Note:** This project was originally developed for Linux and has been adapted for Windows. Some features may behave differently on Windows compared to the original Linux implementation.

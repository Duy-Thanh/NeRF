# build.ps1 - PowerShell build script for NeRF project

param(
    [string]$Configuration = "Release",
    [switch]$Clean
)

$INTEL_ONEAPI_ROOT = "C:\Program Files (x86)\Intel\oneAPI"
$BUILD_DIR = "build"
$BIN_DIR = "$BUILD_DIR\bin"
$OBJ_DIR = "$BUILD_DIR\obj"
$SRC_DIR = "src"

if ($Clean) {
    Write-Host "[INFO] Cleaning build directory..." -ForegroundColor Blue
    Remove-Item -Recurse -Force $BUILD_DIR -ErrorAction SilentlyContinue
}

# Ensure directories exist
New-Item -ItemType Directory -Path $BUILD_DIR -Force | Out-Null
New-Item -ItemType Directory -Path $BIN_DIR -Force | Out-Null
New-Item -ItemType Directory -Path $OBJ_DIR -Force | Out-Null

Write-Host "[INFO] Setting up Intel oneAPI environment..." -ForegroundColor Blue

# Set up Intel oneAPI environment
$env:SETVARS_COMPLETED = $null
& "$INTEL_ONEAPI_ROOT\setvars.bat" intel64

Write-Host "[INFO] Building NeRF project..." -ForegroundColor Blue

try {
    # Compile source files
    $sourceFiles = @(
        "nerf_types.f90",
        "nerf_utils.f90", 
        "nerf_face_processor.f90",
        "nerf_volume_renderer.f90",
        "nerf_mapreduce.f90",
        "nerf_hadoop_interface.f90"
    )
    
    foreach ($file in $sourceFiles) {
        Write-Host "[INFO] Compiling $file..." -ForegroundColor Yellow
        & ifx /c /O3 /QxHost /module:$OBJ_DIR "$SRC_DIR\$file" /object:"$OBJ_DIR\$([System.IO.Path]::GetFileNameWithoutExtension($file)).obj"
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to compile $file"
        }
    }
    
    # Link main executable
    Write-Host "[INFO] Linking main executable..." -ForegroundColor Yellow
    & ifx /O3 /QxHost /module:$OBJ_DIR "$SRC_DIR\main.f90" "$OBJ_DIR\*.obj" /exe:"$BIN_DIR\nerf_bigdata.exe"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to link executable"
    }
    
    Write-Host "[SUCCESS] Build completed successfully!" -ForegroundColor Green
    Write-Host "[INFO] Executable created: $BIN_DIR\nerf_bigdata.exe" -ForegroundColor Green
}
catch {
    Write-Host "[ERROR] Build failed: $_" -ForegroundColor Red
    exit 1
}

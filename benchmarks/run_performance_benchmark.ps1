#
# run_performance_benchmark.ps1 - Windows Performance Benchmark Script
# NeRF System Performance Benchmark for Windows with Intel oneAPI
#
# Copyright (C) 2025 Nguyen Duy Thanh (@Nekkochan0x0007). All right reserved
#
# This file is a part of NeRF project
#
# USAGE:
#       powershell -ExecutionPolicy Bypass -File run_performance_benchmark.ps1
# or:
#       .\run_performance_benchmark.ps1
#

param(
    [int[]]$BatchSizes = @(10, 50, 100, 500),
    [string]$DatasetPath = "datasets\real_faces\celeba",
    [string]$OutputPath = "benchmarks\performance",
    [int]$TimeoutSeconds = 60
)

# Intel oneAPI configuration
$INTEL_ONEAPI_ROOT = "C:\Program Files (x86)\Intel\oneAPI"
$SETVARS_SCRIPT = "$INTEL_ONEAPI_ROOT\setvars.bat"

# Executable path
$NERF_EXECUTABLE = "build\bin\nerf_bigdata.exe"

function Write-BenchmarkHeader {
    Write-Host ""
    Write-Host "📊 NeRF System Performance Benchmark" -ForegroundColor Cyan
    Write-Host "====================================" -ForegroundColor Cyan
    Write-Host ""
}

function Test-Prerequisites {
    Write-Host "[INFO] Checking prerequisites..." -ForegroundColor Blue
    
    # Check Intel oneAPI
    if (-not (Test-Path $SETVARS_SCRIPT)) {
        Write-Host "[ERROR] Intel oneAPI not found at: $INTEL_ONEAPI_ROOT" -ForegroundColor Red
        exit 1
    }
    
    # Check executable
    if (-not (Test-Path $NERF_EXECUTABLE)) {
        Write-Host "[ERROR] NeRF executable not found at: $NERF_EXECUTABLE" -ForegroundColor Red
        Write-Host "[INFO] Please run build.ps1 or build.bat first" -ForegroundColor Yellow
        exit 1
    }
    
    # Check dataset
    if (-not (Test-Path $DatasetPath)) {
        Write-Host "[WARN] Dataset path not found: $DatasetPath" -ForegroundColor Yellow
        Write-Host "[INFO] Benchmark will run with synthetic data" -ForegroundColor Yellow
    }
    
    Write-Host "[OK] Prerequisites check completed" -ForegroundColor Green
}

function Get-SystemInfo {
    $os = Get-WmiObject -Class Win32_OperatingSystem
    $cpu = Get-WmiObject -Class Win32_Processor | Select-Object -First 1
    $memory = Get-WmiObject -Class Win32_ComputerSystem
    
    return @{
        OS = "$($os.Caption) $($os.Version)"
        CPU = $cpu.Name
        Cores = $cpu.NumberOfCores
        Memory = "$([math]::Round($memory.TotalPhysicalMemory / 1GB, 2)) GB"
    }
}

function Get-MemoryUsage {
    $memory = Get-WmiObject -Class Win32_OperatingSystem
    $usedMemory = [math]::Round(($memory.TotalVisibleMemorySize - $memory.FreePhysicalMemory) / 1KB / 1024, 2)
    return "$usedMemory GB"
}

function Run-BenchmarkTest {
    param(
        [int]$BatchSize,
        [string]$OutputDir,
        [string]$LogFile
    )
    
    Write-Host "Testing batch size: $BatchSize" -ForegroundColor Yellow
    Add-Content -Path $LogFile -Value "Testing batch size: $BatchSize"
    
    $testOutputPath = "$OutputDir\test_output_$BatchSize"
    New-Item -ItemType Directory -Path $testOutputPath -Force | Out-Null
    
    # Setup Intel oneAPI environment and run benchmark
    $startTime = Get-Date
    
    try {
        # Create a temporary batch script to run the test with Intel oneAPI environment
        $tempScript = [System.IO.Path]::GetTempFileName() + ".bat"
        
        $batchContent = @"
@echo off
call "$SETVARS_SCRIPT" intel64 > nul 2>&1
"$NERF_EXECUTABLE" --batch-size=$BatchSize --dataset-path="$DatasetPath" --output-path="$testOutputPath" --mode=benchmark
"@
        
        $batchContent | Out-File -FilePath $tempScript -Encoding ascii
        
        # Run with timeout
        $process = Start-Process -FilePath $tempScript -WindowStyle Hidden -PassThru
        $process | Wait-Process -Timeout $TimeoutSeconds -ErrorAction SilentlyContinue
        
        if (-not $process.HasExited) {
            $process | Stop-Process -Force
            Write-Host "  [WARN] Test timed out after $TimeoutSeconds seconds" -ForegroundColor Yellow
            Add-Content -Path $LogFile -Value "  [WARN] Test timed out after $TimeoutSeconds seconds"
        }
        
        Remove-Item $tempScript -ErrorAction SilentlyContinue
    }
    catch {
        Write-Host "  [ERROR] Test failed: $_" -ForegroundColor Red
        Add-Content -Path $LogFile -Value "  [ERROR] Test failed: $_"
        return
    }
    
    $endTime = Get-Date
    $duration = ($endTime - $startTime).TotalSeconds
    $throughput = [math]::Round($BatchSize / $duration, 2)
    $memoryUsage = Get-MemoryUsage
    
    Write-Host "  Duration: $($duration)s" -ForegroundColor White
    Write-Host "  Throughput: $throughput images/second" -ForegroundColor White
    Write-Host "  Memory usage: $memoryUsage" -ForegroundColor White
    Write-Host "" -ForegroundColor White
    
    Add-Content -Path $LogFile -Value "  Duration: $($duration)s"
    Add-Content -Path $LogFile -Value "  Throughput: $throughput images/second"
    Add-Content -Path $LogFile -Value "  Memory usage: $memoryUsage"
    Add-Content -Path $LogFile -Value ""
}

function Start-PerformanceBenchmark {
    # Create output directory
    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
    
    # Create log file with timestamp
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $logFile = "$OutputPath\benchmark_$timestamp.log"
    
    Write-BenchmarkHeader
    Test-Prerequisites
    
    # Get system information
    $systemInfo = Get-SystemInfo
    
    # Write benchmark header to log
    Add-Content -Path $logFile -Value "=== NeRF Big Data Performance Benchmark ==="
    Add-Content -Path $logFile -Value "Date: $(Get-Date)"
    Add-Content -Path $logFile -Value "System: $($systemInfo.OS)"
    Add-Content -Path $logFile -Value "CPU: $($systemInfo.CPU) ($($systemInfo.Cores) cores)"
    Add-Content -Path $logFile -Value "Memory: $($systemInfo.Memory)"
    Add-Content -Path $logFile -Value "Compiler: Intel oneAPI Fortran"
    Add-Content -Path $logFile -Value "Dataset: $DatasetPath"
    Add-Content -Path $logFile -Value ""
    
    Write-Host "=== NeRF Big Data Performance Benchmark ===" -ForegroundColor Cyan
    Write-Host "Date: $(Get-Date)" -ForegroundColor White
    Write-Host "System: $($systemInfo.OS)" -ForegroundColor White
    Write-Host "CPU: $($systemInfo.CPU) ($($systemInfo.Cores) cores)" -ForegroundColor White
    Write-Host "Memory: $($systemInfo.Memory)" -ForegroundColor White
    Write-Host "Compiler: Intel oneAPI Fortran" -ForegroundColor White
    Write-Host "Dataset: $DatasetPath" -ForegroundColor White
    Write-Host ""
    
    # Run benchmark tests for different batch sizes
    foreach ($batchSize in $BatchSizes) {
        Run-BenchmarkTest -BatchSize $batchSize -OutputDir $OutputPath -LogFile $logFile
    }
    
    Add-Content -Path $logFile -Value "=== Benchmark Completed ==="
    
    Write-Host "=== Benchmark Completed ===" -ForegroundColor Green
    Write-Host ""
    Write-Host "📋 Benchmark results saved to: $logFile" -ForegroundColor Yellow
    
    # Display summary
    Write-Host "📊 Summary:" -ForegroundColor Cyan
    Write-Host "  • Tested batch sizes: $($BatchSizes -join ', ')" -ForegroundColor White
    Write-Host "  • Results directory: $OutputPath" -ForegroundColor White
    Write-Host "  • Log file: $logFile" -ForegroundColor White
    Write-Host ""
}

# Main execution
Start-PerformanceBenchmark

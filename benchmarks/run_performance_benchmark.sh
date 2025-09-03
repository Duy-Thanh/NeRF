#!/bin/bash

echo "📊 NeRF System Performance Benchmark"
echo "===================================="

benchmark_log="benchmarks/performance/benchmark_$(date +%Y%m%d_%H%M%S).log"
mkdir -p benchmarks/performance

{
    echo "=== NeRF Big Data Performance Benchmark ==="
    echo "Date: $(date)"
    echo "System: $(uname -a)"
    echo "Compiler: Intel oneAPI Fortran"
    echo "Dataset: Real face images"
    echo ""
    
    # Test different batch sizes
    for batch_size in 10 50 100 500; do
        echo "Testing batch size: $batch_size"
        start_time=$(date +%s.%N)
        
        timeout 60 bash -c '. /opt/intel/oneapi/setvars.sh >/dev/null 2>&1 && ./build/bin/nerf_bigdata' \
            --batch-size=$batch_size \
            --dataset-path=datasets/real_faces/celeba \
            --output-path=benchmarks/performance/test_output_$batch_size \
            --mode=benchmark 2>/dev/null
        
        end_time=$(date +%s.%N)
        duration=$(echo "$end_time - $start_time" | bc -l)
        throughput=$(echo "scale=2; $batch_size / $duration" | bc -l)
        
        echo "  Duration: ${duration}s"
        echo "  Throughput: ${throughput} images/second"
        echo "  Memory usage: $(free -h | grep '^Mem:' | awk '{print $3}')"
        echo ""
    done
    
    echo "=== Benchmark Completed ==="
} | tee "$benchmark_log"

echo "📋 Benchmark results saved to: $benchmark_log"

#!/bin/bash

echo "🎭 NeRF Real-World Demo - Face to 3D Avatar"
echo "=========================================="

# Check if datasets exist
if [ ! -d "datasets/real_faces/celeba" ]; then
    echo "📦 Creating realistic datasets..."
    python3 datasets/create_real_dataset.py
fi

echo ""
echo "📊 CURRENT SYSTEM STATUS:"
echo "├── Available Datasets:"
for dataset in datasets/real_faces/*/; do
    if [ -d "$dataset" ]; then
        count=$(find "$dataset" -name "*.jpg" | wc -l)
        echo "│   ├── $(basename "$dataset"): $count images"
    fi
done

echo "├── NeRF System: $([ -f "build/bin/nerf_bigdata" ] && echo "✅ Ready" || echo "❌ Missing")"
echo "└── Intel Compiler: $([ -d "/opt/intel/oneapi" ] && echo "✅ Available" || echo "❌ Missing")"

echo ""
echo "🚀 STARTING REAL-WORLD NeRF PROCESSING..."

# Run enhanced NeRF with real configuration
echo "Processing datasets with enhanced parameters..."
bash -c '. /opt/intel/oneapi/setvars.sh >/dev/null 2>&1 && ./build/bin/nerf_bigdata' \
    --config=enhanced_nerf_config.conf \
    --dataset-path=datasets/real_faces \
    --output-path=models/output \
    --mode=batch_processing \
    --enable-metrics=true

echo ""
echo "✅ Demo completed! Check models/output/ for results"

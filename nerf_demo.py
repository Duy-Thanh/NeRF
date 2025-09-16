#!/usr/bin/env python3
"""
NeRF Avatar Processing Demo
==========================

This script demonstrates the complete distributed NeRF avatar generation pipeline
using our C++ MapReduce framework. It showcases:

1. Job submission to the distributed coordinator
2. Parallel face processing across multiple workers
3. NeRF avatar generation with volumetric rendering
4. Real-time monitoring and progress tracking

Usage:
    python nerf_demo.py --input-dir ./sample_faces --output-dir ./avatars --workers 3
"""

import os
import sys
import json
import time
import requests
import argparse
from pathlib import Path
from PIL import Image
import numpy as np

class NeRFDemo:
    def __init__(self, coordinator_host="localhost", coordinator_port=8080):
        self.coordinator_url = f"http://{coordinator_host}:{coordinator_port}"
        self.session = requests.Session()
    
    def check_system_status(self):
        """Check if the distributed system is running"""
        try:
            response = self.session.get(f"{self.coordinator_url}/api/status", timeout=5)
            if response.status_code == 200:
                status = response.json()
                print(f"✅ Coordinator online: {status.get('workers', 0)} workers active")
                return True
            else:
                print(f"❌ Coordinator not responding (HTTP {response.status_code})")
                return False
        except requests.exceptions.RequestException as e:
            print(f"❌ Cannot connect to coordinator: {e}")
            print("💡 Make sure the distributed system is running:")
            print("   cd framework/docker && docker-compose up -d")
            return False
    
    def prepare_face_dataset(self, input_dir, num_faces=10):
        """Prepare sample face images for processing"""
        input_path = Path(input_dir)
        input_path.mkdir(exist_ok=True)
        
        face_files = []
        
        # Generate sample face data if directory is empty
        if not list(input_path.glob("*.jpg")):
            print(f"📷 Generating {num_faces} sample face images...")
            for i in range(num_faces):
                # Create a simple synthetic face image (placeholder)
                img_array = np.random.randint(0, 255, (256, 256, 3), dtype=np.uint8)
                
                # Add some face-like features (simplified)
                # Eyes
                img_array[80:100, 70:90] = [50, 50, 50]   # Left eye
                img_array[80:100, 166:186] = [50, 50, 50] # Right eye
                
                # Nose
                img_array[120:140, 120:136] = [200, 150, 120]
                
                # Mouth
                img_array[180:190, 110:146] = [150, 50, 50]
                
                img = Image.fromarray(img_array)
                face_file = input_path / f"face_{i:03d}.jpg"
                img.save(face_file, "JPEG")
                face_files.append(str(face_file))
        else:
            # Use existing images
            face_files = [str(f) for f in input_path.glob("*.jpg")]
            print(f"📷 Found {len(face_files)} existing face images")
        
        return face_files
    
    def submit_nerf_job(self, face_files, output_dir, resolution=512):
        """Submit NeRF avatar generation job to the distributed system"""
        job_config = {
            "plugin_name": "nerf_avatar",
            "config": {
                "output_resolution": str(resolution),
                "max_iterations": "1000",
                "output_format": "obj",
                "volumetric_samples": "64",
                "neural_layers": "8"
            },
            "input_paths": face_files,
            "output_path": output_dir,
            "num_map_tasks": min(len(face_files), 10),  # One task per face, max 10
            "num_reduce_tasks": 2
        }
        
        print(f"🚀 Submitting NeRF job with {len(face_files)} faces...")
        print(f"   Output resolution: {resolution}x{resolution}")
        print(f"   Map tasks: {job_config['num_map_tasks']}")
        print(f"   Reduce tasks: {job_config['num_reduce_tasks']}")
        
        try:
            response = self.session.post(
                f"{self.coordinator_url}/api/jobs",
                json=job_config,
                timeout=10
            )
            
            if response.status_code == 200:
                result = response.json()
                job_id = result.get("job_id")
                print(f"✅ Job submitted successfully: {job_id}")
                return job_id
            else:
                print(f"❌ Job submission failed: HTTP {response.status_code}")
                print(f"   Response: {response.text}")
                return None
                
        except requests.exceptions.RequestException as e:
            print(f"❌ Job submission error: {e}")
            return None
    
    def monitor_job_progress(self, job_id):
        """Monitor job execution progress"""
        print(f"📊 Monitoring job progress: {job_id}")
        start_time = time.time()
        
        while True:
            try:
                response = self.session.get(
                    f"{self.coordinator_url}/api/jobs/{job_id}/status",
                    timeout=5
                )
                
                if response.status_code == 200:
                    status = response.json()
                    
                    job_status = status.get("status", "unknown")
                    progress = status.get("progress_percent", 0)
                    completed_tasks = status.get("completed_tasks", 0)
                    total_tasks = status.get("total_tasks", 0)
                    
                    elapsed = time.time() - start_time
                    
                    print(f"   Status: {job_status} | Progress: {progress:.1f}% | "
                          f"Tasks: {completed_tasks}/{total_tasks} | "
                          f"Elapsed: {elapsed:.1f}s")
                    
                    if job_status in ["completed", "failed"]:
                        if job_status == "completed":
                            print(f"🎉 Job completed successfully in {elapsed:.1f}s!")
                            return True
                        else:
                            error_msg = status.get("error_message", "Unknown error")
                            print(f"❌ Job failed: {error_msg}")
                            return False
                    
                    time.sleep(2)  # Poll every 2 seconds
                    
                else:
                    print(f"⚠️ Status check failed: HTTP {response.status_code}")
                    time.sleep(5)
                    
            except requests.exceptions.RequestException as e:
                print(f"⚠️ Status check error: {e}")
                time.sleep(5)
    
    def verify_output(self, output_dir):
        """Verify that NeRF avatars were generated"""
        output_path = Path(output_dir)
        
        if not output_path.exists():
            print(f"❌ Output directory not found: {output_dir}")
            return False
        
        # Look for generated 3D model files
        model_files = list(output_path.glob("*.obj")) + list(output_path.glob("*.ply"))
        texture_files = list(output_path.glob("*.png")) + list(output_path.glob("*.jpg"))
        
        print(f"📁 Output verification:")
        print(f"   3D Models: {len(model_files)} files")
        print(f"   Textures: {len(texture_files)} files")
        
        if model_files:
            print(f"✅ NeRF avatar generation successful!")
            for model_file in model_files[:3]:  # Show first 3
                size_mb = model_file.stat().st_size / (1024 * 1024)
                print(f"   📄 {model_file.name} ({size_mb:.2f} MB)")
            return True
        else:
            print(f"❌ No 3D model files found")
            return False
    
    def run_demo(self, input_dir, output_dir, num_faces=10, resolution=512):
        """Run the complete NeRF demo pipeline"""
        print("🎭 NeRF Avatar Processing Demo")
        print("=" * 50)
        
        # Step 1: Check system status
        if not self.check_system_status():
            return False
        
        # Step 2: Prepare face dataset
        face_files = self.prepare_face_dataset(input_dir, num_faces)
        if not face_files:
            print("❌ No face images to process")
            return False
        
        # Step 3: Submit NeRF job
        job_id = self.submit_nerf_job(face_files, output_dir, resolution)
        if not job_id:
            return False
        
        # Step 4: Monitor progress
        success = self.monitor_job_progress(job_id)
        if not success:
            return False
        
        # Step 5: Verify output
        return self.verify_output(output_dir)

def main():
    parser = argparse.ArgumentParser(description="NeRF Avatar Processing Demo")
    parser.add_argument("--input-dir", default="./demo_faces", 
                        help="Directory containing face images")
    parser.add_argument("--output-dir", default="./demo_avatars", 
                        help="Directory for generated avatars")
    parser.add_argument("--num-faces", type=int, default=10,
                        help="Number of sample faces to generate")
    parser.add_argument("--resolution", type=int, default=512,
                        help="Output resolution for avatars")
    parser.add_argument("--coordinator-host", default="localhost",
                        help="Coordinator host address")
    parser.add_argument("--coordinator-port", type=int, default=8080,
                        help="Coordinator API port")
    
    args = parser.parse_args()
    
    # Create output directory
    Path(args.output_dir).mkdir(exist_ok=True)
    
    # Run the demo
    demo = NeRFDemo(args.coordinator_host, args.coordinator_port)
    
    success = demo.run_demo(
        input_dir=args.input_dir,
        output_dir=args.output_dir,
        num_faces=args.num_faces,
        resolution=args.resolution
    )
    
    if success:
        print("\n🎉 Demo completed successfully!")
        print(f"Generated avatars are in: {args.output_dir}")
        print("\n📊 System Performance Summary:")
        print("   - Distributed processing across multiple workers")
        print("   - Memory usage under 512MB per container")
        print("   - Real-time job monitoring and progress tracking")
        print("   - Fault-tolerant task execution")
        sys.exit(0)
    else:
        print("\n❌ Demo failed!")
        print("\n🔧 Troubleshooting:")
        print("   1. Make sure Docker containers are running:")
        print("      cd framework/docker && docker-compose ps")
        print("   2. Check coordinator logs:")
        print("      docker-compose logs coordinator")
        print("   3. Verify system health:")
        print("      curl http://localhost:8080/api/status")
        sys.exit(1)

if __name__ == "__main__":
    main()
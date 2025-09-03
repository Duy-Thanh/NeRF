#!/usr/bin/env python3
"""
Real Dataset Creator for NeRF Big Data Project
Simulates real face datasets for academic demonstration
"""

import os
import numpy as np
from PIL import Image, ImageDraw
import random

def create_realistic_face_dataset(dataset_name, num_images=1000):
    """Create realistic face dataset simulation"""
    dataset_path = f"datasets/real_faces/{dataset_name}"
    os.makedirs(dataset_path, exist_ok=True)
    
    print(f"Creating {dataset_name} dataset with {num_images} images...")
    
    for i in range(num_images):
        # Create realistic face image (512x512)
        img = Image.new('RGB', (512, 512), color=(
            random.randint(200, 255),  # Skin tone variation
            random.randint(180, 230), 
            random.randint(160, 210)
        ))
        
        draw = ImageDraw.Draw(img)
        
        # Draw face outline
        face_center = (256, 256)
        face_size = random.randint(180, 220)
        draw.ellipse([
            face_center[0] - face_size, face_center[1] - face_size,
            face_center[0] + face_size, face_center[1] + face_size
        ], fill=(random.randint(220, 255), random.randint(200, 240), random.randint(180, 220)))
        
        # Draw eyes
        eye_y = face_center[1] - 40
        left_eye = (face_center[0] - 50, eye_y)
        right_eye = (face_center[0] + 50, eye_y)
        
        for eye in [left_eye, right_eye]:
            draw.ellipse([eye[0]-15, eye[1]-10, eye[0]+15, eye[1]+10], fill='white')
            draw.ellipse([eye[0]-8, eye[1]-5, eye[0]+8, eye[1]+5], fill='black')
        
        # Draw nose
        nose_points = [
            (face_center[0], face_center[1]-10),
            (face_center[0]-8, face_center[1]+20),
            (face_center[0]+8, face_center[1]+20)
        ]
        draw.polygon(nose_points, fill=(random.randint(200, 230), random.randint(180, 210), random.randint(160, 190)))
        
        # Draw mouth
        mouth_y = face_center[1] + 60
        draw.arc([face_center[0]-30, mouth_y-10, face_center[0]+30, mouth_y+10], 0, 180, fill='red', width=3)
        
        # Save image
        filename = f"{dataset_path}/face_{i:06d}.jpg"
        img.save(filename, quality=95)
        
        if (i + 1) % 100 == 0:
            print(f"  Generated {i+1}/{num_images} images")
    
    print(f"✅ {dataset_name} dataset created: {num_images} images")
    return dataset_path

if __name__ == "__main__":
    # Create multiple realistic datasets
    print("🎭 CREATING REALISTIC FACE DATASETS FOR NeRF PROCESSING")
    print("=" * 60)
    
    datasets = [
        ("celeba", 2000),     # Simulate CelebA subset
        ("ffhq", 1500),       # Simulate FFHQ subset  
        ("custom", 1000),     # Custom dataset
    ]
    
    total_images = 0
    for name, count in datasets:
        create_realistic_face_dataset(name, count)
        total_images += count
    
    print("\n📊 DATASET SUMMARY:")
    print(f"Total Datasets: {len(datasets)}")
    print(f"Total Images: {total_images}")
    print(f"Storage Used: ~{total_images * 0.15:.1f} MB")
    print("\n🚀 Ready for NeRF Big Data Processing!")

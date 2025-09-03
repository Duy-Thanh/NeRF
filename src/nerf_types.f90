!
! nerf_types.f90 - Type definitions for NeRF Big Data Processing
! 
! Copyright (C) 2025 Nguyen Duy Thanh (@Nekkochan0x0007). All right reserved
! Copyright (C) 2025 NeRF Team. All right reserved
!
! FORTRAN, FORTRAN documentation and compiler references from Intel Corporation. All the references are
! Copyright (C) 2019 - 2023 Intel Corporation. All right reserved.
! Please refer to the Intel Corporation for more information.
!
! This file is a part of NeRF project
!

module nerf_types
    use iso_fortran_env, only: real64, int32, int64
    implicit none

    ! Precision definitions
    integer, parameter :: dp = real64
    integer, parameter :: sp = real64
    integer, parameter :: i4 = int32
    integer, parameter :: i8 = int64

    ! Constants
    real(dp), parameter :: PI = 3.141592653589793_dp
    real(dp), parameter :: EPSILON = 1.0e-8_dp
    
    ! Image dimensions
    integer, parameter :: MAX_IMAGE_WIDTH = 2048
    integer, parameter :: MAX_IMAGE_HEIGHT = 2048
    integer, parameter :: RGB_CHANNELS = 3
    
    ! NeRF specific constants
    integer, parameter :: MAX_RAY_SAMPLES = 128
    integer, parameter :: MAX_FACE_FEATURES = 512
    
    ! MapReduce constants
    integer, parameter :: MAX_BATCH_SIZE = 1000
    integer, parameter :: MAX_MAPPER_NODES = 64

    ! Data structures for NeRF processing
    type :: ray_t
        real(dp) :: origin(3)        ! Ray origin point
        real(dp) :: direction(3)     ! Ray direction vector
        real(dp) :: near, far        ! Near and far bounds
        integer :: sample_count      ! Number of samples along ray
    end type ray_t

    type :: face_image_t
        integer :: width, height     ! Image dimensions
        real(sp), allocatable :: pixels(:,:,:)  ! RGB pixel data
        character(len=256) :: filename          ! Source filename
        real(dp) :: camera_pose(4,4)           ! Camera transformation matrix
    end type face_image_t

    type :: face_features_t
        real(dp) :: landmarks(68,2)   ! 68 facial landmarks
        real(dp) :: geometry(MAX_FACE_FEATURES) ! 3D geometry features
        real(dp) :: texture(MAX_FACE_FEATURES)  ! Texture features
        logical :: valid              ! Feature extraction success flag
    end type face_features_t

    type :: volume_data_t
        integer :: resolution(3)      ! Volume grid resolution
        real(sp), allocatable :: density(:,:,:)    ! Volume density
        real(sp), allocatable :: color(:,:,:,:)    ! Volume color (RGBA)
        real(dp) :: bounds(2,3)       ! Volume bounding box
    end type volume_data_t

    type :: mapreduce_job_t
        character(len=256) :: job_id  ! Unique job identifier
        integer :: mapper_count       ! Number of mapper nodes
        integer :: reducer_count      ! Number of reducer nodes
        character(len=512) :: input_path   ! HDFS input path
        character(len=512) :: output_path  ! HDFS output path
        logical :: completed          ! Job completion status
    end type mapreduce_job_t

    type :: nerf_config_t
        ! Processing parameters
        integer :: image_batch_size
        integer :: ray_samples_per_pixel
        real(dp) :: learning_rate
        real(dp) :: density_threshold
        
        ! File paths
        character(len=512) :: input_dataset_path
        character(len=512) :: output_model_path
        character(len=512) :: hadoop_config_path
        
        ! Performance settings
        logical :: use_parallel_processing
        integer :: max_threads
        logical :: enable_gpu_acceleration
    end type nerf_config_t

    ! Status codes
    integer, parameter :: NERF_SUCCESS = 0
    integer, parameter :: NERF_ERROR_FILE_NOT_FOUND = -1
    integer, parameter :: NERF_ERROR_INVALID_INPUT = -2
    integer, parameter :: NERF_ERROR_MEMORY_ALLOCATION = -3
    integer, parameter :: NERF_ERROR_HADOOP_CONNECTION = -4
    integer, parameter :: NERF_ERROR_COMPILATION_FAILED = -5

end module nerf_types

!
! nerf_utils.f90 - Utility functions for NeRF Big Data Processing
! 
! Copyright (C) 2025 Nguyen Duy Thanh (@Nekkochan0x0007). All right reserved
! Copyright (C) 2025 NeRF Team. All right reserved
!
! This file is a part of NeRF project
!

module nerf_utils
    use nerf_types
    implicit none

    private
    public :: vector_normalize, vector_dot, vector_cross
    public :: matrix_multiply, matrix_inverse
    public :: read_config_file, write_log_message
    public :: allocate_face_image, deallocate_face_image
    public :: allocate_volume_data, deallocate_volume_data
    public :: generate_rays_from_camera
    public :: load_image_file, generate_synthetic_face
    public :: interpolate_along_ray
    public :: compute_face_landmarks

contains

    !> Normalize a 3D vector
    subroutine vector_normalize(vec)
        real(dp), intent(inout) :: vec(3)
        real(dp) :: magnitude
        
        magnitude = sqrt(sum(vec**2))
        if (magnitude > EPSILON) then
            vec = vec / magnitude
        end if
    end subroutine vector_normalize

    !> Compute dot product of two 3D vectors
    function vector_dot(a, b) result(dot_product)
        real(dp), intent(in) :: a(3), b(3)
        real(dp) :: dot_product
        
        dot_product = sum(a * b)
    end function vector_dot

    !> Compute cross product of two 3D vectors
    function vector_cross(a, b) result(cross_product)
        real(dp), intent(in) :: a(3), b(3)
        real(dp) :: cross_product(3)
        
        cross_product(1) = a(2) * b(3) - a(3) * b(2)
        cross_product(2) = a(3) * b(1) - a(1) * b(3)
        cross_product(3) = a(1) * b(2) - a(2) * b(1)
    end function vector_cross

    !> Multiply two 4x4 matrices
    subroutine matrix_multiply(a, b, result_matrix)
        real(dp), intent(in) :: a(4,4), b(4,4)
        real(dp), intent(out) :: result_matrix(4,4)
        integer :: i, j, k
        
        result_matrix = 0.0_dp
        do i = 1, 4
            do j = 1, 4
                do k = 1, 4
                    result_matrix(i,j) = result_matrix(i,j) + a(i,k) * b(k,j)
                end do
            end do
        end do
    end subroutine matrix_multiply

    !> Compute matrix inverse (placeholder implementation)
    subroutine matrix_inverse(matrix, inverse_matrix, status)
        real(dp), intent(in) :: matrix(4,4)
        real(dp), intent(out) :: inverse_matrix(4,4)
        integer, intent(out) :: status
        
        ! TODO: Implement proper matrix inversion
        ! For now, just return identity matrix
        inverse_matrix = 0.0_dp
        inverse_matrix(1,1) = 1.0_dp
        inverse_matrix(2,2) = 1.0_dp
        inverse_matrix(3,3) = 1.0_dp
        inverse_matrix(4,4) = 1.0_dp
        status = NERF_SUCCESS
    end subroutine matrix_inverse

    !> Read configuration from file
    subroutine read_config_file(filename, config, status)
        character(len=*), intent(in) :: filename
        type(nerf_config_t), intent(out) :: config
        integer, intent(out) :: status
        
        integer :: unit_num, ios
        character(len=512) :: line, key, value
        integer :: equals_pos
        
        ! Set default values first
        config%image_batch_size = 32
        config%ray_samples_per_pixel = 64
        config%learning_rate = 0.001_dp
        config%density_threshold = 0.1_dp
        config%use_parallel_processing = .true.
        config%max_threads = 8
        config%enable_gpu_acceleration = .false.
        config%input_dataset_path = "datasets/real_faces/"
        config%output_model_path = "results/3d_models/"
        config%hadoop_config_path = "config/hadoop/"
        
        ! Try to read configuration file
        open(newunit=unit_num, file=filename, status='old', action='read', iostat=ios)
        if (ios /= 0) then
            call write_log_message("Config file not found, using defaults")
            status = NERF_SUCCESS
            return
        end if
        
        ! Parse configuration file
        do
            read(unit_num, '(A)', iostat=ios) line
            if (ios /= 0) exit
            
            line = adjustl(line)
            if (len_trim(line) == 0 .or. line(1:1) == '#') cycle
            
            equals_pos = index(line, '=')
            if (equals_pos > 0) then
                key = trim(adjustl(line(1:equals_pos-1)))
                value = trim(adjustl(line(equals_pos+1:)))
                
                select case (trim(key))
                case ('image_batch_size')
                    read(value, *, iostat=ios) config%image_batch_size
                case ('ray_samples_per_pixel')
                    read(value, *, iostat=ios) config%ray_samples_per_pixel
                case ('learning_rate')
                    read(value, *, iostat=ios) config%learning_rate
                case ('density_threshold')
                    read(value, *, iostat=ios) config%density_threshold
                case ('max_threads')
                    read(value, *, iostat=ios) config%max_threads
                case ('input_dataset_path')
                    config%input_dataset_path = trim(value)
                case ('output_model_path')
                    config%output_model_path = trim(value)
                case ('hadoop_config_path')
                    config%hadoop_config_path = trim(value)
                case ('use_parallel_processing')
                    config%use_parallel_processing = (trim(value) == 'true')
                case ('enable_gpu_acceleration')
                    config%enable_gpu_acceleration = (trim(value) == 'true')
                end select
            end if
        end do
        
        close(unit_num)
        config%enable_gpu_acceleration = .false.
        
        status = NERF_SUCCESS
        call write_log_message("Configuration loaded successfully")
    end subroutine read_config_file

    !> Write log message
    subroutine write_log_message(message)
        character(len=*), intent(in) :: message
        
        print '(A,A)', "[LOG] ", trim(message)
    end subroutine write_log_message

    !> Load image from file (simplified implementation)
    subroutine load_image_file(filename, image, status)
        character(len=*), intent(in) :: filename
        type(face_image_t), intent(out) :: image
        integer, intent(out) :: status
        integer :: i, j
        
        ! For now, create a dummy image since we don't have image loading libraries
        image%width = 512
        image%height = 512
        allocate(image%pixels(512, 512, 3))
        
        ! Generate a simple gradient pattern
        do i = 1, 512
            do j = 1, 512
                image%pixels(i, j, 1) = real(i, sp) / 512.0_sp  ! Red gradient
                image%pixels(i, j, 2) = real(j, sp) / 512.0_sp  ! Green gradient
                image%pixels(i, j, 3) = 0.5_sp                   ! Blue constant
            end do
        end do
        
        status = NERF_SUCCESS
        call write_log_message("Loaded image: " // trim(filename))
    end subroutine load_image_file

    !> Generate synthetic face (simplified implementation)
    subroutine generate_synthetic_face(face_id, image, status)
        integer, intent(in) :: face_id
        type(face_image_t), intent(out) :: image
        integer, intent(out) :: status
        
        real(sp) :: center_x, center_y, radius, face_color(3)
        real(sp) :: distance
        integer :: i, j
        
        ! Create synthetic face with basic geometric shapes
        image%width = 512
        image%height = 512
        allocate(image%pixels(512, 512, 3))
        
        ! Set background
        image%pixels = 0.1_sp
        
        ! Create face shape (circular)
        center_x = 256.0_sp
        center_y = 256.0_sp
        radius = 200.0_sp
        
        ! Face color varies based on face_id
        face_color(1) = 0.8_sp + 0.1_sp * sin(real(face_id, sp))      ! Skin tone variation
        face_color(2) = 0.6_sp + 0.1_sp * cos(real(face_id, sp))
        face_color(3) = 0.4_sp + 0.05_sp * sin(real(face_id, sp) * 2.0_sp)
        
        do i = 1, 512
            do j = 1, 512
                distance = sqrt((real(i, sp) - center_x)**2 + (real(j, sp) - center_y)**2)
                
                if (distance <= radius) then
                    ! Inside face area
                    image%pixels(i, j, :) = face_color
                    
                    ! Add simple features
                    ! Eyes (two dark circles)
                    if ((distance <= 40.0_sp .and. abs(real(i, sp) - (center_x - 60.0_sp)) < 20.0_sp .and. &
                         abs(real(j, sp) - (center_y - 60.0_sp)) < 20.0_sp) .or. &
                        (distance <= 40.0_sp .and. abs(real(i, sp) - (center_x + 60.0_sp)) < 20.0_sp .and. &
                         abs(real(j, sp) - (center_y - 60.0_sp)) < 20.0_sp)) then
                        image%pixels(i, j, :) = 0.2_sp  ! Dark eyes
                    end if
                    
                    ! Nose (small central feature)
                    if (abs(real(i, sp) - center_x) < 15.0_sp .and. &
                        abs(real(j, sp) - center_y) < 30.0_sp .and. real(j, sp) > center_y - 10.0_sp) then
                        image%pixels(i, j, :) = face_color * 0.9_sp  ! Slightly darker nose
                    end if
                    
                    ! Mouth (horizontal line)
                    if (abs(real(i, sp) - center_x) < 40.0_sp .and. &
                        abs(real(j, sp) - (center_y + 60.0_sp)) < 5.0_sp) then
                        image%pixels(i, j, :) = 0.3_sp  ! Dark mouth
                    end if
                end if
            end do
        end do
        
        status = NERF_SUCCESS
        call write_log_message("Generated synthetic face with ID: " // trim(adjustl(char(48 + face_id))))
    end subroutine generate_synthetic_face

    !> Allocate memory for face image
    subroutine allocate_face_image(face_img, width, height, status)
        type(face_image_t), intent(inout) :: face_img
        integer, intent(in) :: width, height
        integer, intent(out) :: status
        
        face_img%width = width
        face_img%height = height
        
        allocate(face_img%pixels(width, height, RGB_CHANNELS), stat=status)
        if (status /= 0) then
            status = NERF_ERROR_MEMORY_ALLOCATION
            return
        end if
        
        face_img%pixels = 0.0_sp
        status = NERF_SUCCESS
    end subroutine allocate_face_image

    !> Deallocate face image memory
    subroutine deallocate_face_image(face_img)
        type(face_image_t), intent(inout) :: face_img
        
        if (allocated(face_img%pixels)) then
            deallocate(face_img%pixels)
        end if
    end subroutine deallocate_face_image

    !> Allocate memory for volume data
    subroutine allocate_volume_data(volume, resolution, status)
        type(volume_data_t), intent(inout) :: volume
        integer, intent(in) :: resolution(3)
        integer, intent(out) :: status
        
        volume%resolution = resolution
        
        allocate(volume%density(resolution(1), resolution(2), resolution(3)), stat=status)
        if (status /= 0) then
            status = NERF_ERROR_MEMORY_ALLOCATION
            return
        end if
        
        allocate(volume%color(resolution(1), resolution(2), resolution(3), 4), stat=status)
        if (status /= 0) then
            deallocate(volume%density)
            status = NERF_ERROR_MEMORY_ALLOCATION
            return
        end if
        
        volume%density = 0.0_sp
        volume%color = 0.0_sp
        status = NERF_SUCCESS
    end subroutine allocate_volume_data

    !> Deallocate volume data memory
    subroutine deallocate_volume_data(volume)
        type(volume_data_t), intent(inout) :: volume
        
        if (allocated(volume%density)) deallocate(volume%density)
        if (allocated(volume%color)) deallocate(volume%color)
    end subroutine deallocate_volume_data

    !> Generate rays from camera parameters
    subroutine generate_rays_from_camera(camera_pose, image_width, image_height, rays, ray_count)
        real(dp), intent(in) :: camera_pose(4,4)
        integer, intent(in) :: image_width, image_height
        type(ray_t), intent(out) :: rays(:)
        integer, intent(out) :: ray_count
        
        integer :: i, j, ray_idx
        real(dp) :: pixel_x, pixel_y, normalized_x, normalized_y
        
        ray_idx = 0
        do j = 1, image_height
            do i = 1, image_width
                ray_idx = ray_idx + 1
                if (ray_idx > size(rays)) exit
                
                ! Convert pixel coordinates to normalized device coordinates
                pixel_x = real(i - 1, dp)
                pixel_y = real(j - 1, dp)
                normalized_x = (2.0_dp * pixel_x / real(image_width, dp)) - 1.0_dp
                normalized_y = (2.0_dp * pixel_y / real(image_height, dp)) - 1.0_dp
                
                ! Set ray origin from camera position
                rays(ray_idx)%origin = camera_pose(1:3, 4)
                
                ! Set ray direction (simplified)
                rays(ray_idx)%direction = [normalized_x, normalized_y, -1.0_dp]
                call vector_normalize(rays(ray_idx)%direction)
                
                ! Set ray bounds
                rays(ray_idx)%near = 0.1_dp
                rays(ray_idx)%far = 10.0_dp
                rays(ray_idx)%sample_count = MAX_RAY_SAMPLES
            end do
        end do
        
        ray_count = min(ray_idx, size(rays))
    end subroutine generate_rays_from_camera

    !> Interpolate along ray
    function interpolate_along_ray(ray, t) result(point)
        type(ray_t), intent(in) :: ray
        real(dp), intent(in) :: t
        real(dp) :: point(3)
        
        point = ray%origin + t * ray%direction
    end function interpolate_along_ray

    !> Compute facial landmarks (placeholder)
    subroutine compute_face_landmarks(face_img, landmarks, status)
        type(face_image_t), intent(in) :: face_img
        real(dp), intent(out) :: landmarks(68,2)
        integer, intent(out) :: status
        
        ! TODO: Implement actual facial landmark detection
        ! For now, generate dummy landmarks
        landmarks = 0.0_dp
        status = NERF_SUCCESS
        call write_log_message("Facial landmarks computed (placeholder)")
    end subroutine compute_face_landmarks

end module nerf_utils

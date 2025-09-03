!
! nerf_volume_renderer.f90 - Volume rendering module for NeRF
! 
! Copyright (C) 2025 Nguyen Duy Thanh (@Nekkochan0x0007). All right reserved
! Copyright (C) 2025 NeRF Team. All right reserved
!
! This file is a part of NeRF project
!

module nerf_volume_renderer
    use nerf_types
    use nerf_utils
    implicit none

    private
    public :: render_volume, sample_along_ray
    public :: compute_alpha_compositing, integrate_ray_samples
    public :: apply_transfer_function, compute_volume_gradients

contains

    !> Render volume using ray marching
    subroutine render_volume(volume, camera_pose, rendered_image, width, height, status)
        type(volume_data_t), intent(in) :: volume
        real(dp), intent(in) :: camera_pose(4,4)
        real(sp), intent(out) :: rendered_image(:,:,:)  ! (width, height, RGB)
        integer, intent(in) :: width, height
        integer, intent(out) :: status
        
        type(ray_t) :: current_ray
        real(sp) :: pixel_color(3)
        integer :: x, y
        
        call write_log_message("Starting volume rendering...")
        
        do y = 1, height
            do x = 1, width
                ! Generate ray for current pixel
                call generate_pixel_ray(camera_pose, x, y, width, height, current_ray)
                
                ! Render along ray
                call render_ray(volume, current_ray, pixel_color, status)
                if (status /= NERF_SUCCESS) cycle
                
                ! Store pixel color
                rendered_image(x, y, :) = pixel_color
            end do
        end do
        
        status = NERF_SUCCESS
        call write_log_message("Volume rendering completed")
    end subroutine render_volume

    !> Sample volume along ray
    subroutine sample_along_ray(volume, ray, sample_points, sample_colors, sample_densities, sample_count)
        type(volume_data_t), intent(in) :: volume
        type(ray_t), intent(in) :: ray
        real(dp), intent(out) :: sample_points(:,:)      ! (sample, xyz)
        real(sp), intent(out) :: sample_colors(:,:)      ! (sample, RGB)
        real(sp), intent(out) :: sample_densities(:)     ! (sample)
        integer, intent(out) :: sample_count
        
        integer :: i
        real(dp) :: t, dt, point(3)
        
        dt = (ray%far - ray%near) / real(ray%sample_count, dp)
        sample_count = min(ray%sample_count, size(sample_points, 1))
        
        do i = 1, sample_count
            t = ray%near + real(i-1, dp) * dt
            point = interpolate_along_ray(ray, t)
            
            sample_points(i, :) = point
            call sample_volume_at_point(volume, point, sample_colors(i,:), sample_densities(i))
        end do
    end subroutine sample_along_ray

    !> Compute alpha compositing for volume rendering
    subroutine compute_alpha_compositing(colors, densities, distances, final_color, sample_count)
        real(sp), intent(in) :: colors(:,:)       ! (sample, RGB)
        real(sp), intent(in) :: densities(:)      ! (sample)
        real(dp), intent(in) :: distances(:)      ! (sample)
        real(sp), intent(out) :: final_color(3)
        integer, intent(in) :: sample_count
        
        integer :: i
        real(sp) :: alpha, transmittance, weight
        
        final_color = 0.0_sp
        transmittance = 1.0_sp
        
        do i = 1, sample_count
            if (i > size(densities) .or. i > size(colors, 1)) exit
            
            ! Compute alpha from density and distance
            alpha = 1.0_sp - exp(-densities(i) * real(distances(i), sp))
            
            ! Compute weight for this sample
            weight = alpha * transmittance
            
            ! Accumulate color
            final_color = final_color + weight * colors(i, :)
            
            ! Update transmittance
            transmittance = transmittance * (1.0_sp - alpha)
            
            ! Early termination if fully opaque
            if (transmittance < 0.001_sp) exit
        end do
    end subroutine compute_alpha_compositing

    !> Integrate ray samples for final pixel color
    subroutine integrate_ray_samples(ray, volume, final_color, status)
        type(ray_t), intent(in) :: ray
        type(volume_data_t), intent(in) :: volume
        real(sp), intent(out) :: final_color(3)
        integer, intent(out) :: status
        
        real(dp) :: sample_points(MAX_RAY_SAMPLES, 3)
        real(sp) :: sample_colors(MAX_RAY_SAMPLES, 3)
        real(sp) :: sample_densities(MAX_RAY_SAMPLES)
        real(dp) :: distances(MAX_RAY_SAMPLES)
        integer :: sample_count, i
        real(dp) :: dt
        
        ! Sample along ray
        call sample_along_ray(volume, ray, sample_points, sample_colors, sample_densities, sample_count)
        
        ! Compute distances between samples
        dt = (ray%far - ray%near) / real(sample_count, dp)
        do i = 1, sample_count
            distances(i) = dt
        end do
        
        ! Perform alpha compositing
        call compute_alpha_compositing(sample_colors, sample_densities, distances, final_color, sample_count)
        
        status = NERF_SUCCESS
    end subroutine integrate_ray_samples

    !> Apply transfer function to map density to color
    subroutine apply_transfer_function(density, base_color, output_color)
        real(sp), intent(in) :: density
        real(sp), intent(in) :: base_color(3)
        real(sp), intent(out) :: output_color(3)
        
        real(sp) :: intensity
        
        ! Simple transfer function: scale color by density
        intensity = min(max(density, 0.0_sp), 1.0_sp)
        output_color = intensity * base_color
    end subroutine apply_transfer_function

    !> Compute volume gradients for lighting
    subroutine compute_volume_gradients(volume, gradients, status)
        type(volume_data_t), intent(in) :: volume
        real(sp), intent(out) :: gradients(:,:,:,:)  ! (x, y, z, xyz)
        integer, intent(out) :: status
        
        integer :: x, y, z, nx, ny, nz
        real(sp) :: dx, dy, dz
        
        nx = volume%resolution(1)
        ny = volume%resolution(2)
        nz = volume%resolution(3)
        
        call write_log_message("Computing volume gradients...")
        
        do z = 2, nz-1
            do y = 2, ny-1
                do x = 2, nx-1
                    ! Compute gradient using central differences
                    dx = (volume%density(x+1,y,z) - volume%density(x-1,y,z)) * 0.5_sp
                    dy = (volume%density(x,y+1,z) - volume%density(x,y-1,z)) * 0.5_sp
                    dz = (volume%density(x,y,z+1) - volume%density(x,y,z-1)) * 0.5_sp
                    
                    gradients(x, y, z, 1) = dx
                    gradients(x, y, z, 2) = dy
                    gradients(x, y, z, 3) = dz
                end do
            end do
        end do
        
        status = NERF_SUCCESS
        call write_log_message("Volume gradients computation completed")
    end subroutine compute_volume_gradients

    ! Helper subroutines

    subroutine generate_pixel_ray(camera_pose, x, y, width, height, ray)
        real(dp), intent(in) :: camera_pose(4,4)
        integer, intent(in) :: x, y, width, height
        type(ray_t), intent(out) :: ray
        
        real(dp) :: ndc_x, ndc_y
        
        ! Convert pixel to normalized device coordinates
        ndc_x = (2.0_dp * real(x, dp) / real(width, dp)) - 1.0_dp
        ndc_y = (2.0_dp * real(y, dp) / real(height, dp)) - 1.0_dp
        
        ! Set ray origin from camera position
        ray%origin = camera_pose(1:3, 4)
        
        ! Set ray direction
        ray%direction = [ndc_x, ndc_y, -1.0_dp]
        call vector_normalize(ray%direction)
        
        ! Set ray bounds
        ray%near = 0.1_dp
        ray%far = 10.0_dp
        ray%sample_count = 64
    end subroutine generate_pixel_ray

    subroutine render_ray(volume, ray, pixel_color, status)
        type(volume_data_t), intent(in) :: volume
        type(ray_t), intent(in) :: ray
        real(sp), intent(out) :: pixel_color(3)
        integer, intent(out) :: status
        
        call integrate_ray_samples(ray, volume, pixel_color, status)
    end subroutine render_ray

    subroutine sample_volume_at_point(volume, point, color, density)
        type(volume_data_t), intent(in) :: volume
        real(dp), intent(in) :: point(3)
        real(sp), intent(out) :: color(3)
        real(sp), intent(out) :: density
        
        integer :: x, y, z
        
        ! Convert world coordinates to volume indices
        call world_to_volume_coords(volume, point, x, y, z)
        
        ! Check bounds
        if (x < 1 .or. x > volume%resolution(1) .or. &
            y < 1 .or. y > volume%resolution(2) .or. &
            z < 1 .or. z > volume%resolution(3)) then
            color = 0.0_sp
            density = 0.0_sp
            return
        end if
        
        ! Sample volume data
        density = volume%density(x, y, z)
        color = volume%color(x, y, z, 1:3)
    end subroutine sample_volume_at_point

    subroutine world_to_volume_coords(volume, world_point, x, y, z)
        type(volume_data_t), intent(in) :: volume
        real(dp), intent(in) :: world_point(3)
        integer, intent(out) :: x, y, z
        
        real(dp) :: normalized_point(3)
        
        ! Normalize point to [0,1] range within volume bounds
        normalized_point(1) = (world_point(1) - volume%bounds(1,1)) / &
                             (volume%bounds(2,1) - volume%bounds(1,1))
        normalized_point(2) = (world_point(2) - volume%bounds(1,2)) / &
                             (volume%bounds(2,2) - volume%bounds(1,2))
        normalized_point(3) = (world_point(3) - volume%bounds(1,3)) / &
                             (volume%bounds(2,3) - volume%bounds(1,3))
        
        ! Convert to volume indices
        x = max(1, min(volume%resolution(1), int(normalized_point(1) * volume%resolution(1)) + 1))
        y = max(1, min(volume%resolution(2), int(normalized_point(2) * volume%resolution(2)) + 1))
        z = max(1, min(volume%resolution(3), int(normalized_point(3) * volume%resolution(3)) + 1))
    end subroutine world_to_volume_coords

end module nerf_volume_renderer

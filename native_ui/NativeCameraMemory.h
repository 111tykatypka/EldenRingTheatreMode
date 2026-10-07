#pragma once
// Called only with the current native copy's pointers; nothing is cached here.
bool camera_local_read(const void*camera,float*matrix_and_parameters) noexcept;
bool camera_local_write(void*camera,const float*matrix,float fov) noexcept;

#include <windows.h>
#include <cstdint>
#include <cstring>
#include "GameProfile.h"
#include "NativeCameraMemory.h"
// Leaf SEH boundaries: no C++ objects/unwinding and no permission changes, syscalls,
// allocations, locks or VirtualQuery in the per-copy memory access.
bool camera_local_read(const void*camera,float*out) noexcept {
 if(reinterpret_cast<std::uintptr_t>(camera)<0x10000||!out)return false;
 __try {std::memcpy(out,static_cast<const unsigned char*>(camera)+TM_OFF_CAMERA_MATRIX,80);return true;}
 __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool camera_local_write(void*camera,const float*matrix,float fov) noexcept {
 if(reinterpret_cast<std::uintptr_t>(camera)<0x10000||!matrix)return false;
 __try {
  std::memcpy(static_cast<unsigned char*>(camera)+TM_OFF_CAMERA_MATRIX,matrix,64);
  *reinterpret_cast<float*>(static_cast<unsigned char*>(camera)+TM_OFF_CAMERA_FOV)=fov;
  return true;
 } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}

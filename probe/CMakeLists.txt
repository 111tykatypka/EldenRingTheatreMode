cmake_minimum_required(VERSION 3.24)
project(EldenRingCompatibilityProbe LANGUAGES CXX)
add_subdirectory(../shared shared)
add_executable(EldenRingCompatibilityProbe main.cpp)
target_compile_features(EldenRingCompatibilityProbe PRIVATE cxx_std_20)
target_compile_definitions(EldenRingCompatibilityProbe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(EldenRingCompatibilityProbe PRIVATE GameProfile)
if(MSVC)
  target_compile_options(EldenRingCompatibilityProbe PRIVATE /W4 /permissive- /utf-8)
endif()

target_include_directories(EldenRingCompatibilityProbe PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../shared)





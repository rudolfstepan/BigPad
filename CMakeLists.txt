cmake_minimum_required(VERSION 3.20)
project(BigPad LANGUAGES CXX)

add_executable(BigPad WIN32 src/main.cpp resources.rc)
target_compile_features(BigPad PRIVATE cxx_std_20)
target_compile_definitions(BigPad PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(BigPad PRIVATE comdlg32 shell32 dwmapi uxtheme comctl32 advapi32)

if(MSVC)
  target_compile_options(BigPad PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(BigPad PRIVATE -Wall -Wextra -Wpedantic)
  target_link_options(BigPad PRIVATE -municode)
endif()

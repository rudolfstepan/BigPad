cmake_minimum_required(VERSION 3.20)
project(BigPad LANGUAGES CXX)

add_executable(BigPad WIN32 src/main.cpp resources.rc)
target_compile_features(BigPad PRIVATE cxx_std_20)
target_compile_definitions(BigPad PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(BigPad PRIVATE comdlg32 shell32 dwmapi uxtheme comctl32 advapi32)

if(MSVC)
  set_property(TARGET BigPad PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(BigPad PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(BigPad PRIVATE -Wall -Wextra -Wpedantic)
  target_link_options(BigPad PRIVATE -municode)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(BigPadDocumentTests tests/document_tests.cpp)
  target_compile_features(BigPadDocumentTests PRIVATE cxx_std_20)
  target_compile_definitions(BigPadDocumentTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(BigPadDocumentTests PRIVATE comdlg32 shell32 dwmapi uxtheme comctl32 advapi32)
  add_test(NAME BigPadDocument COMMAND BigPadDocumentTests)
  add_executable(BigPadMenuTests tests/menu_tests.cpp
    src/workbench/window_frame.cpp src/workbench/workbench_menu.cpp
    src/workbench/workbench_dialogs.cpp src/workbench/workbench_graphics.cpp)
  target_compile_features(BigPadMenuTests PRIVATE cxx_std_20)
  target_compile_definitions(BigPadMenuTests PRIVATE UNICODE _UNICODE NOMINMAX _WIN32_WINNT=0x0A00)
  target_include_directories(BigPadMenuTests PRIVATE src)
  target_link_libraries(BigPadMenuTests PRIVATE comctl32 dwmapi uxtheme gdi32)
  add_test(NAME BigPadMenu COMMAND BigPadMenuTests)
endif()

# Keep the original editor available alongside the Workbench desktop edition.
add_executable(BigPadWorkbench WIN32 src/main.cpp src/workbench_chrome.cpp
  src/workbench/window_frame.cpp src/workbench/workbench_graphics.cpp
  src/workbench/workbench_dialogs.cpp src/workbench/workbench_menu.cpp resources.rc)
target_compile_features(BigPadWorkbench PRIVATE cxx_std_20)
target_compile_definitions(BigPadWorkbench PRIVATE BIGPAD_WORKBENCH UNICODE _UNICODE
  WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
target_link_libraries(BigPadWorkbench PRIVATE shell32 dwmapi uxtheme comctl32 advapi32 ole32 uuid)
if(MSVC)
  set_property(TARGET BigPadWorkbench PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(BigPadWorkbench PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(BigPadWorkbench PRIVATE -Wall -Wextra -Wpedantic)
  target_link_options(BigPadWorkbench PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

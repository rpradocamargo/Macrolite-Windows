cmake_minimum_required(VERSION 3.16)
project(Macrolite VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static runtime so the .exe runs on any Windows 11 PC without extra installs.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(macrolite_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

# ---- Platform-independent core (also unit tested) -------------------------
add_library(macrolite_core STATIC
  src/core/events.cpp
  src/core/hotkey.cpp
  src/core/repeat.cpp
  src/core/settings.cpp)
target_include_directories(macrolite_core PUBLIC src)
macrolite_warnings(macrolite_core)

# ---- Tests -----------------------------------------------------------------
enable_testing()
add_executable(macrolite_tests tests/test_main.cpp)
target_link_libraries(macrolite_tests PRIVATE macrolite_core)
macrolite_warnings(macrolite_tests)
add_test(NAME macrolite_tests COMMAND macrolite_tests)

# ---- Windows application ---------------------------------------------------
if(WIN32)
  add_executable(macrolite WIN32
    src/win/main.cpp
    src/win/main_window.cpp
    src/win/input_recorder.cpp
    src/win/input_player.cpp
    src/win/settings_store.cpp)
  target_compile_definitions(macrolite PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)
  target_link_libraries(macrolite PRIVATE macrolite_core
    user32 gdi32 comctl32 shell32 dwmapi winmm)
  if(NOT MSVC)
    target_link_options(macrolite PRIVATE -static)
  endif()
  macrolite_warnings(macrolite)
endif()

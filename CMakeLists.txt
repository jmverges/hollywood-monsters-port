cmake_minimum_required(VERSION 3.16)
project(hollywood_monsters_port C)

# The recompiled game code addresses the original 32-bit memory layout, so the build is 32-bit (x86).
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
add_compile_options(-m32 -fno-pie -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks -fno-omit-frame-pointer)
add_link_options(-m32 -no-pie)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

file(GLOB GAME_SOURCES ${CMAKE_SOURCE_DIR}/src/gen/f_*.c)
file(GLOB OVERRIDE_SOURCES ${CMAKE_SOURCE_DIR}/src/override/*.c)
file(GLOB WIN32_SOURCES ${CMAKE_SOURCE_DIR}/platform/win32/*.c)
set(HOST_SOURCES ${CMAKE_SOURCE_DIR}/platform/host/host_sdl.c ${CMAKE_SOURCE_DIR}/platform/host/host_main.c)

if(NOT GAME_SOURCES)
  message(FATAL_ERROR "src/gen is empty: run tools/recomp/gen.py first (see docs/BUILDING.md)")
endif()

# Generated game code: decompiler output, warnings are expected and not actionable here.
add_library(hm_game OBJECT ${GAME_SOURCES} ${OVERRIDE_SOURCES})
target_include_directories(hm_game PRIVATE src/include src/gen)
target_compile_options(hm_game PRIVATE -w)

# Emulated Win32/DirectX layer, built against the game's types.
add_library(hm_win32 OBJECT ${WIN32_SOURCES})
target_include_directories(hm_win32 PRIVATE src/include src/gen)
target_compile_options(hm_win32 PRIVATE -Wall -Wno-unused-parameter -Wno-int-conversion)

# Host layer (SDL2 + POSIX).
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig")
pkg_check_modules(SDL2 REQUIRED sdl2)
add_library(hm_host OBJECT ${HOST_SOURCES})
target_include_directories(hm_host PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_options(hm_host PRIVATE -Wall ${SDL2_CFLAGS_OTHER})

add_executable(hollywood-monsters $<TARGET_OBJECTS:hm_game> $<TARGET_OBJECTS:hm_win32> $<TARGET_OBJECTS:hm_host>)
target_link_libraries(hollywood-monsters PRIVATE ${SDL2_LDFLAGS} -lpthread -lm)

cmake_minimum_required(VERSION 3.16)
project(hollywood_monsters_port C)

# The recompiled game code addresses the original 32-bit memory layout, so the build is 32-bit (x86).
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
add_compile_options(-fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks -fno-omit-frame-pointer)
if(NOT WIN32)
  add_compile_options(-m32 -fno-pie)
  add_link_options(-m32 -no-pie)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

file(GLOB GAME_SOURCES ${CMAKE_SOURCE_DIR}/src/gen/f_*.c)
file(GLOB OVERRIDE_SOURCES ${CMAKE_SOURCE_DIR}/src/override/*.c)
file(GLOB WIN32_SOURCES ${CMAKE_SOURCE_DIR}/platform/win32/*.c)
set(HOST_SOURCES ${CMAKE_SOURCE_DIR}/platform/host/host_sdl.c ${CMAKE_SOURCE_DIR}/platform/host/host_main.c)
if(WIN32)
  list(APPEND HOST_SOURCES ${CMAKE_SOURCE_DIR}/platform/host/win32_reserve.c)
endif()

if(NOT GAME_SOURCES)
  message(FATAL_ERROR "src/gen is empty: run tools/recomp/gen.py first (see docs/BUILDING.md)")
endif()

# Generated game code: decompiler output, warnings are expected and not actionable here.
add_library(hm_game OBJECT ${GAME_SOURCES})
target_include_directories(hm_game PRIVATE src/include src/gen)
target_compile_options(hm_game PRIVATE -w)

# Hand-written overrides: full prototypes, warnings on.
add_library(hm_override OBJECT ${OVERRIDE_SOURCES})
target_include_directories(hm_override PRIVATE src/include src/gen)
target_compile_definitions(hm_override PRIVATE HM_OVERRIDE)
target_compile_options(hm_override PRIVATE -Wall -Wno-unused-parameter -Werror=implicit-function-declaration -Werror=incompatible-pointer-types)

# Emulated Win32/DirectX layer, built against the game's types.
add_library(hm_win32 OBJECT ${WIN32_SOURCES})
target_include_directories(hm_win32 PRIVATE src/include src/gen)
target_compile_options(hm_win32 PRIVATE -Wall -Wno-unused-parameter -Wno-int-conversion)

# Host layer (SDL2 + POSIX).
if(WIN32)
  set(SDL2_MINGW_DIR "" CACHE PATH "SDL2 MinGW development directory (…/i686-w64-mingw32)")
  set(SDL2_INCLUDE_DIRS ${SDL2_MINGW_DIR}/include/SDL2)
  set(SDL2_LDFLAGS -L${SDL2_MINGW_DIR}/lib -lSDL2)
  set(SDL2_CFLAGS_OTHER "")
else()
  find_package(PkgConfig REQUIRED)
  set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig")
  pkg_check_modules(SDL2 REQUIRED sdl2)
endif()
add_library(hm_host OBJECT ${HOST_SOURCES})
target_include_directories(hm_host PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_options(hm_host PRIVATE -Wall ${SDL2_CFLAGS_OTHER})

add_executable(hollywood-monsters $<TARGET_OBJECTS:hm_game> $<TARGET_OBJECTS:hm_override> $<TARGET_OBJECTS:hm_win32> $<TARGET_OBJECTS:hm_host>)
if(WIN32)
  # Linked at the game's original base with the image range reserved by the .hmimg section; no ASLR.
  target_link_options(hollywood-monsters PRIVATE -Wl,--image-base,0x400000 -Wl,--disable-dynamicbase
                      -Wl,-T,${CMAKE_SOURCE_DIR}/cmake/mingw-hm.ld -static-libgcc -mconsole)
  target_link_libraries(hollywood-monsters PRIVATE ${SDL2_LDFLAGS} -Wl,-Bstatic -lpthread -Wl,-Bdynamic -lm)
else()
  target_link_libraries(hollywood-monsters PRIVATE ${SDL2_LDFLAGS} -lpthread -lm -rdynamic)
endif()

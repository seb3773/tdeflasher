cmake_minimum_required(VERSION 3.5)
if(POLICY CMP0069)
  cmake_policy(SET CMP0069 NEW)
endif()
project(tde-flasher VERSION 1.0.0 LANGUAGES CXX)

option(USE_STATIC_TQT3 "Link TQt3 statically" OFF)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization and warning flags
set(OPTIMIZATION_FLAGS "-O2 -march=x86-64 -fomit-frame-pointer -Wall -Wextra -Wshadow -fstrict-aliasing -ffunction-sections -fdata-sections")
set(OPTIMIZATION_LINKER_FLAGS "-Wl,-O1 -Wl,--as-needed -Wl,--gc-sections")

set(CMAKE_CXX_FLAGS_RELEASE "${OPTIMIZATION_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${OPTIMIZATION_LINKER_FLAGS}")

# Fallback for other build types
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPTIMIZATION_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OPTIMIZATION_LINKER_FLAGS}")

add_subdirectory(core)
add_subdirectory(gui)

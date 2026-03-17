cmake_minimum_required(VERSION 3.5)
if(POLICY CMP0069)
  cmake_policy(SET CMP0069 NEW)
endif()
project(tde-flasher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Aggressive Size Optimization Flags
set(AGGRESSIVE_FLAGS "-Os -march=x86-64 -fomit-frame-pointer -Wall -Wextra -Wshadow -fstrict-aliasing -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables")
set(AGGRESSIVE_LINKER_FLAGS "-Wl,-O1 -Wl,--as-needed -Wl,--gc-sections -Wl,--strip-all -flto -Wl,--build-id=none")

set(CMAKE_CXX_FLAGS_RELEASE "${AGGRESSIVE_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${AGGRESSIVE_LINKER_FLAGS}")

# Fallback for other build types if explicitly requested to be size-optimized
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${AGGRESSIVE_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${AGGRESSIVE_LINKER_FLAGS}")

# Enable Link Time Optimization (LTO) if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)
if(lto_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

add_subdirectory(core)
add_subdirectory(gui)

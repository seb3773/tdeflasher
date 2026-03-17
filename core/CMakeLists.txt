# Core library — no GUI dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(ARCHIVE REQUIRED libarchive)
pkg_check_modules(GCRYPT REQUIRED libgcrypt)
pkg_check_modules(CURL REQUIRED libcurl)

set(CORE_SOURCES
    diskscanner.cpp
    imagereader.cpp
    blockwriter.cpp
    verifier.cpp
    flasher.cpp
)

add_library(flasher_core STATIC ${CORE_SOURCES})

target_include_directories(flasher_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ARCHIVE_INCLUDE_DIRS}
    ${GCRYPT_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(flasher_core
    ${ARCHIVE_LIBRARIES}
    ${GCRYPT_LIBRARIES}
    ${CURL_LIBRARIES}
)

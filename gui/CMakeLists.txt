# GUI — TQT3 interface
find_package(PkgConfig REQUIRED)
pkg_check_modules(TQT REQUIRED tqt)

set(TDE_PREFIX "/opt/trinity" CACHE PATH "TDE installation prefix")

include_directories(
    ${TQT_INCLUDE_DIRS}
    ${TDE_PREFIX}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)
link_directories(${TQT_LIBRARY_DIRS} ${TDE_PREFIX}/lib)
add_definitions(${TQT_CFLAGS_OTHER})

# Find MOC
find_program(TQT_MOC_EXECUTABLE NAMES tmoc moc-tqt moc REQUIRED)

set(GUI_HEADERS
    mainwindow.h
    urlinputdialog.h
)

set(GUI_SOURCES
    main.cpp
    mainwindow.cpp
    icons_data.h
    help_icon.c
    urlinputdialog.cpp
)

# MOC processing
set(MOC_SOURCES)
foreach(header ${GUI_HEADERS})
    get_filename_component(header_name ${header} NAME_WE)
    set(moc_output ${CMAKE_CURRENT_BINARY_DIR}/moc_${header_name}.cpp)
    add_custom_command(
        OUTPUT ${moc_output}
        COMMAND ${TQT_MOC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${header} -o ${moc_output}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${header}
    )
    list(APPEND MOC_SOURCES ${moc_output})
endforeach()

# Create executable
add_executable(tde-flasher ${GUI_SOURCES} ${MOC_SOURCES})

# Handle TQt3 Linking (Static or Dynamic)
if(USE_STATIC_TQT3)
    find_library(TQT_STATIC_LIB NAMES libtqt-mt.a libtqt.a PATHS ${CMAKE_SOURCE_DIR}/libs ${TQT_LIBRARY_DIRS} /usr/lib/x86_64-linux-gnu /opt/trinity/lib NO_DEFAULT_PATH)
    find_library(TQT_UI_STATIC_LIB NAMES libtqui.a PATHS ${CMAKE_SOURCE_DIR}/libs ${TQT_LIBRARY_DIRS} /usr/lib/x86_64-linux-gnu /opt/trinity/lib NO_DEFAULT_PATH)
    
    if(NOT TQT_STATIC_LIB)
        message(FATAL_ERROR "Static TQt3 library (libtqt-mt.a) not found in 'libs/' or system paths, but USE_STATIC_TQT3 is enabled.")
    endif()
    
    set(TQT_LIBRARIES_TO_LINK ${TQT_STATIC_LIB})
    if(TQT_UI_STATIC_LIB)
        list(APPEND TQT_LIBRARIES_TO_LINK ${TQT_UI_STATIC_LIB})
    endif()

    # Static TQt3 needs its dependencies linked explicitly
    list(APPEND TQT_LIBRARIES_TO_LINK X11 Xext Xrender Xft fontconfig freetype z dl GL png jpeg mng lcms2 SM ICE Xi Xcursor Xrandr Xinerama Xfixes)
    message(STATUS "Linking TQt3 STATICALLY using: ${TQT_LIBRARIES_TO_LINK}")
else()
    # Force dynamic by finding the .so specifically if possible, or just use pkg-config results
    # We use the results from pkg_check_modules(TQT REQUIRED tqt)
    set(TQT_LIBRARIES_TO_LINK ${TQT_LIBRARIES})
    message(STATUS "Linking TQt3 DYNAMICALLY using: ${TQT_LIBRARIES_TO_LINK}")
endif()

# Link: core library + TQT3
target_link_libraries(tde-flasher
    flasher_core
    ${TQT_LIBRARIES_TO_LINK}
    pthread
)

# Aggressive custom strip step
find_program(SSTRIP_EXECUTABLE NAMES sstrip)
if(SSTRIP_EXECUTABLE)
    add_custom_command(TARGET tde-flasher POST_BUILD
        COMMAND ${SSTRIP_EXECUTABLE} tde-flasher
        COMMENT "Running sstrip on tde-flasher"
    )
else()
    add_custom_command(TARGET tde-flasher POST_BUILD
        COMMAND strip --strip-all -R .comment -R .note tde-flasher
        COMMENT "Running strip on tde-flasher"
    )
endif()

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

# Link: core library + TQT3 + TDE
target_link_libraries(tde-flasher
    flasher_core
    ${TQT_LIBRARIES}
    tdecore
    tdeui
    pthread
)

# Aggressive custom strip step
add_custom_command(TARGET tde-flasher POST_BUILD
    COMMAND strip --strip-all -R .comment -R .note tde-flasher
)

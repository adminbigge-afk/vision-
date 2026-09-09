cmake_minimum_required(VERSION 3.24)
project(vision VERSION 1.3.0 LANGUAGES RC CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "vision requires Windows x64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Widgets Sql)
qt_standard_project_setup()
configure_file(cmake/Version.h.in generated/Version.h @ONLY)
configure_file(resources/windows/vision.rc.in generated/vision.rc @ONLY)

qt_add_executable(vision WIN32
    src/app/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/VideoSurface.cpp
    src/ui/VideoSurface.h
    src/player/MpvEngine.cpp
    src/player/MpvEngine.h
    src/storage/Library.cpp
    src/storage/Library.h
    "${CMAKE_CURRENT_BINARY_DIR}/generated/vision.rc"
)
target_include_directories(vision PRIVATE src .deps/mpv/include "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(vision PRIVATE Qt6::Widgets Qt6::Sql dwmapi shell32)
add_custom_command(TARGET vision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/.deps/mpv/libmpv-2.dll" "$<TARGET_FILE_DIR:vision>"
)
if(MSVC)
    target_compile_options(vision PRIVATE /W4 /utf-8 /permissive-)
endif()
install(TARGETS vision RUNTIME DESTINATION .)

qt_add_executable(vision-core-checks
    tests/CoreChecks.cpp src/player/MpvEngine.cpp src/player/MpvEngine.h
    src/storage/Library.cpp src/storage/Library.h
)
target_include_directories(vision-core-checks PRIVATE src .deps/mpv/include)
target_link_libraries(vision-core-checks PRIVATE Qt6::Core Qt6::Sql)
if(MSVC)
    target_compile_options(vision-core-checks PRIVATE /W4 /utf-8)
endif()

qt_add_executable(vision-ui-checks
    tests/UiChecks.cpp src/ui/MainWindow.cpp src/ui/MainWindow.h
    src/ui/VideoSurface.cpp src/ui/VideoSurface.h
    src/player/MpvEngine.cpp src/player/MpvEngine.h
    src/storage/Library.cpp src/storage/Library.h)
target_include_directories(vision-ui-checks PRIVATE src .deps/mpv/include "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(vision-ui-checks PRIVATE Qt6::Widgets Qt6::Sql dwmapi shell32)
if(MSVC)
    target_compile_options(vision-ui-checks PRIVATE /W4 /utf-8)
endif()
qt_add_resources(vision "vision_icons" PREFIX "/" FILES resources/windows/app-icon.png)
qt_add_resources(vision-ui-checks "vision_test_icons" PREFIX "/" FILES resources/windows/app-icon.png)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/generated/vision.rc" PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/vision.ico")

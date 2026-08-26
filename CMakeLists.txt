cmake_minimum_required(VERSION 3.16)

if (POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(ToolSizeWatcher LANGUAGES C CXX)

if (NOT APPLE AND NOT WIN32)
    message(FATAL_ERROR "Tool Size Watcher currently supports macOS and Windows")
endif()

if (APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (APPLE)
    set(CMAKE_OBJCXX_STANDARD 11)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

# A statically linked MSVC runtime keeps the packaged Windows executable self-contained.
if (MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)
find_package(OpenGL REQUIRED)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
set(IMGUI_BACKENDS_DIR "${IMGUI_DIR}/backends")

set(IMGUI_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp"
    "${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp"
)

set(PLATFORM_SOURCES)
set(PLATFORM_LIBRARIES)
set(EXECUTABLE_TYPE)

if (APPLE)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    list(APPEND PLATFORM_SOURCES src/macos_platform.mm)
    list(APPEND PLATFORM_LIBRARIES ${COCOA_FRAMEWORK})
elseif (WIN32)
    list(APPEND PLATFORM_SOURCES
        src/windows_platform.cpp
        resources/windows_resources.rc
    )
    list(APPEND PLATFORM_LIBRARIES advapi32 ole32 shell32)
    set(EXECUTABLE_TYPE WIN32)
endif()

add_executable(ToolSizeWatcher ${EXECUTABLE_TYPE}
    src/app.cpp
    src/filesystem.cpp
    src/main.cpp
    src/scanner.cpp
    ${PLATFORM_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(ToolSizeWatcher
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        "${IMGUI_DIR}"
        "${IMGUI_BACKENDS_DIR}"
)

if (APPLE)
    target_compile_definitions(ToolSizeWatcher PRIVATE GL_SILENCE_DEPRECATION)
elseif (WIN32)
    target_compile_definitions(ToolSizeWatcher PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
    target_compile_options(ToolSizeWatcher PRIVATE /W4 /permissive- /utf-8)
    # The resource script already embeds the application manifest.
    target_link_options(ToolSizeWatcher PRIVATE /MANIFEST:NO)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ToolSizeWatcher PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (TARGET OpenGL::GL)
    set(OPENGL_TARGET OpenGL::GL)
elseif (TARGET OpenGL::OpenGL)
    set(OPENGL_TARGET OpenGL::OpenGL)
else()
    message(FATAL_ERROR "Unable to find a usable OpenGL imported target")
endif()

target_link_libraries(ToolSizeWatcher
    PRIVATE
        glfw
        ${OPENGL_TARGET}
        ${PLATFORM_LIBRARIES}
)

install(TARGETS ToolSizeWatcher
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

option(TOOL_SIZE_WATCHER_BUILD_TESTS "Build Tool Size Watcher filesystem tests" OFF)
if (TOOL_SIZE_WATCHER_BUILD_TESTS)
    enable_testing()
    add_executable(ToolSizeWatcherFilesystemTests
        tests/filesystem_tests.cpp
        src/filesystem.cpp
    )
    target_include_directories(ToolSizeWatcherFilesystemTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    if (WIN32)
        target_compile_definitions(ToolSizeWatcherFilesystemTests
            PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(ToolSizeWatcherFilesystemTests PRIVATE ole32 shell32)
    endif()
    if (MSVC)
        target_compile_options(ToolSizeWatcherFilesystemTests PRIVATE /W4 /permissive- /utf-8)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(ToolSizeWatcherFilesystemTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME filesystem COMMAND ToolSizeWatcherFilesystemTests)
endif()

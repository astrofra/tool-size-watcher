cmake_minimum_required(VERSION 3.16)

project(ToolSizeWatcher LANGUAGES C CXX OBJCXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 11)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)

find_package(OpenGL REQUIRED)
find_library(COCOA_FRAMEWORK Cocoa REQUIRED)

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

add_executable(ToolSizeWatcher
    src/app.cpp
    src/filesystem.cpp
    src/macos_platform.mm
    src/main.cpp
    src/scanner.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(ToolSizeWatcher
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${IMGUI_DIR}"
        "${IMGUI_BACKENDS_DIR}"
)

target_compile_definitions(ToolSizeWatcher
    PRIVATE
        GL_SILENCE_DEPRECATION
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
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
        ${COCOA_FRAMEWORK}
)


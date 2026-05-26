cmake_minimum_required(VERSION 3.20)
project(DataScope LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# ImGui
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
)
FetchContent_MakeAvailable(imgui)

# implot
FetchContent_Declare(implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.16
)
FetchContent_MakeAvailable(implot)

set(SOURCES
    src/main.cpp
    src/parser.cpp
)

add_executable(DataScope ${SOURCES})

target_include_directories(DataScope PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
    src
)

# ImGui sources
file(GLOB IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/*.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
list(FILTER IMGUI_SOURCES EXCLUDE REGEX ".*/imgui_tables\\.cpp$")

# implot sources
set(IMPLOT_SOURCES
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)

target_sources(DataScope PRIVATE ${IMGUI_SOURCES} ${IMPLOT_SOURCES})

target_link_libraries(DataScope PRIVATE glfw)

if(WIN32)
    target_link_libraries(DataScope PRIVATE opengl32)
elseif(APPLE)
    find_library(OpenGL_LIB OpenGL REQUIRED)
    find_library(Cocoa_LIB Cocoa REQUIRED)
    find_library(IOKit_LIB IOKit REQUIRED)
    target_link_libraries(DataScope PRIVATE ${OpenGL_LIB} ${Cocoa_LIB} ${IOKit_LIB})
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(DataScope PRIVATE OpenGL::GL)
endif()

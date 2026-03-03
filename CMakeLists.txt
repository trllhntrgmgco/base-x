cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(BaseXEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Exclude Unreal plugin directory from CMake — it builds via UBT, not CMake
set(CMAKE_IGNORE_PATH ${CMAKE_SOURCE_DIR}/unreal)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BUILD_DEMOS "Build demo applications" ON)
# Option to disable building the full engine (useful to build only demos)
option(BUILD_ENGINE "Build core engine libraries" OFF)
# Probe for OpenXR first so we can default ENABLE_VR sensibly when OpenXR
# is not installed. Use QUIET to avoid hard failure during this check.
find_package(OpenXR QUIET)
if(OpenXR_FOUND)
    set(_DEFAULT_ENABLE_VR ON)
else()
    set(_DEFAULT_ENABLE_VR OFF)
endif()
option(ENABLE_VR "Enable OpenXR VR support" ${_DEFAULT_ENABLE_VR})
option(USE_VULKAN "Use Vulkan renderer" ON)

# Optionally use vcpkg toolchain for dependencies. If you have vcpkg installed
# you can pass -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using CMake toolchain: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Point CMake at the existing vcpkg-installed packages
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")

# Find packages
find_package(Vulkan REQUIRED)
find_package(glfw3 QUIET)
find_package(OpenXR QUIET)
find_package(imgui QUIET)
find_package(VulkanMemoryAllocator QUIET)
find_package(onnxruntime QUIET)
if(onnxruntime_FOUND)
    set(HAVE_ONNXRUNTIME ON)
else()
    set(HAVE_ONNXRUNTIME OFF)
endif()
find_package(Bullet QUIET)

# GLM (header-only math library)
include(FetchContent)
find_package(glm QUIET)
if(NOT glm_FOUND)
    message(STATUS "GLM not found locally; fetching from GitHub...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glm)
    set(HAVE_GLM ON)
else()
    set(HAVE_GLM ON)
endif()

# If VR is enabled, require OpenXR
if(ENABLE_VR)
    find_package(OpenXR REQUIRED)
endif()

# SQLite3
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(WARNING "SQLite3 not found; building without SQLite support.")
endif()

# Bullet3 Physics
if(Bullet_FOUND)
    message(STATUS "Bullet3 found: ${BULLET_LIBRARIES}")
    set(HAVE_BULLET3 ON)
else()
    message(WARNING "Bullet3 not found; building without Bullet3 support.")
    set(HAVE_BULLET3 OFF)
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include
)

# Add subdirectories
add_subdirectory(src/core)
add_subdirectory(src/rendering)
add_subdirectory(src/utils)
if(ENABLE_VR)
    add_subdirectory(src/vr)
endif()
if(BUILD_ENGINE)
    add_subdirectory(src/physics)

    # Create unified engine library
    add_library(BaseXEngine STATIC
        $<TARGET_OBJECTS:BaseXCore>
        $<TARGET_OBJECTS:BaseXPhysics>
        $<TARGET_OBJECTS:BaseXRendering>
        $<TARGET_OBJECTS:BaseXUtils>
    )
    if(ENABLE_VR)
        target_sources(BaseXEngine PRIVATE $<TARGET_OBJECTS:BaseXVR>)
    endif()

    target_include_directories(BaseXEngine PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include
    )

    # Link dependencies
    if(HAVE_BULLET3)
        target_link_libraries(BaseXEngine PRIVATE
            ${BULLET_LIBRARIES}
        )
    endif()
    if(HAVE_GLM AND TARGET glm::glm)
        target_link_libraries(BaseXEngine PRIVATE glm::glm)
    endif()
    if(onnxruntime_FOUND)
        target_link_libraries(BaseXEngine PRIVATE onnxruntime::onnxruntime)
    endif()
    target_link_libraries(BaseXEngine PRIVATE ${Vulkan_LIBRARIES})
endif()

if(BUILD_DEMOS)
    add_subdirectory(src/demos)
endif()

# --- Demo targets (from CMakeLists_demos.txt) ---------------------------------
# Add demo executables under demos/ structured directories
if(BUILD_DEMOS)
    # FND-001: DRO precision test
    if(EXISTS "${CMAKE_SOURCE_DIR}/demos/FND-001/FND_001_DRO_Precision.cpp")
        add_executable(FND_001_DRO_Precision demos/FND-001/FND_001_DRO_Precision.cpp)
        if(TARGET BaseXEngine)
            target_link_libraries(FND_001_DRO_Precision PRIVATE BaseXEngine)
        endif()
    endif()

    # FND-002: LOD visualisers
    if(EXISTS "${CMAKE_SOURCE_DIR}/demos/FND-002/FND_002_LOD_Visualizer.cpp")
        add_executable(FND_002_LOD_Visualizer demos/FND-002/FND_002_LOD_Visualizer.cpp)
        if(TARGET BaseXEngine)
            target_link_libraries(FND_002_LOD_Visualizer PRIVATE BaseXEngine)
        endif()
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/demos/FND-002/FND_002_LOD_Vulkan.cpp")
        add_executable(FND_002_LOD_Vulkan demos/FND-002/FND_002_LOD_Vulkan.cpp)
        if(TARGET BaseXEngine)
            target_link_libraries(FND_002_LOD_Vulkan PRIVATE BaseXEngine)
        endif()
    endif()

    # FND-003: DRO showcase
    if(EXISTS "${CMAKE_SOURCE_DIR}/demos/FND-003/FND_003_DRO_Showcase.cpp")
        add_executable(FND_003_DRO_Showcase demos/FND-003/FND_003_DRO_Showcase.cpp)
        if(TARGET BaseXEngine)
            target_link_libraries(FND_003_DRO_Showcase PRIVATE BaseXEngine)
        endif()
    endif()

    # DRO-LOD-SHOWCASE
    if(EXISTS "${CMAKE_SOURCE_DIR}/demos/DRO-LOD-SHOWCASE/DRO_LOD_BaseX_Showcase.cpp")
        add_executable(DRO_LOD_BaseX_Showcase demos/DRO-LOD-SHOWCASE/DRO_LOD_BaseX_Showcase.cpp)
        if(TARGET BaseXEngine)
            target_link_libraries(DRO_LOD_BaseX_Showcase PRIVATE BaseXEngine)
        endif()
    endif()
endif()
# ------------------------------------------------------------------------------

# Bootstrap vcpkg (if missing) and install dependencies for the project.
# Usage:
#   cmake -P scripts/install_vcpkg_deps.cmake

cmake_minimum_required(VERSION 3.15)

if(NOT DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_DIR "${CMAKE_SOURCE_DIR}/vcpkg")
else()
    set(VCPKG_DIR $ENV{VCPKG_ROOT})
endif()

if(WIN32)
    set(TRIPLET "x64-windows")
elseif(APPLE)
    set(TRIPLET "x64-osx")
else()
    set(TRIPLET "x64-linux")
endif()

message(STATUS "vcpkg dir: ${VCPKG_DIR}")
message(STATUS "vcpkg triplet: ${TRIPLET}")

if(NOT EXISTS "${VCPKG_DIR}" )
    message(STATUS "vcpkg not found; cloning into ${VCPKG_DIR}...")
    execute_process(COMMAND git clone https://github.com/microsoft/vcpkg.git "${VCPKG_DIR}"
        OUTPUT_VARIABLE _git_out ERROR_VARIABLE _git_err RESULT_VARIABLE _git_res)
    if(NOT _git_res EQUAL 0)
        message(FATAL_ERROR "Failed to clone vcpkg: ${_git_err}")
    endif()
endif()

# Bootstrap vcpkg
if(WIN32)
    set(BOOT_CMD "${VCPKG_DIR}/bootstrap-vcpkg.bat")
else()
    set(BOOT_CMD "${VCPKG_DIR}/bootstrap-vcpkg.sh")
endif()

if(NOT EXISTS "${BOOT_CMD}")
    # Fall back to using a repo-local vcpkg directory if the chosen VCPKG_DIR
    # doesn't have the bootstrap script (common when VCPKG_ROOT points to
    # Visual Studio's internal path).
    set(VCPKG_DIR "${CMAKE_SOURCE_DIR}/vcpkg")
    set(BOOT_CMD "${VCPKG_DIR}/bootstrap-vcpkg.bat")
    if(NOT EXISTS "${VCPKG_DIR}")
        message(STATUS "Falling back and cloning vcpkg into ${VCPKG_DIR}...")
        execute_process(COMMAND git clone https://github.com/microsoft/vcpkg.git "${VCPKG_DIR}"
            OUTPUT_VARIABLE _git_out ERROR_VARIABLE _git_err RESULT_VARIABLE _git_res)
        if(NOT _git_res EQUAL 0)
            message(FATAL_ERROR "Failed to clone vcpkg: ${_git_err}")
        endif()
    endif()
    if(NOT EXISTS "${BOOT_CMD}")
        message(FATAL_ERROR "vcpkg bootstrap script not found at ${BOOT_CMD}")
    endif()
endif()

message(STATUS "Bootstrapping vcpkg...")
if(WIN32)
    execute_process(COMMAND cmd /c "${BOOT_CMD}"
        WORKING_DIRECTORY "${VCPKG_DIR}"
        OUTPUT_VARIABLE _boot_out ERROR_VARIABLE _boot_err RESULT_VARIABLE _boot_res)
else()
    execute_process(COMMAND sh "${BOOT_CMD}"
        WORKING_DIRECTORY "${VCPKG_DIR}"
        OUTPUT_VARIABLE _boot_out ERROR_VARIABLE _boot_err RESULT_VARIABLE _boot_res)
endif()
if(NOT _boot_res EQUAL 0)
    message(FATAL_ERROR "Failed to bootstrap vcpkg: ${_boot_err}")
endif()

# Packages to install
# set(VCPKG_PACKAGES
#     glfw3
#     openxr-loader
#     imgui
#     vulkan-headers
#     vulkan-loader
#     vulkan-memory-allocator
#     bullet3
# )

# list(JOIN VCPKG_PACKAGES " " VCPKG_PACKAGES_STR)
# message(STATUS "Installing vcpkg packages: ${VCPKG_PACKAGES_STR}")

# execute_process(COMMAND "${VCPKG_EXE}" install ${VCPKG_PACKAGES_STR} --triplet ${TRIPLET} --vcpkg-root "${VCPKG_DIR}"
#     WORKING_DIRECTORY "${VCPKG_DIR}"
#     OUTPUT_VARIABLE _inst_out ERROR_VARIABLE _inst_err RESULT_VARIABLE _inst_res)

message(STATUS "Installing vcpkg packages from manifest")

execute_process(COMMAND "${VCPKG_EXE}" install --x-manifest-root="${CMAKE_SOURCE_DIR}" --triplet ${TRIPLET} --vcpkg-root "${VCPKG_DIR}"
    WORKING_DIRECTORY "${VCPKG_DIR}"
    OUTPUT_VARIABLE _inst_out ERROR_VARIABLE _inst_err RESULT_VARIABLE _inst_res)
if(NOT _inst_res EQUAL 0)
    message(FATAL_ERROR "vcpkg install failed: ${_inst_err}")
endif()

message(STATUS "vcpkg packages installed successfully into ${VCPKG_DIR}/installed/${TRIPLET}")
message(STATUS "To use vcpkg with CMake, re-run CMake configure with:")
message(STATUS "  -DCMAKE_TOOLCHAIN_FILE=${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake")
message(STATUS "Or set the environment variable VCPKG_ROOT to ${VCPKG_DIR} before running this script.")

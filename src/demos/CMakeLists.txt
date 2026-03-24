# Enable vcpkg manifest mode
set(VCPKG_MANIFEST_MODE ON)

include_directories(${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include/Bullet)

file(GLOB_RECURSE DEMO_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# ── Whitelist filter ──────────────────────────────────────────────────
# When RELEASE_DEMO_WHITELIST is set, keep only matching source files.
if(RELEASE_DEMO_WHITELIST)
    set(_filtered)
    foreach(_src IN LISTS DEMO_SOURCES)
        get_filename_component(_base ${_src} NAME_WE)
        if(_base IN_LIST RELEASE_DEMO_WHITELIST)
            list(APPEND _filtered ${_src})
        endif()
    endforeach()
    set(DEMO_SOURCES ${_filtered})
    message(STATUS "Release whitelist active — building: ${RELEASE_DEMO_WHITELIST}")
endif()
# ─────────────────────────────────────────────────────────────────────

# ═══════════════════════════════════════════════════════════════════
# PRODUCTION TRACK DEMOS (NEC, BNT, SPT, SIM, HWV, SBX)
# ═══════════════════════════════════════════════════════════════════

# Necrotopia VR (NEC) - Hell Campaign
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/NEC_CMakeLists.txt")
    include(NEC_CMakeLists.txt)
endif()

# TODO: Add other track demos as they're created
# if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/BNT_CMakeLists.txt")
#     include(BNT_CMakeLists.txt)  # Bible NT VR
# endif()

# ═══════════════════════════════════════════════════════════════════
# GENERIC DEMOS (Existing workflow)
# ═══════════════════════════════════════════════════════════════════

# Find GLFW for graphical demos
find_package(glfw3 REQUIRED PATHS "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows")

# If engine is not being built but demos are, we need to build minimal components
if(NOT TARGET BaseXEngine AND NOT TARGET BaseXPhysicsStatic)
    # Build minimal physics library for demos
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/physics ${CMAKE_BINARY_DIR}/src/physics)
endif()

foreach(demo_src IN LISTS DEMO_SOURCES)
    file(RELATIVE_PATH demo_rel ${CMAKE_CURRENT_SOURCE_DIR} ${demo_src})
    get_filename_component(demo_name ${demo_rel} NAME_WE)

    # Make name unique by prepending directory if not root
    get_filename_component(demo_dir ${demo_rel} DIRECTORY)
    if(demo_dir)
        string(REPLACE "/" "_" demo_dir_clean ${demo_dir})
        set(demo_name "${demo_dir_clean}_${demo_name}")
    endif()

    # Check for graphical demo naming convention: suffix _glfw
    string(REGEX MATCH "_glfw$" _is_glfw ${demo_name})
    if(_is_glfw)
        # Only add graphical demo if GLFW is available
        if(glfw3_FOUND)
            add_executable(${demo_name} ${demo_src})
            target_include_directories(${demo_name} PRIVATE "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/include")
            target_link_libraries(${demo_name} PRIVATE "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/lib/glfw3dll.lib" opengl32)
            if(TARGET BaseXEngine)
                target_link_libraries(${demo_name} PRIVATE BaseXEngine)
            elseif(TARGET BaseXPhysicsStatic)
                target_link_libraries(${demo_name} PRIVATE BaseXPhysicsStatic)
            endif()
            if(HAVE_BULLET3)
                target_link_libraries(${demo_name} PRIVATE
                    ${BULLET_LIBRARIES}
                )
            endif()
            if(HAVE_GLM AND TARGET glm::glm)
                target_link_libraries(${demo_name} PRIVATE glm::glm)
            endif()
            if(onnxruntime_FOUND)
                target_link_libraries(${demo_name} PRIVATE onnxruntime::onnxruntime)
            endif()
            target_link_libraries(${demo_name} PRIVATE opengl32)
            target_link_libraries(${demo_name} PRIVATE BaseXCore BaseXRendering BaseXUtils)
            if(TARGET BaseXVR)
                target_link_libraries(${demo_name} PRIVATE BaseXVR)
            endif()
        else()
            message(WARNING "Skipping graphical demo ${demo_name}: GLFW not found.")
        endif()
    else()
        add_executable(${demo_name} ${demo_src})
        # Link to BaseXEngine or BaseXPhysicsStatic
        if(TARGET BaseXEngine)
            target_link_libraries(${demo_name} PRIVATE BaseXEngine)
        elseif(TARGET BaseXPhysicsStatic)
            target_link_libraries(${demo_name} PRIVATE BaseXPhysicsStatic)
            target_include_directories(${demo_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
        endif()
        if(HAVE_BULLET3)
            target_link_libraries(${demo_name} PRIVATE
                ${BULLET_LIBRARIES}
            )
        endif()
        if(HAVE_GLM AND TARGET glm::glm)
            target_link_libraries(${demo_name} PRIVATE glm::glm)
        endif()
        if(onnxruntime_FOUND)
            target_link_libraries(${demo_name} PRIVATE onnxruntime::onnxruntime)
        endif()
        target_link_libraries(${demo_name} PRIVATE opengl32)
        target_link_libraries(${demo_name} PRIVATE BaseXCore BaseXRendering BaseXUtils)
        if(TARGET BaseXVR)
            target_link_libraries(${demo_name} PRIVATE BaseXVR)
        endif()
    endif()
endforeach()

if(MSVC)
    add_compile_options(/W4 /WX)  # Treat warnings as errors
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    endif()
endif()

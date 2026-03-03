add_library(BaseXPhysics OBJECT
    physics.cpp
    DRO.cpp
    BulletIntegration.cpp
)

include_directories(${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include/Bullet)

target_include_directories(BaseXPhysics PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include
)

# Link Bullet3 if available
if(HAVE_BULLET3)
    target_link_libraries(BaseXPhysics PRIVATE
        BulletDynamics
        BulletCollision
        LinearMath
    )
    target_compile_definitions(BaseXPhysics PRIVATE BASEX_HAVE_BULLET3)
endif()

# Also create a static library version for standalone demos
if(NOT TARGET BaseXEngine)
    add_library(BaseXPhysicsStatic STATIC
        physics.cpp
        DRO.cpp
        BulletIntegration.cpp
    )
    target_include_directories(BaseXPhysicsStatic PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    )

    if(HAVE_BULLET3)
        target_link_libraries(BaseXPhysicsStatic PUBLIC
            ${BULLET_LIBRARIES}
        )
        target_compile_definitions(BaseXPhysicsStatic PUBLIC BASEX_HAVE_BULLET3)
    endif()

    # Create an alias so demos can link to BaseXPhysics regardless
    add_library(BaseXPhysicsLink ALIAS BaseXPhysicsStatic)
endif()

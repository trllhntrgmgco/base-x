add_library(BaseXVR OBJECT
    vr.cpp
    VRSystem.cpp
)

target_include_directories(BaseXVR PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(TARGET OpenXR::openxr_loader)
    target_link_libraries(BaseXVR PUBLIC OpenXR::openxr_loader)
endif()

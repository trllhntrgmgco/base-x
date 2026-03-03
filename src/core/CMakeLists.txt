add_library(BaseXCore OBJECT
    core.cpp
)

target_include_directories(BaseXCore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/include
)

if(ENABLE_VR)
    target_compile_definitions(BaseXCore PUBLIC BASEX_ENABLE_VR)
endif()

if(onnxruntime_FOUND)
    target_link_libraries(BaseXCore PUBLIC
        onnxruntime::onnxruntime
    )
endif()

set_target_properties(BaseXCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(BaseXRendering OBJECT
    rendering.cpp
)

target_include_directories(BaseXRendering PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(BaseXRendering PUBLIC ${Vulkan_LIBRARIES})

if(TARGET glfw)
    target_link_libraries(BaseXRendering PUBLIC glfw)
elseif(TARGET glfw3::glfw)
    target_link_libraries(BaseXRendering PUBLIC glfw3::glfw)
elseif(TARGET glfw3)
    target_link_libraries(BaseXRendering PUBLIC glfw3)
endif()

if(TARGET imgui)
    target_link_libraries(BaseXRendering PUBLIC imgui)
endif()

if(TARGET VulkanMemoryAllocator::VulkanMemoryAllocator)
    target_link_libraries(BaseXRendering PUBLIC VulkanMemoryAllocator::VulkanMemoryAllocator)
endif()

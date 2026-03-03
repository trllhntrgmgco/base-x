add_library(BaseXUtils OBJECT
    utils.cpp
)

target_include_directories(BaseXUtils PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

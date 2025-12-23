set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    SYSTEM ${CMAKE_SOURCE_DIR}/Dependencies/Include
    ${CMAKE_SOURCE_DIR}/package
    ${CMAKE_SOURCE_DIR}/application
    ${CMAKE_SOURCE_DIR}
)

link_directories(${CMAKE_SOURCE_DIR}/Dependencies/Lib)

add_subdirectory(package)
add_subdirectory(application)

add_executable(openglCube
    main.cpp
    glad.c
)

target_link_libraries(openglCube
    PRIVATE
    glfw3.lib
    package
    application
    application
    opengl32.lib
)
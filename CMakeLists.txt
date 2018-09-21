cmake_minimum_required(VERSION 2.6)
project(ashigaru)

find_package(GLEW REQUIRED)
if (GLEW_FOUND)
    include_directories($(GLEW_INCLUDE_DIRS))
endif()
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
include_directories(${GLFW_INCLUDE_DIRS})

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})

find_package(Boost COMPONENTS program_options REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )

find_package(glm REQUIRED)
include_directories(${GLM_INCLUDE_DIRS})
link_directories(${GLM_LIBRARY_DIRS})
add_definitions(${GLM_DEFINITIONS})

find_package(Threads)

add_executable(ashigaru main.cpp util.cpp opengl_utils.cpp shader_program.cpp tiled_view.cpp)

target_link_libraries(ashigaru ${OPENGL_gl_LIBRARY})
target_link_libraries(ashigaru ${GLFW_LIBRARIES})
target_link_libraries(ashigaru ${GLEW_LIBRARIES})
target_link_libraries(ashigaru ${PNG_LIBRARY})
target_link_libraries(ashigaru ${Boost_LIBRARIES} )
target_link_libraries(ashigaru ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS ashigaru RUNTIME DESTINATION bin)

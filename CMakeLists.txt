cmake_minimum_required(VERSION 3.0.0)
project(CowsTracker VERSION 0.1.0)

add_compile_options(-Wall -Wextra -g -D_GLIBCXX_DEBUG)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.45.0 COMPONENTS iostreams system filesystem)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
	add_executable(CowsTracker main.cpp)
	target_link_libraries(CowsTracker ${Boost_LIBRARIES})
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

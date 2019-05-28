cmake_minimum_required(VERSION 3.0.0)
project(CowsTracker VERSION 0.1.0)

add_compile_options(-Wall -Wextra -g -D_GLIBCXX_DEBUG)

set(Boost_USE_STATIC_LIBS ON) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 
find_package(Boost 1.45.0 COMPONENTS iostreams system filesystem)

set(MYSQLCONNECTORCPP_ROOT_DIR
        "${MYSQLCONNECTORCPP_ROOT_DIR}"
        CACHE
        PATH
        "Where to start looking for this component.")

find_path(MYSQLCONNECTORCPP_INCLUDE_DIR
mysql_connection.h
HINTS
${MYSQLCONNECTORCPP_ROOT_DIR}
PATH_SUFFIXES
include)

find_library(MYSQLCONNECTORCPP_LIBRARY
NAMES
mysqlcppconn
mysqlcppconn-static
HINTS
${MYSQLCONNECTORCPP_ROOT_DIR}
PATH_SUFFIXES
lib64
lib)

mark_as_advanced(MYSQLCONNECTORCPP_INCLUDE_DIR MYSQLCONNECTORCPP_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MysqlConnectorCpp
        DEFAULT_MSG
        MYSQLCONNECTORCPP_INCLUDE_DIR
        MYSQLCONNECTORCPP_LIBRARY)

if(MYSQLCONNECTORCPP_FOUND)
    set(MYSQLCONNECTORCPP_INCLUDE_DIRS "${MYSQLCONNECTORCPP_INCLUDE_DIR}") # Add any dependencies here
    set(MYSQLCONNECTORCPP_LIBRARIES "${MYSQLCONNECTORCPP_LIBRARY}") # Add any dependencies here
    mark_as_advanced(MYSQLCONNECTORCPP_ROOT_DIR)
endif()

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS} /usr/local/include/igraph/) 
	add_executable(CowsTracker main.cpp)
	target_link_libraries(CowsTracker igraph ${Boost_LIBRARIES} ${MYSQLCONNECTORCPP_LIBRARY})
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

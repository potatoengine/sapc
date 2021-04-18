cmake_minimum_required(VERSION 3.16)
project(sapc VERSION 0.11 LANGUAGES CXX C)

set(SAPC_IS_ROOT_PROJECT OFF)
if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
	set(SAPC_IS_ROOT_PROJECT ON)
endif()

option(SAPC_BUILD_TESTS "Build sapc test" ${SAPC_IS_ROOT_PROJECT})
option(SAPC_USE_EMBEDDED_JSON "Try to acquire and use an embedded copy of nlohmann_json" ${SAPC_IS_ROOT_PROJECT})

if(SAPC_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(external)
add_subdirectory(source)
add_subdirectory(test)
